// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/start_host_as_root.h"

#include <pwd.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/check.h"
#include "base/command_line.h"
#include "base/process/launch.h"

namespace remoting {

int StartHostAsRoot(int argc, char** argv) {
  DCHECK(getuid() == 0);

  base::CommandLine command_line(argc, argv);
  std::string user_name;
  if (command_line.HasSwitch("corp-user")) {
    std::string corp_user_email = command_line.GetSwitchValueASCII("corp-user");
    size_t at_symbol_pos = corp_user_email.find("@");
    if (at_symbol_pos != std::string::npos) {
      user_name = corp_user_email.substr(0, at_symbol_pos);
    }
  } else if (command_line.HasSwitch("user_name")) {
    user_name = command_line.GetSwitchValueASCII("user-name");
  }
  if (user_name.empty()) {
    fprintf(stderr,
            "Must specify the --user-name or --corp-user option when running "
            "as root.\n");
    return 1;
  }

  int return_value = 1;
  command_line.RemoveSwitch("user-name");
  command_line.AppendSwitch("no-start");
  std::vector<std::string> create_config_command_line{
      "/usr/bin/sudo",
      "-u",
      user_name.c_str(),
  };
  create_config_command_line.insert(create_config_command_line.end(),
                                    command_line.argv().begin(),
                                    command_line.argv().end());
  // LaunchProcess redirects stdin to /dev/null, but start_host prompts for a
  // PIN if one isn't specified on the command-line, so dup and remap it.
  base::LaunchOptions options;
  int stdin_dup = dup(STDIN_FILENO);
  options.fds_to_remap.emplace_back(stdin_dup, STDIN_FILENO);
  auto create_config_process =
      base::LaunchProcess(create_config_command_line, options);
  close(stdin_dup);
  if (!create_config_process.WaitForExit(&return_value) || return_value != 0) {
    fprintf(stderr, "Failed to set new config.\n");
    return return_value;
  }

  return_value = 1;
  std::vector<std::string> systemctl_command_line{
      "systemctl", "enable", "--now",
      std::string("chrome-remote-desktop@") + user_name};
  auto systemctl_process =
      base::LaunchProcess(systemctl_command_line, base::LaunchOptions());
  if (!systemctl_process.WaitForExit(&return_value) || return_value != 0) {
    fprintf(stderr, "Failed to enable host service.\n");
    return return_value;
  }

  printf("Host service started successfully.\n");
  return 0;
}

}  // namespace remoting
