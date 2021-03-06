/****************************************************************************
 *
 *   Copyright (C) 2015 Mark Charlebois. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/
/**
 * @file main.cpp
 * Basic shell to execute builtin "apps"
 *
 * @author Mark Charlebois <charlebm@gmail.com>
 * @auther Roman Bapst <bapstroman@gmail.com>
 */

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <signal.h>
#include "apps.h"
#include "px4_middleware.h"
#include "DriverFramework.hpp"
#include <termios.h>

namespace px4
{
void init_once(void);
}

using namespace std;

typedef int (*px4_main_t)(int argc, char *argv[]);

#define CMD_BUFF_SIZE	100

static bool _ExitFlag = false;
extern "C" {
	void _SigIntHandler(int sig_num);
	void _SigIntHandler(int sig_num)
	{
		cout.flush();
		cout << endl << "Exiting.." << endl;
		cout.flush();
		_exit(0);
	}
	void _SigFpeHandler(int sig_num);
	void _SigFpeHandler(int sig_num)
	{
		cout.flush();
		cout << endl << "floating point exception" << endl;
		PX4_BACKTRACE();
		cout.flush();
	}
}

static void print_prompt()
{
	cout.flush();
	cout << "pxh> ";
	cout.flush();
}

static void run_cmd(const vector<string> &appargs, bool exit_on_fail)
{
	// command is appargs[0]
	string command = appargs[0];

	if (apps.find(command) != apps.end()) {
		const char *arg[appargs.size() + 2];

		unsigned int i = 0;

		while (i < appargs.size() && appargs[i] != "") {
			arg[i] = (char *)appargs[i].c_str();
			++i;
		}

		arg[i] = (char *)0;

		if (exit_on_fail) {
			cout << endl;
		}

		int retval = apps[command](i, (char **)arg);

		if (exit_on_fail && retval) {
			exit(retval);
		}

	} else if (command.compare("help") == 0) {
		list_builtins();

	} else if (command.length() == 0) {
		// Do nothing

	} else {
		cout << endl << "Invalid command: " << command << "\ntype 'help' for a list of commands" << endl;

	}

	if (exit_on_fail) {
		print_prompt();
	}
}

static void usage()
{

	cout << "./mainapp [-d] [startup_config] -h" << std::endl;
	cout << "   -d            - Optional flag to run the app in daemon mode and does not take listen for user input." <<
	     std::endl;
	cout << "                   This is needed if mainapp is intended to be run as a upstart job on linux" << std::endl;
	cout << "<startup_config> - config file for starting/stopping px4 modules" << std::endl;
	cout << "   -h            - help/usage information" << std::endl;
}

static void process_line(string &line, bool exit_on_fail)
{
	if (line.length() == 0) {
		printf("\n");
	}

	vector<string> appargs(10);

	stringstream(line) >> appargs[0] >> appargs[1] >> appargs[2] >> appargs[3] >> appargs[4] >> appargs[5] >> appargs[6] >>
			   appargs[7] >> appargs[8] >> appargs[9];
	run_cmd(appargs, exit_on_fail);
}

int main(int argc, char **argv)
{
	bool daemon_mode = false;
	signal(SIGINT, _SigIntHandler);
	signal(SIGFPE, _SigFpeHandler);

	int index = 1;
	bool error_detected = false;
	char *commands_file = nullptr;

	while (index < argc) {
		if (argv[index][0] == '-') {
			// the arg starts with -
			if (strcmp(argv[index], "-d") == 0) {
				daemon_mode = true;

			} else if (strcmp(argv[index], "-h") == 0) {
				usage();
				return 0;

			} else {
				PX4_WARN("Unknown/unhandled parameter: %s", argv[index]);
				return 1;
			}

		} else {
			// this is an argument that does not have '-' prefix; treat it like a file name
			ifstream infile(argv[index]);

			if (infile.good()) {
				infile.close();
				commands_file = argv[index];

			} else {
				PX4_WARN("Error opening file: %s", argv[index]);
				error_detected = true;
				break;
			}
		}

		++index;
	}

	if (!error_detected) {
		DriverFramework::Framework::initialize();
		px4::init_once();

		px4::init(argc, argv, "mainapp");

		//if commandfile is present, process the commands from the file
		if (commands_file != nullptr) {
			ifstream infile(commands_file);

			if (infile.is_open()) {
				for (string line; getline(infile, line, '\n');) {
					process_line(line, false);
				}

			} else {
				PX4_WARN("Error opening file: %s", commands_file);
			}
		}

		if (!daemon_mode) {
			string mystr = "";
			string string_buffer[CMD_BUFF_SIZE];
			int buf_ptr_write = 0;
			int buf_ptr_read = 0;

			print_prompt();

			// change input mode so that we can manage shell
			struct termios term;
			tcgetattr(0, &term);
			term.c_lflag &= ~ICANON;
			term.c_lflag &= ~ECHO;
			tcsetattr(0, TCSANOW, &term);
			setbuf(stdin, NULL);

			while (!_ExitFlag) {

				char c = getchar();

				switch (c) {
				case 127:	// backslash
					if (mystr.length() > 0) {
						mystr.pop_back();
						printf("%c[2K", 27);	// clear line
						cout << (char)13;
						print_prompt();
						cout << mystr;
					}

					break;

				case'\n':	// user hit enter
					if (buf_ptr_write == CMD_BUFF_SIZE) {
						buf_ptr_write = 0;
					}

					if (buf_ptr_write > 0) {
						if (mystr != string_buffer[buf_ptr_write - 1]) {
							string_buffer[buf_ptr_write] = mystr;
							buf_ptr_write++;
						}

					} else {
						if (mystr != string_buffer[CMD_BUFF_SIZE - 1]) {
							string_buffer[buf_ptr_write] = mystr;
							buf_ptr_write++;
						}
					}

					process_line(mystr, !daemon_mode);
					mystr = "";
					buf_ptr_read = buf_ptr_write;
					break;

				case '\033': {	// arrow keys
						c = getchar();	// skip first one, does not have the info
						c = getchar();

						// arrow up
						if (c == 'A') {
							buf_ptr_read--;
							// arrow down

						} else if (c == 'B') {
							if (buf_ptr_read < buf_ptr_write) {
								buf_ptr_read++;
							}

						} else {
							// TODO: Support editing current line
						}

						if (buf_ptr_read < 0) {
							buf_ptr_read = 0;
						}

						string saved_cmd = string_buffer[buf_ptr_read];
						printf("%c[2K", 27);
						cout << (char)13;
						mystr = saved_cmd;
						print_prompt();
						cout << mystr;
						break;
					}

				default:	// any other input
					cout << c;
					mystr += c;
					break;
				}
			}

		} else {
			while (!_ExitFlag) {
				usleep(100000);
			}
		}

		if (px4_task_is_running("muorb")) {
			// sending muorb stop is needed if it is running to exit cleanly
			vector<string> muorb_stop_cmd = { "muorb", "stop" };
			run_cmd(muorb_stop_cmd, !daemon_mode);
		}

		vector<string> shutdown_cmd = { "shutdown" };
		run_cmd(shutdown_cmd, true);
		DriverFramework::Framework::shutdown();
	}
}
