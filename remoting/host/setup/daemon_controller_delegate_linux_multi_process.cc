// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller_delegate_linux_multi_process.h"

#include <unistd.h>

#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "remoting/base/branding.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/logging.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/host_config.h"
#include "remoting/host/setup/daemon_controller.h"

namespace remoting {

namespace {

// Returns the config path that is the source of truth. It may contain sensitive
// information, and may not be readable by the current user.
base::FilePath GetPrivilegedConfigPath() {
  return GetMultiProcessHostGlobalConfigDir().Append(kDefaultHostConfigFile);
}

// Returns a config path that can be access by an unprivileged user. Note that
// the returned config file could still contain sensitive information.
base::FilePath GetUnprivilegedConfigPath() {
  return GetMultiProcessHostGlobalConfigDir().Append(
      kDefaultUnprivilegedConfigFileName);
}

// Runs a systemctl command with the given arguments. Returns true if the
// command succeeded (i.e. exit code is 0), and false otherwise.
bool RunSystemctlCommand(const base::CommandLine::StringVector& args,
                         std::string* output = nullptr) {
  base::CommandLine command_line(base::FilePath("systemctl"));
  for (const auto& arg : args) {
    command_line.AppendArg(arg);
  }
  std::string local_output;
  bool result = base::GetAppOutputAndError(command_line, &local_output);
  if (output) {
    *output = local_output;
  } else if (!result) {
    LOG(ERROR) << "Failed to run systemctl command. Output: " << local_output;
  }
  return result;
}

}  // namespace

DaemonControllerDelegateLinuxMultiProcess::
    DaemonControllerDelegateLinuxMultiProcess() = default;

DaemonControllerDelegateLinuxMultiProcess::
    ~DaemonControllerDelegateLinuxMultiProcess() = default;

DaemonController::State DaemonControllerDelegateLinuxMultiProcess::GetState() {
  // Check if the multi-process host service is running.
  std::string state;
  if (RunSystemctlCommand({"is-active", "chrome-remote-desktop.service"},
                          &state)) {
    return DaemonController::STATE_STARTED;
  }
  base::TrimWhitespaceASCII(state, base::TRIM_ALL, &state);
  // See:
  // https://www.freedesktop.org/software/systemd/man/latest/systemctl.html
  if (state == "inactive") {
    return DaemonController::STATE_STOPPED;
  } else if (state == "failed") {
    LOG(ERROR) << "chrome-remote-desktop.service is in failed state.";
    return DaemonController::STATE_STOPPED;
  } else if (state == "activating") {
    return DaemonController::STATE_STARTING;
  } else if (state == "deactivating") {
    return DaemonController::STATE_STOPPING;
  }
  LOG(ERROR) << "Unexpected state returned from systemctl: " << state;
  return DaemonController::STATE_UNKNOWN;
}

std::optional<base::DictValue>
DaemonControllerDelegateLinuxMultiProcess::GetConfig() {
  return HostConfigFromJsonFile(GetUnprivilegedConfigPath());
}

void DaemonControllerDelegateLinuxMultiProcess::CheckPermission(
    bool it2me,
    DaemonController::BoolCallback callback) {
  std::move(callback).Run(true);
}

void DaemonControllerDelegateLinuxMultiProcess::SetConfigAndStart(
    base::DictValue config,
    bool consent,
    DaemonController::CompletionCallback done) {
  // TODO: b/502664397 - Support starting multi-process host for the start-host
  // command.
  NOTIMPLEMENTED();
  std::move(done).Run(DaemonController::RESULT_FAILED);
}

void DaemonControllerDelegateLinuxMultiProcess::UpdateConfig(
    base::DictValue config,
    DaemonController::CompletionCallback done) {
  base::FilePath config_path = GetPrivilegedConfigPath();

  if (!is_privileged()) {
    LOG(ERROR) << "Root privileges required to update multi-process config.";
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  std::optional<base::DictValue> full_config(
      HostConfigFromJsonFile(config_path));
  if (!full_config.has_value()) {
    LOG(ERROR) << "Failed to read existing config file: "
               << config_path.value();
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  full_config->Merge(std::move(config));

  if (!HostConfigToJsonFile(*full_config, config_path)) {
    LOG(ERROR) << "Failed to update config file: " << config_path.value();
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  // Sync unprivileged config.
  base::DictValue unprivileged_config;
  for (const auto& key : DaemonController::GetUnprivilegedConfigKeys()) {
    if (const base::Value* value = full_config->Find(key)) {
      unprivileged_config.Set(key, value->Clone());
    }
  }
  base::FilePath unprivileged_path = GetUnprivilegedConfigPath();
  if (!HostConfigToJsonFile(unprivileged_config, unprivileged_path)) {
    LOG(ERROR) << "Failed to update unprivileged config file: "
               << unprivileged_path.value();
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  // Ensure the unprivileged config is readable by everyone.
  if (!base::SetPosixFilePermissions(unprivileged_path, 0644)) {
    LOG(ERROR) << "Failed to set permissions on unprivileged config file: "
               << unprivileged_path.value();
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  // We only kill the main process of the service so that the daemon is
  // restarted and picks up the new configuration, but any existing
  // graphical sessions (which are in separate processes) are not terminated.
  // SIGKILL instead of SIGTERM is used to ensure the daemon process is
  // terminated immediately without it attempting to shut down the sessions.
  if (!RunSystemctlCommand({"kill", "-s", "SIGKILL", "--kill-who=main",
                            "chrome-remote-desktop.service"})) {
    LOG(ERROR) << "Failed to restart chrome-remote-desktop.service";
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }
  std::move(done).Run(DaemonController::RESULT_OK);
}

void DaemonControllerDelegateLinuxMultiProcess::Stop(
    DaemonController::CompletionCallback done) {
  if (!is_privileged()) {
    LOG(ERROR) << "Root privileges required to stop multi-process host.";
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  // Disable the service and stop the process immediately.
  if (RunSystemctlCommand(
          {"disable", "--now", "chrome-remote-desktop.service"})) {
    std::move(done).Run(DaemonController::RESULT_OK);
  } else {
    std::move(done).Run(DaemonController::RESULT_FAILED);
  }
}

DaemonController::UsageStatsConsent
DaemonControllerDelegateLinuxMultiProcess::GetUsageStatsConsent() {
  DaemonController::UsageStatsConsent consent;
  consent.supported = true;
  consent.allowed = false;
  consent.set_by_policy = false;

  base::FilePath config_path = GetUnprivilegedConfigPath();
  std::optional<base::DictValue> config = HostConfigFromJsonFile(config_path);
  if (config) {
    consent.allowed =
        config->FindBool(kUsageStatsConsentConfigPath).value_or(false);
  }

  return consent;
}

bool DaemonControllerDelegateLinuxMultiProcess::is_privileged() const {
  return getuid() == 0;  // Run as root.
}

}  // namespace remoting
