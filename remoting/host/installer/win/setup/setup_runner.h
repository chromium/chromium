// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INSTALLER_WIN_SETUP_SETUP_RUNNER_H_
#define REMOTING_HOST_INSTALLER_WIN_SETUP_SETUP_RUNNER_H_

#include <string>

#include "base/files/file_path.h"
#include "remoting/base/branding.h"

namespace base {
class CommandLine;
}  // namespace base

namespace remoting::installer {

// Process exit codes returned by `remoting_setup.exe`.
enum class SetupExitCode : int {
  kSuccess = 0,
  kInvalidArguments = 1,
  kPathResolutionFailed = 2,
  kInstallFailed = 3,
  kUninstallFailed = 4,
};

// Optional path and event overrides for executing setup pipelines in tests.
// Any empty `base::FilePath` falls back to its production default.
struct SetupConfig {
  SetupConfig();
  SetupConfig(const SetupConfig&);
  SetupConfig& operator=(const SetupConfig&);
  ~SetupConfig();

  base::FilePath source_dir;
  base::FilePath install_dir;
  base::FilePath host_config_path;
  std::wstring session_active_event_name = kHostSessionActiveEventName;
  std::wstring update_pending_event_name = kHostUpdatePendingEventName;
};

// Executes the setup action specified on `command_line` (`--install` or
// `--uninstall`; defaults to `--install` when no action switch is provided)
// using production paths and kernel event names.
SetupExitCode RunSetup(const base::CommandLine& command_line);

// Executes the setup action specified on `command_line` using the overrides in
// `config`.
SetupExitCode RunSetupWithConfig(const base::CommandLine& command_line,
                                 const SetupConfig& config);

}  // namespace remoting::installer

#endif  // REMOTING_HOST_INSTALLER_WIN_SETUP_SETUP_RUNNER_H_
