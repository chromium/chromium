// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/start_host_as_root.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/safe_strerror.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/host/daemon_process.h"

namespace remoting {

namespace {

constexpr std::string_view kUserNameSwitch = "user-name";
constexpr std::string_view kCorpUserSwitch = "corp-user";
constexpr std::string_view kCloudUserSwitch = "cloud-user";
constexpr std::string_view kGdmManagedSwitch = "gdm-managed";
constexpr std::string_view kFromLegacyConfigSwitch = "from-legacy-config";
constexpr std::string_view kLegacySwitch = "legacy";
constexpr std::string_view kFromMultiProcessConfigSwitch =
    "from-multi-process-config";

bool ValidateCommandLineForHostTypes(const base::CommandLine& command_line) {
  bool is_gdm_managed = command_line.HasSwitch(kGdmManagedSwitch);
  bool from_legacy_config = command_line.HasSwitch(kFromLegacyConfigSwitch);
  bool is_legacy = command_line.HasSwitch(kLegacySwitch);
  bool from_multi_process_config =
      command_line.HasSwitch(kFromMultiProcessConfigSwitch);

  if (from_legacy_config && !is_gdm_managed) {
    std::cerr << "'--" << kFromLegacyConfigSwitch
              << "' can only be used with '--" << kGdmManagedSwitch << "'.\n";
    return false;
  }

  if (from_multi_process_config && !is_legacy) {
    std::cerr << "'--" << kFromMultiProcessConfigSwitch
              << "' can only be used with '--" << kLegacySwitch << "'.\n";
    return false;
  }

  if (is_gdm_managed && is_legacy) {
    std::cerr << "'--" << kGdmManagedSwitch << "' and '--" << kLegacySwitch
              << "' flags cannot be used together.\n";
    return false;
  }

  // TODO: crbug.com/475611769 - Remove this once it is supported.
  if (is_gdm_managed && !from_legacy_config) {
    std::cerr << "Starting new GDM-managed hosts is not supported yet.\n";
    return false;
  }
  return true;
}

bool ValidateCommandLine(const base::CommandLine& command_line) {
  bool has_username = command_line.HasSwitch(kUserNameSwitch);
  bool has_corp_user = command_line.HasSwitch(kCorpUserSwitch);
  bool has_cloud_user = command_line.HasSwitch(kCloudUserSwitch);

  if (!ValidateCommandLineForHostTypes(command_line)) {
    return false;
  }

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

std::string ExtractUsernameFromCommandLine(
    const base::CommandLine& command_line) {
  // The 'user-name' flag contains the local account to run the host as.
  // If '--user-name' is not provided, then we will try to extract it from
  // either '--cloud-user' or '--corp-user'.
  // Note, it is expected that the switch contents have been validated via
  // ValidateCommandLine() before calling this function.
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
    user_name = base::SplitStringOnce(arg_value, '@')->first;
  }

  return user_name;
}

// Enable or disable (and start or stop accordingly) a systemd unit. Returns the
// exit code.
int ControlService(const std::string& unit_name, bool enable) {
  const char* action = enable ? "enable" : "disable";
  std::vector<std::string> command_line = {"systemctl", action, "--now",
                                           unit_name};
  int exit_code = 1;
  auto process = base::LaunchProcess(command_line, base::LaunchOptions());
  if (!process.WaitForExit(&exit_code) || exit_code != 0) {
    std::cerr << "Failed to " << action << " host service (" << unit_name
              << ").\n";
  }
  return exit_code;
}

struct HostConfig {
  std::string config_name;
  base::FilePath config_path;
  // If nullptr, file config owner will not be changed.
  raw_ptr<const passwd> config_file_owner;
  mode_t config_dir_permissions;
  std::string service_name;
};

int MigrateExistingConfig(const HostConfig& src, const HostConfig& dst) {
  std::cout << "Migrating host config from " << src.config_name << " to "
            << dst.config_name << ".\n";

  if (!base::PathExists(src.config_path)) {
    std::cerr << "Source config file not found: " << src.config_path.value()
              << "\n";
    return 1;
  }

  base::FilePath dst_dir = dst.config_path.DirName();
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(dst_dir, &error)) {
    std::cerr << "Failed to create directory: " << dst_dir.value() << ": "
              << base::File::ErrorToString(error) << "\n";
    return 1;
  }
  if (dst.config_file_owner &&
      HANDLE_EINTR(chown(dst_dir.value().c_str(), dst.config_file_owner->pw_uid,
                         dst.config_file_owner->pw_gid)) != 0) {
    std::cerr << "Failed to change config directory ownership for "
              << dst_dir.value() << ": " << base::safe_strerror(errno) << "\n";
    return 1;
  }
  if (HANDLE_EINTR(
          chmod(dst_dir.value().c_str(), dst.config_dir_permissions)) != 0) {
    std::cerr << "Failed to set permissions for " << dst_dir.value() << ": "
              << base::safe_strerror(errno) << "\n";
    return 1;
  }

  // The config file should only be readable and writable by the owner.
  // Note that umask is applied to the whole process and therefore not
  // thread-safe. It is safe here since the start host process is
  // single-threaded.
  mode_t old_mask = umask(077);
  if (!base::CopyFile(src.config_path, dst.config_path)) {
    std::cerr << "Failed to copy config file from " << src.config_path.value()
              << " to " << dst.config_path.value() << "\n";
    umask(old_mask);
    return 1;
  }
  umask(old_mask);

  if (dst.config_file_owner &&
      HANDLE_EINTR(chown(dst.config_path.value().c_str(),
                         dst.config_file_owner->pw_uid,
                         dst.config_file_owner->pw_gid)) != 0) {
    std::cerr << "Failed to change config file ownership for "
              << dst.config_path.value() << ": " << base::safe_strerror(errno)
              << "\n";
    return 1;
  }

  // Disable the old service.
  int exit_code = ControlService(src.service_name, /*enable=*/false);
  if (exit_code != 0) {
    return exit_code;
  }

  // Enable the new service.
  exit_code = ControlService(dst.service_name, /*enable=*/true);
  if (exit_code != 0) {
    return exit_code;
  }

  // Ideally we should also delete the old host config file, but an internal
  // service will re-provision the host if it detects that the config file no
  // longer exists.
  // TODO: b/495898776 - delete old config file once tooling is fixed.

  std::cout << "Successfully migrated host config.\n";
  return 0;
}

}  // namespace

int StartHostAsRoot() {
  DCHECK(getuid() == 0);

  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (!ValidateCommandLine(command_line)) {
    return 1;
  }
  std::string user_name = ExtractUsernameFromCommandLine(command_line);
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

  bool is_gdm_managed = command_line.HasSwitch(kGdmManagedSwitch);
  bool from_legacy_config = command_line.HasSwitch(kFromLegacyConfigSwitch);
  bool is_legacy = command_line.HasSwitch(kLegacySwitch);
  bool from_multi_process_config =
      command_line.HasSwitch(kFromMultiProcessConfigSwitch);

  if ((is_gdm_managed && from_legacy_config) ||
      (is_legacy && from_multi_process_config)) {
    HostConfig legacy_config = {
        .config_name = "legacy",
        .config_path = base::FilePath(user_struct->pw_dir)
                           .Append(GetPerUserConfigRelativeDir())
                           .Append(GetHostHash() + ".json"),
        .config_file_owner = user_struct,
        .config_dir_permissions = 0700,
        .service_name =
            std::string("chrome-remote-desktop@") + user_struct->pw_name};
    HostConfig gdm_managed_config = {
        .config_name = "gdm-managed",
        .config_path = DaemonProcess::GetConfigPath(),
        // Config file ownership does not need to be changed since this process
        // is already run as root.
        .config_file_owner = nullptr,
        // Allow the network user to access paired-clients/ in the config
        // directory. The config file will still have the permissions of 600 so
        // it won't be accessible by non-root users.
        .config_dir_permissions = 0755,
        .service_name = "chrome-remote-desktop"};

    if (is_gdm_managed) {
      return MigrateExistingConfig(legacy_config, gdm_managed_config);
    } else {
      return MigrateExistingConfig(gdm_managed_config, legacy_config);
    }
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
    std::cerr << "Failed to set new config.\n";
    return return_value;
  }

  return_value =
      ControlService(std::string("chrome-remote-desktop@") + user_name,
                     /*enable=*/true);
  if (return_value != 0) {
    return return_value;
  }
  std::cout << "Host service started successfully.\n";
  return 0;
}

}  // namespace remoting
