// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/start_host_as_root.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_types.h"

namespace remoting {

namespace {

constexpr std::string_view kUserNameSwitch = "user-name";
constexpr std::string_view kCorpUserSwitch = "corp-user";
constexpr std::string_view kCloudUserSwitch = "cloud-user";
constexpr std::string_view kHostTypeSwitch = "host-type";

bool ValidateCommandLine(const base::CommandLine& command_line) {
  bool has_username = command_line.HasSwitch(kUserNameSwitch);
  bool has_corp_user = command_line.HasSwitch(kCorpUserSwitch);
  bool has_cloud_user = command_line.HasSwitch(kCloudUserSwitch);

  if (!has_username && !has_corp_user && !has_cloud_user) {
    std::cerr << "At least one of the following args must be provided:\n"
              << "  --user-name=<username>\n"
              << "  --corp-user=<username>\n"
              << "  --cloud-user=<email>\n";
    return false;
  } else if (has_corp_user && has_cloud_user) {
    std::cerr << "'--corp-user' and '--cloud-user' flags cannot be used "
                 "together.\n";
    return false;
  } else if (has_cloud_user) {
    std::string arg_value = command_line.GetSwitchValueASCII(kCloudUserSwitch);
    if (!base::SplitStringOnce(arg_value, '@')) {
      std::cerr << "The --cloud-user flag requires an email address.\n";
      return false;
    }
  }

  return true;
}

}  // namespace

std::string GetLocalUserNameFromCommandLine(
    const base::CommandLine& command_line) {
  // The 'user-name' flag contains the local account to run the host as.
  // If '--user-name' is not provided, then we will try to extract it from
  // either '--cloud-user' or '--corp-user'.
  std::string user_name;
  if (command_line.HasSwitch(kUserNameSwitch)) {
    user_name = command_line.GetSwitchValueASCII(kUserNameSwitch);
  } else if (command_line.HasSwitch(kCorpUserSwitch)) {
    // For compat reasons, we support either email or username for this param.
    // TODO: joedow - Remove support for the email param in M139.
    std::string arg_value = command_line.GetSwitchValueASCII(kCorpUserSwitch);
    auto parts = base::SplitStringOnce(arg_value, '@');
    if (!parts) {
      user_name = std::move(arg_value);
    } else {
      user_name = std::string(parts->first);
    }
  } else if (command_line.HasSwitch(kCloudUserSwitch)) {
    std::string arg_value = command_line.GetSwitchValueASCII(kCloudUserSwitch);
    // ValidateCommandLine() rejects a `--cloud-user` that is not an email
    // address.
    auto parts = base::SplitStringOnce(arg_value, '@');
    if (parts) {
      user_name = std::string(parts->first);
    }
  }

  return user_name;
}

int StartHostAsRoot(const HostType& host_type) {
  CHECK_EQ(getuid(), 0u);
  CHECK(!host_type.is_multi_process());

  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (!ValidateCommandLine(command_line)) {
    return 1;
  }
  std::string user_name = GetLocalUserNameFromCommandLine(command_line);
  if (user_name.empty()) {
    std::cerr << "A username and/or email must be provided from one of the "
                 "following switches:\n"
              << "  --user-name=<username>\n"
              << "  --corp-user=<username>\n"
              << "  --cloud-user=<email>\n";
    return 1;
  }

  errno = 0;
  const passwd* user_struct = getpwnam(user_name.c_str());
  if (!user_struct) {
    std::cerr << "Failed to retrieve passwd struct for " << user_name
              << ". errno = " << strerror(errno) << "(" << errno << ")\n"
              << "Does this user account exist on the machine?\n";
    return -1;
  }

  std::cout << "Configuring the host service to run as local account: "
            << user_name << "\n";

  std::string home_dir = user_struct->pw_dir ?: "";
  base::FilePath home_dir_path = base::FilePath(home_dir);
  if (!base::DirectoryExists(home_dir_path)) {
    std::cerr << "[WARNING] Can't find home directory (" << home_dir << ") for "
              << user_name << "(" << user_struct->pw_uid << ").\n"
              << "Please run the 'mkhomedir_helper' utility, or similar, to "
                 "create a home directory for the user.\n"
              << "The host setup process will not complete successfully "
                 "without one.\n";
  } else {
    std::cout << "Verified that home directory (" << home_dir << ") exists for "
              << user_name << "(" << user_struct->pw_uid << ")\n";
  }

  int return_value = 1;
  command_line.RemoveSwitch(kUserNameSwitch);
  command_line.AppendSwitch("no-start");
  // The child only registers the host as the user, and must use the host type
  // chosen here rather than the configured one.
  command_line.RemoveSwitch(kHostTypeSwitch);
  command_line.AppendSwitchASCII(kHostTypeSwitch, host_type.name());
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
  options.allow_new_privs = true;
  int stdin_dup = dup(STDIN_FILENO);
  options.fds_to_remap.emplace_back(stdin_dup, STDIN_FILENO);
  auto create_config_process =
      base::LaunchProcess(create_config_command_line, options);
  close(stdin_dup);
  if (!create_config_process.WaitForExit(&return_value) || return_value != 0) {
    std::cerr << "Failed to set new config.\n";
    return return_value;
  }

  // `is_corp_host` is only used to set up multi-process host types.
  std::unique_ptr<HostTypeEnvironment> env = HostTypeEnvironment::Create();
  HostTypeContext context(*env, user_name, /*is_corp_host=*/false);
  if (!host_type.RunStartHostHook(context)) {
    std::cerr << "Failed to start the " << host_type.name() << " host.\n";
    return 1;
  }
  std::cout << "Host service started successfully.\n";
  return 0;
}

}  // namespace remoting
