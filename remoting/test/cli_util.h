// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_CLI_UTIL_H_
#define REMOTING_TEST_CLI_UTIL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"

namespace remoting {
namespace test {

struct CommandOption {
  CommandOption();
  CommandOption(const CommandOption&);
  CommandOption(CommandOption&&);
  CommandOption(
      const std::string name,
      const base::RepeatingCallback<void(base::OnceClosure on_done)>& command);
  ~CommandOption();

  CommandOption& operator=(const CommandOption&);
  CommandOption& operator=(CommandOption&&);

  std::string name;
  base::RepeatingCallback<void(base::OnceClosure on_done)> command;
};

// Shows a menu and keeps asking user to choose a command option. This will
// block until the user chooses "Quit" from the menu (which is added in addition
// to |options|).
void RunCommandOptionsLoop(const std::vector<CommandOption>& options);

// Reads a newline-terminated string from stdin.
std::string ReadString();

// Reads a newline-terminated string from stdin and returns true iff the string
// is `Y` or `y`, or the string is empty and |default_value| is true.
bool ReadYNBool(bool default_value = false);

// Read the value of |switch_name| from command line if it exists, otherwise
// read from stdin.
std::string ReadStringFromCommandLineOrStdin(const std::string& switch_name,
                                             const std::string& read_prompt);

// Wait for the user to press enter key on an anonymous sequence and calls
// |on_done| on current sequence once it is done.
void WaitForEnterKey(base::OnceClosure on_done);

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_TEST_CLI_UTIL_H_
