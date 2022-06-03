// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/cli_util.h"

#include <string.h>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"

namespace {

void PrintOption(int option_number, const std::string& option_name) {
  printf("  %d. %s\n", option_number, option_name.c_str());
}

}  // namespace

namespace remoting {
namespace test {

CommandOption::CommandOption() = default;
CommandOption::CommandOption(const CommandOption&) = default;
CommandOption::CommandOption(CommandOption&&) = default;
CommandOption::CommandOption(
    const std::string name,
    const base::RepeatingCallback<void(base::OnceClosure on_done)>& command)
    : name(name), command(command) {}
CommandOption::~CommandOption() = default;
CommandOption& CommandOption::operator=(const CommandOption&) = default;
CommandOption& CommandOption::operator=(CommandOption&&) = default;

void RunCommandOptionsLoop(const std::vector<CommandOption>& options) {
  DCHECK_LT(0u, options.size());
  while (true) {
    base::RunLoop run_loop;

    printf("\nOptions:\n");
    int print_option_number = 1;
    for (const auto& option : options) {
      PrintOption(print_option_number, option.name);
      print_option_number++;
    }
    int quit_option_number = print_option_number;
    PrintOption(quit_option_number, "Quit");

    printf("\nYour choice [number]: ");
    int choice = 0;
    base::StringToInt(test::ReadString(), &choice);
    if (choice < 1 || choice > quit_option_number) {
      fprintf(stderr, "Unknown option\n");
      continue;
    }
    if (choice == quit_option_number) {
      return;
    }
    auto& command = options[choice - 1].command;
    command.Run(run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }
}

std::string ReadString() {
  const int kMaxLen = 1024;
  std::string str(kMaxLen, 0);
  char* result = fgets(&str[0], kMaxLen, stdin);
  if (!result)
    return std::string();
  size_t newline_index = str.find('\n');
  if (newline_index != std::string::npos)
    str[newline_index] = '\0';
  str.resize(strlen(&str[0]));
  return str;
}

bool ReadYNBool(bool default_value) {
  std::string result = test::ReadString();
  return result == "y" || result == "Y" || (default_value && result.empty());
}

std::string ReadStringFromCommandLineOrStdin(const std::string& switch_name,
                                             const std::string& read_prompt) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switch_name)) {
    return command_line->GetSwitchValueASCII(switch_name);
  }
  printf("%s", read_prompt.c_str());
  return ReadString();
}

void WaitForEnterKey(base::OnceClosure on_done) {
  base::ThreadPool::PostTaskAndReply(FROM_HERE, {base::MayBlock()},
                                     base::BindOnce([]() { getchar(); }),
                                     std::move(on_done));
}

}  // namespace test
}  // namespace remoting
