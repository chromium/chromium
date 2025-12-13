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

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"

namespace remoting {

namespace {

constexpr char kChromotingGroupName[] = "chrome-remote-desktop";
constexpr std::string_view kUserNameSwitch = "user-name";
constexpr std::string_view kCorpUserSwitch = "corp-user";
constexpr std::string_view kCloudUserSwitch = "cloud-user";

void PrintGroupMembershipError(const char* user_name) {
  UNSAFE_TODO(fprintf(stderr,
                      "%s is not a member of the `%s` group.\n"
                      "  Please add them using this command:\n"
                      "  sudo usermod -G %s %s\n",
                      user_name, kChromotingGroupName, kChromotingGroupName,
                      user_name));
}

bool CheckChromotingGroupMembership(const char* user_name,
                                    gid_t user_group_id) {
  errno = 0;
  group* chromoting_group = getgrnam(kChromotingGroupName);
  if (!chromoting_group) {
    // The installer creates this group so this condition is unexpected but we
    // check here in case the machine is in a bad state or our installer was
    // modified (broken) in some way such that the group does not exist.
    UNSAFE_TODO(
        fprintf(stderr,
                "Failed to retrieve group info for `%s`. errno = %s(%d)\n"
                "  Please create this group using this command:\n"
                "  sudo addgroup --system chrome-remote-desktop\n",
                kChromotingGroupName, strerror(errno), errno));
    return false;
  }

  // Figure out how many groups the user is in.
  int group_count = 0;
  getgrouplist(user_name, user_group_id, nullptr, &group_count);

  if (group_count < 1) {
    PrintGroupMembershipError(user_name);
    return false;
  }

  // Retrieve the groups.
  std::vector<gid_t> groups(group_count);
  getgrouplist(user_name, user_group_id, groups.data(), &group_count);
  if (!base::Contains(groups, chromoting_group->gr_gid)) {
    PrintGroupMembershipError(user_name);
    return false;
  }

  UNSAFE_TODO(fprintf(stdout, "Verified that %s is a member of %s\n", user_name,
                      kChromotingGroupName));
  return true;
}

bool ValidateCommandLine(const base::CommandLine& command_line) {
  bool has_username = command_line.HasSwitch(kUserNameSwitch);
  bool has_corp_user = command_line.HasSwitch(kCorpUserSwitch);
  bool has_cloud_user = command_line.HasSwitch(kCloudUserSwitch);

  if (!has_username && !has_corp_user && !has_cloud_user) {
    fprintf(stderr,
            "At least one of the following args must be provided:\n"
            "  --user-name=<username>\n"
            "  --corp-user=<username>\n"
            "  --cloud-user=<email>\n");
    return false;
  } else if (has_corp_user && has_cloud_user) {
    fprintf(
        stderr,
        "'--corp-user' and '--cloud-user' flags cannot be used together.\n");
    return false;
  } else if (has_cloud_user) {
    std::string arg_value = command_line.GetSwitchValueASCII(kCloudUserSwitch);
    if (!base::SplitStringOnce(arg_value, '@')) {
      fprintf(stderr, "The --cloud-user flag requires an email address.\n");
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

}  // namespace

int StartHostAsRoot(int argc, char** argv) {
  DCHECK(getuid() == 0);

  base::CommandLine command_line(argc, argv);
  if (!ValidateCommandLine(command_line)) {
    return 1;
  }
  std::string user_name = ExtractUsernameFromCommandLine(command_line);
  if (user_name.empty()) {
    fprintf(stderr,
            "A username and/or email must be provided from one of the "
            "following switches:\n"
            "  --user-name=<username>\n"
            "  --corp-user=<username>\n"
            "  --cloud-user=<email>\n");
    return 1;
  }
  fprintf(stdout, "Configuring the host service to run as local account: %s\n",
          user_name.c_str());

  errno = 0;
  const passwd* user_struct = getpwnam(user_name.c_str());
  if (!user_struct) {
    UNSAFE_TODO(
        fprintf(stderr,
                "Failed to retrieve passwd struct for %s. errno = %s(%d)\n"
                "Does this user account exist on the machine?\n",
                user_name.c_str(), strerror(errno), errno));
    return -1;
  }

  std::string home_dir = user_struct->pw_dir ?: "";
  base::FilePath home_dir_path = base::FilePath(home_dir);
  if (!base::DirectoryExists(home_dir_path)) {
    fprintf(stderr,
            "[WARNING] Can't find home directory (%s) for %s(%d).\n"
            "Please run the 'mkhomedir_helper' utility, or similar, to create "
            "a home directory for the user.\nThe host setup process will not "
            "complete successfully without one.\n",
            home_dir.c_str(), user_name.c_str(), user_struct->pw_uid);
  } else {
    fprintf(stdout, "Verified that home directory (%s) exists for %s(%d)\n",
            home_dir.c_str(), user_name.c_str(), user_struct->pw_uid);
  }

  // This switch is provided for environments where systemd is not being used.
  bool use_sysvinit = command_line.HasSwitch("sysvinit");
  if (use_sysvinit) {
    fprintf(stdout, "Checking sysvinit configuration requirements.\n");
    if (!CheckChromotingGroupMembership(user_struct->pw_name,
                                        user_struct->pw_gid)) {
      return -1;
    }
  }

  int return_value = 1;
  command_line.RemoveSwitch(kUserNameSwitch);
  command_line.RemoveSwitch("sysvinit");
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

  std::vector<std::string> start_service_command_line{
      "systemctl", "enable", "--now",
      std::string("chrome-remote-desktop@") + user_name};
  if (use_sysvinit) {
    // Using sysvinit is much less common that using systemd so we optimize for
    // that codepath and reset the vector if needed.
    start_service_command_line = {"/etc/init.d/chrome-remote-desktop", "start",
                                  user_name};
  }

  return_value = 1;
  auto start_service_process =
      base::LaunchProcess(start_service_command_line, base::LaunchOptions());
  if (!start_service_process.WaitForExit(&return_value) || return_value != 0) {
    fprintf(stderr, "Failed to enable host service.\n");
    return return_value;
  }

  printf("Host service started successfully.\n");
  return 0;
}

}  // namespace remoting
