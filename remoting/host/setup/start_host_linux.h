// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_START_HOST_LINUX_H_
#define REMOTING_HOST_SETUP_START_HOST_LINUX_H_

#include <string>
#include <string_view>

namespace base {
class CommandLine;
}  // namespace base

namespace remoting {

class HostType;
class HostTypeEnvironment;

// Validates the user switches of `command_line` and prints an error to
// std::cerr if they are invalid (e.g. both `--corp-user` and `--cloud-user`, or
// `--cloud-user` is not an email address).
bool ValidateUserSwitches(const base::CommandLine& command_line);

// Returns the local account for `start-host`: the user from `--user-name`,
// `--corp-user`, or `--cloud-user`, in that order; else GetUsername() when
// non-root; else empty.
std::string GetLocalUserNameForStartHost(const base::CommandLine& command_line);

// Resolves the target host type from `--host-type`,
// `HostType::GetConfigured()`, or `HostType::GetDefaultHostType()`. Rejects
// `--host-type` if it differs from the configured host type (printing the
// `migrate-host` command to run). Returns nullptr on error.
const HostType* ResolveTargetHostType(const base::CommandLine& command_line,
                                      std::string_view local_user_name,
                                      const HostTypeEnvironment* env = nullptr);

// Replaces the current process with `sudo -k -- <argv...>`, adding
// `--user-name=<local_user_name>` if `command_line` has no user switch. Only
// returns if `execv()` fails.
void ReExecStartHostWithSudo(const base::CommandLine& command_line,
                             std::string_view local_user_name);

// Completes the full `start-host` setup for a single-process host when invoked
// as root: validates `local_user_name`, spawns `sudo -u <local_user_name>
// start-host --no-start` so `HostStarter` registers the host and writes the
// user's config unprivileged, then runs `host_type.RunStartHostHook()` as root
// to enable and start the systemd unit. Returns the process exit code.
int StartSingleProcessHostAsRoot(base::CommandLine& command_line,
                                 const HostType& host_type,
                                 const std::string& local_user_name);

// Validates `local_user_name`, configures `DaemonController` for the
// multi-process `host_type`, and removes `--user-name` from `command_line`
// before `StartHostMain()` runs `HostStarter` as root.
bool ConfigureMultiProcessHostSetupAsRoot(base::CommandLine& command_line,
                                          const HostType& host_type,
                                          std::string_view local_user_name);

// Configures `DaemonController` for the single-process `host_type` (including
// `--no-start` when spawned as an unprivileged child by
// `StartSingleProcessHostAsRoot()`) and removes Linux-only switches
// (`--user-name`, `--no-start`) from `command_line` before `StartHostMain()`
// runs `HostStarter` as the current user. Returns false if `--user-name` was
// passed and does not match the current user.
bool ConfigureSingleProcessHostSetupAsUser(base::CommandLine& command_line,
                                           const HostType& host_type);

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_START_HOST_LINUX_H_
