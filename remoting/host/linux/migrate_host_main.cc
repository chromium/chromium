// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/migrate_host_main.h"

#include <signal.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/process/launch.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/optional_util.h"
#include "remoting/base/logging.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/username.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_type_migrator.h"
#include "remoting/host/linux/host_types.h"

namespace remoting {

namespace {

constexpr char kUserNameSwitch[] = "user-name";
constexpr char kKeepConfigSwitch[] = "keep-config";

std::optional<PasswdUserInfo> LookupAndValidateLocalUser(
    const std::string& user_name) {
  CHECK(!user_name.empty());
  if (!IsUserNameSafeForUnitName(user_name)) {
    std::cerr << "The username '" << user_name << "' is not supported.\n";
    return std::nullopt;
  }
  auto user_info = GetPasswdUserInfo(user_name);
  if (!user_info.has_value()) {
    std::cerr << "Failed to look up user: " << user_name << ": "
              << user_info.error() << "\n";
    return std::nullopt;
  }
  if (user_info->uid == 0 ||
      user_info->username == GetNetworkProcessUsername()) {
    std::cerr << "The local user '" << user_name
              << "' cannot be root or the network service account.\n";
    return std::nullopt;
  }
  return std::move(*user_info);
}

int ElevateAndRunMigrateHost(const base::CommandLine& command_line,
                             const HostType& target_host_type) {
  CHECK_NE(getuid(), 0u);
  CHECK_NE(GetUsername(), GetNetworkProcessUsername());

  std::string user_name = command_line.GetSwitchValueASCII(kUserNameSwitch);
  if (!user_name.empty()) {
    if (!LookupAndValidateLocalUser(user_name)) {
      return 1;
    }
  } else {
    user_name = GetUsername();
  }

  base::CommandLine sudo_command_line(base::CommandLine::NO_PROGRAM);
  std::vector<std::string> full_args{"/usr/bin/sudo", "-k", "--",
                                     command_line.GetProgram().value(),
                                     std::string(target_host_type.name())};

  // Without a user, the elevated process asks for --user-name if one is
  // needed.
  if (!user_name.empty()) {
    full_args.push_back(
        base::StringPrintf("--%s=%s", kUserNameSwitch, user_name.c_str()));
  }

  if (command_line.HasSwitch(kKeepConfigSwitch)) {
    full_args.push_back(base::StringPrintf("--%s", kKeepConfigSwitch));
  }

  sudo_command_line.InitFromArgv(full_args);

  base::LaunchOptions options;
  options.allow_new_privs = true;

  int exit_code = 1;
  auto process = base::LaunchProcess(sudo_command_line, options);
  if (!process.IsValid() || !process.WaitForExit(&exit_code)) {
    std::cerr << "Failed to launch elevated migrate-host process.\n";
    return 1;
  }

  return exit_code != 0 ? (exit_code > 0 ? exit_code : 1) : 0;
}

int RunMigrateHostAsRoot(const base::CommandLine& command_line,
                         const HostType& target_host_type) {
  CHECK_EQ(getuid(), 0u);

  std::string user_name = command_line.GetSwitchValueASCII(kUserNameSwitch);
  std::optional<PasswdUserInfo> user_info;
  if (!user_name.empty()) {
    user_info = LookupAndValidateLocalUser(user_name);
    if (!user_info.has_value()) {
      return 1;
    }
  }

  std::unique_ptr<HostTypeEnvironment> env = HostTypeEnvironment::Create();
  base::expected<const HostType*, std::string> configured =
      HostType::GetConfigured(*env);
  if (!configured.has_value()) {
    std::cerr << configured.error() << "\n";
    return 1;
  }
  const HostType* current_host_type = *configured;

  // Without a configured host type, HostTypeMigrator only prints how to set up
  // a host, which doesn't need a local user.
  if (current_host_type) {
    base::expected<void, std::string> user_check =
        CheckLocalUserForHostTypeSetup(target_host_type, current_host_type,
                                       user_name);
    if (!user_check.has_value()) {
      std::cerr << user_check.error();
      if (user_name.empty()) {
        std::cerr << " Pass --" << kUserNameSwitch
                  << "=<user>, or run this command again as the user.";
      }
      std::cerr << "\n";
      return 1;
    }
  }

  return HostTypeMigrator(*env, current_host_type, target_host_type,
                          base::OptionalToPtr(user_info),
                          command_line.HasSwitch(kKeepConfigSwitch))
                 .Run()
             ? 0
             : 1;
}

}  // namespace

int MigrateHostMain(int argc, char** argv) {
  base::AtExitManager exit_manager;
  // RunAsUser() pipes JSON to child processes; ignore SIGPIPE so an early
  // child exit surfaces as a write error instead of terminating migrate-host.
  signal(SIGPIPE, SIG_IGN);

  base::CommandLine::Init(argc, argv);
  InitHostLogging();

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (std::optional<int> exit_code = RunMigrationChildIfRequested(command_line);
      exit_code.has_value()) {
    return *exit_code;
  }

  const base::CommandLine::StringVector& args = command_line.GetArgs();
  if (args.size() != 1 || command_line.HasSwitch("help") ||
      command_line.HasSwitch("h") || command_line.HasSwitch("?")) {
    std::cerr << "Usage: migrate-host <host_type> [--user-name=<username>] "
                 "[--keep-config]\n\n";
    // Internal switches are not documented here.
    HostType::PrintHostTypeHelp();
    return 1;
  }

  std::string host_type_str = args[0];
  const HostType* target_host_type = HostType::Find(host_type_str);
  if (!target_host_type) {
    std::cerr << "Unknown host type: " << host_type_str << "\n\n";
    HostType::PrintHostTypeHelp();
    return 1;
  }

  if (getuid() != 0) {
    return ElevateAndRunMigrateHost(command_line, *target_host_type);
  }

  return RunMigrateHostAsRoot(command_line, *target_host_type);
}

}  // namespace remoting
