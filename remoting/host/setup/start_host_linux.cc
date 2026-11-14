// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/start_host_linux.h"

#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/types/expected.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/username.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_types.h"
#include "remoting/host/setup/daemon_controller.h"
#include "remoting/host/setup/daemon_controller_delegate_linux_single_process.h"

namespace remoting {

constexpr std::string_view kUserNameSwitch = "user-name";
constexpr std::string_view kCorpUserSwitch = "corp-user";
constexpr std::string_view kCloudUserSwitch = "cloud-user";
constexpr std::string_view kHostTypeSwitch = "host-type";
constexpr std::string_view kNoStartSwitch = "no-start";

bool ValidateUserSwitches(const base::CommandLine& command_line) {
  bool has_corp_user = command_line.HasSwitch(kCorpUserSwitch);
  bool has_cloud_user = command_line.HasSwitch(kCloudUserSwitch);

  if (has_corp_user && has_cloud_user) {
    std::cerr << "'--" << kCorpUserSwitch << "' and '--" << kCloudUserSwitch
              << "' flags cannot be used together.\n";
    return false;
  }
  if (has_cloud_user) {
    std::string arg_value = command_line.GetSwitchValueASCII(kCloudUserSwitch);
    if (!base::SplitStringOnce(arg_value, '@')) {
      std::cerr << "The --" << kCloudUserSwitch
                << " flag requires an email address.\n";
      return false;
    }
  }

  return true;
}

namespace {

// Returns whether the command line names the user that the host is set up for.
bool HasUserSwitch(const base::CommandLine& command_line) {
  return command_line.HasSwitch(kUserNameSwitch) ||
         command_line.HasSwitch(kCorpUserSwitch) ||
         command_line.HasSwitch(kCloudUserSwitch);
}

// Returns whether `user_name` (may be empty) can be used to set up
// `host_type`, and prints an error if not.
bool CheckLocalUser(const HostType& host_type, std::string_view user_name) {
  base::expected<void, std::string> user_check = CheckLocalUserForHostTypeSetup(
      host_type, /*current_host_type=*/nullptr, user_name);
  if (user_check.has_value()) {
    return true;
  }
  std::cerr << user_check.error();
  if (user_name.empty()) {
    std::cerr << " Pass --" << kUserNameSwitch << "=<user>.";
  }
  std::cerr << "\n";
  return false;
}

// Looks up the passwd entry for `local_user_name` and checks that it is
// neither root nor the network service account. Prints an error and returns
// std::nullopt on failure.
std::optional<PasswdUserInfo> LookupAndValidateLocalUser(
    std::string_view local_user_name) {
  auto user_info = GetPasswdUserInfo(std::string(local_user_name));
  if (!user_info.has_value()) {
    std::cerr << "Failed to find the local user '" << local_user_name
              << "': " << user_info.error() << "\n"
              << "Specify an existing local account with --" << kUserNameSwitch
              << "=<local user>.\n";
    return std::nullopt;
  }
  if (user_info->uid == 0 ||
      user_info->username == GetNetworkProcessUsername()) {
    std::cerr << "The local user '" << local_user_name
              << "' cannot be root or the network service account.\n";
    return std::nullopt;
  }
  return *user_info;
}

}  // namespace

std::string GetLocalUserNameForStartHost(
    const base::CommandLine& command_line) {
  // The 'user-name' flag contains the local account to run the host as.
  // If '--user-name' is not provided, then we will try to extract it from
  // either '--corp-user' or '--cloud-user'.
  if (command_line.HasSwitch(kUserNameSwitch)) {
    return command_line.GetSwitchValueASCII(kUserNameSwitch);
  }
  if (command_line.HasSwitch(kCorpUserSwitch)) {
    // For compat reasons, we support either email or username for this param.
    // TODO: joedow - Remove support for the email param in M139.
    std::string arg_value = command_line.GetSwitchValueASCII(kCorpUserSwitch);
    auto parts = base::SplitStringOnce(arg_value, '@');
    return parts ? std::string(parts->first) : arg_value;
  }
  if (command_line.HasSwitch(kCloudUserSwitch)) {
    std::string arg_value = command_line.GetSwitchValueASCII(kCloudUserSwitch);
    // ValidateUserSwitches() rejects a `--cloud-user` that is not an email
    // address.
    auto parts = base::SplitStringOnce(arg_value, '@');
    if (parts) {
      return std::string(parts->first);
    }
    return std::string();
  }

  return getuid() != 0 ? GetUsername() : std::string();
}

const HostType* ResolveTargetHostType(const base::CommandLine& command_line,
                                      std::string_view local_user_name,
                                      const HostTypeEnvironment* env) {
  std::unique_ptr<HostTypeEnvironment> owned_env;
  if (!env) {
    owned_env = HostTypeEnvironment::Create();
    env = owned_env.get();
  }

  const HostType* requested_host_type = nullptr;
  if (command_line.HasSwitch(kHostTypeSwitch)) {
    std::string host_type_name =
        command_line.GetSwitchValueASCII(kHostTypeSwitch);
    requested_host_type = HostType::Find(host_type_name);
    if (!requested_host_type) {
      std::cerr << "Invalid host type: " << host_type_name << "\n\n";
      HostType::PrintHostTypeHelp();
      return nullptr;
    }
  }

  base::expected<const HostType*, std::string> configured =
      HostType::GetConfigured(*env);
  if (!configured.has_value()) {
    std::cerr << configured.error() << "\n";
    return nullptr;
  }
  const HostType* configured_host_type = *configured;

  if (requested_host_type && configured_host_type &&
      requested_host_type != configured_host_type) {
    std::cerr << "Cannot change the configured host type from '"
              << configured_host_type->name() << "' to '"
              << requested_host_type->name()
              << "' with start-host. To switch host types, run:\n"
              << "  " << kMigrateHostPath << " " << requested_host_type->name();
    if (!local_user_name.empty()) {
      std::cerr << " --" << kUserNameSwitch << "=" << local_user_name;
    }
    std::cerr << "\n";
    return nullptr;
  }

  if (requested_host_type) {
    return requested_host_type;
  }
  // If the host type is not specified, the configured host is set up again
  // with its host type.
  return configured_host_type ? configured_host_type
                              : &HostType::GetDefaultHostType();
}

void ReExecStartHostWithSudo(const base::CommandLine& command_line,
                             std::string_view local_user_name) {
  CHECK_NE(getuid(), 0u);

  std::string user_name_switch;
  if (!HasUserSwitch(command_line) && !local_user_name.empty()) {
    user_name_switch =
        base::StrCat({"--", kUserNameSwitch, "=", local_user_name});
  }
  std::vector<const char*> sudo_argv;
  sudo_argv.push_back("/usr/bin/sudo");
  // Use '-k' to prevent silently piggybacking off a recent sudo session.
  sudo_argv.push_back("-k");
  // Use '--' to prevent any subsequent arguments from being interpreted as
  // sudo options.
  sudo_argv.push_back("--");
  const base::CommandLine::StringVector& args = command_line.argv();
  sudo_argv.push_back(args[0].c_str());
  // Insert the switch right after the program, so it can't end up after a
  // `--` that ends the switches.
  if (!user_name_switch.empty()) {
    sudo_argv.push_back(user_name_switch.c_str());
  }
  for (const auto& arg : base::span(args).subspan(1u)) {
    sudo_argv.push_back(arg.c_str());
  }
  sudo_argv.push_back(nullptr);  // NULL-terminated
  execv("/usr/bin/sudo", const_cast<char* const*>(sudo_argv.data()));
  // If execv returns, it failed.
  PLOG(ERROR) << "Failed to re-run with sudo";
}

int StartSingleProcessHostAsRoot(base::CommandLine& command_line,
                                 const HostType& host_type,
                                 const std::string& local_user_name) {
  CHECK_EQ(getuid(), 0u);
  CHECK(!host_type.is_multi_process());

  if (!CheckLocalUser(host_type, local_user_name)) {
    return 1;
  }

  std::optional<PasswdUserInfo> user_info =
      LookupAndValidateLocalUser(local_user_name);
  if (!user_info.has_value()) {
    return 1;
  }

  std::cout << "Configuring the host service to run as local account: "
            << local_user_name << "\n";

  base::FilePath home_dir_path = user_info->home_dir;
  if (!base::DirectoryExists(home_dir_path)) {
    std::cerr << "[WARNING] Can't find home directory ("
              << home_dir_path.value() << ") for " << local_user_name << "("
              << user_info->uid << ").\n"
              << "Please run the 'mkhomedir_helper' utility, or similar, to "
                 "create a home directory for the user.\n"
              << "The host setup process will not complete successfully "
                 "without one.\n";
  } else {
    std::cout << "Verified that home directory (" << home_dir_path.value()
              << ") exists for " << local_user_name << "(" << user_info->uid
              << ")\n";
  }

  int return_value = 1;
  // `--user-name` is only used by root; remove it so the child doesn't
  // receive it.
  command_line.RemoveSwitch(kUserNameSwitch);
  command_line.AppendSwitch(kNoStartSwitch);
  command_line.RemoveSwitch(kHostTypeSwitch);
  command_line.AppendSwitchASCII(kHostTypeSwitch, host_type.name());
  std::vector<std::string> register_host_command_line{
      "/usr/bin/sudo",
      "-u",
      local_user_name,
      "--",
  };
  register_host_command_line.insert(register_host_command_line.end(),
                                    command_line.argv().begin(),
                                    command_line.argv().end());
  // LaunchProcess redirects stdin to /dev/null, but start_host prompts for a
  // PIN if one isn't specified on the command-line, so dup and remap it.
  base::LaunchOptions options;
  options.allow_new_privs = true;
  base::ScopedFD stdin_dup(dup(STDIN_FILENO));
  if (stdin_dup.is_valid()) {
    options.fds_to_remap.emplace_back(stdin_dup.get(), STDIN_FILENO);
  }
  auto register_host_process =
      base::LaunchProcess(register_host_command_line, options);
  stdin_dup.reset();
  if (!register_host_process.IsValid() ||
      !register_host_process.WaitForExit(&return_value) || return_value != 0) {
    std::cerr << "Failed to register host.\n";
    return return_value != 0 ? return_value : 1;
  }

  // `is_corp_host` is only used to set up multi-process host types.
  std::unique_ptr<HostTypeEnvironment> env = HostTypeEnvironment::Create();
  HostTypeContext context(*env, local_user_name, /*is_corp_host=*/false);
  if (!host_type.RunStartHostHook(context)) {
    std::cerr << "Failed to start the " << host_type.name() << " host.\n";
    return 1;
  }
  std::cout << "Host service started successfully.\n";
  return 0;
}

bool ConfigureMultiProcessHostSetupAsRoot(base::CommandLine& command_line,
                                          const HostType& host_type,
                                          std::string_view local_user_name) {
  CHECK_EQ(getuid(), 0u);
  CHECK(host_type.is_multi_process());

  if (host_type.requires_local_user()) {
    if (!CheckLocalUser(host_type, local_user_name) ||
        !LookupAndValidateLocalUser(local_user_name)) {
      return false;
    }
    DaemonController::SetHostTypeForSetup(host_type, local_user_name);
  } else {
    DaemonController::SetHostTypeForSetup(host_type);
  }

  // `--user-name` is only used here (it may have been added when re-running
  // with sudo), and isn't accepted by the corp and cloud flows.
  command_line.RemoveSwitch(kUserNameSwitch);
  return true;
}

bool ConfigureSingleProcessHostSetupAsUser(base::CommandLine& command_line,
                                           const HostType& host_type) {
  CHECK_NE(getuid(), 0u);
  CHECK(!host_type.is_multi_process());

  if (command_line.HasSwitch(kUserNameSwitch) &&
      command_line.GetSwitchValueASCII(kUserNameSwitch) != GetUsername()) {
    std::cerr << "The --" << kUserNameSwitch << " switch ("
              << command_line.GetSwitchValueASCII(kUserNameSwitch)
              << ") does not match the current user (" << GetUsername()
              << "). Run with sudo to configure the host for another user.\n";
    return false;
  }

  if (command_line.HasSwitch(kNoStartSwitch)) {
    // Registering the host with systemd and starting it is the only reason
    // start_host requires root. The --no-start option skips that final step,
    // allowing it to be run non-interactively if the parent process has root
    // and can complete the setup itself. Since this functionality is
    // Linux-specific, it isn't plumbed through the platform-independent
    // daemon controller code, and must be configured on the Linux delegate
    // explicitly.
    DaemonControllerDelegateLinuxSingleProcess::set_start_host_after_setup(
        false);
    // Remove the switch from the command line to simplify arg count checks.
    command_line.RemoveSwitch(kNoStartSwitch);
  }
  DaemonController::SetHostTypeForSetup(host_type);

  // `--user-name` is only used here, and isn't accepted by the corp and cloud
  // flows.
  command_line.RemoveSwitch(kUserNameSwitch);
  return true;
}

}  // namespace remoting
