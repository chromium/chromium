// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller_delegate_linux_multi_process.h"

#include <unistd.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/process/launch.h"
#include "base/values.h"
#include "remoting/base/branding.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/logging.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/host_config.h"
#include "remoting/host/linux/host_type_environment.h"
#include "remoting/host/linux/host_types.h"
#include "remoting/host/setup/daemon_controller.h"

namespace remoting {
namespace {

// Runs a systemctl command with the given arguments. Returns true if the
// command succeeded (i.e. exit code is 0), and false otherwise.
bool RunSystemctlCommand(const base::CommandLine::StringVector& args) {
  base::CommandLine command_line(base::FilePath("systemctl"));
  for (const auto& arg : args) {
    command_line.AppendArg(arg);
  }
  std::string output;
  bool result = base::GetAppOutputAndError(command_line, &output);
  if (!result) {
    LOG(ERROR) << "Failed to run systemctl command. Output: " << output;
  }
  return result;
}

bool WriteConfigs(const base::DictValue& full_config) {
  // Ensure the configuration directory exists.
  base::FilePath config_dir = GetMultiProcessHostGlobalConfigDir();
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(config_dir, &error)) {
    if (error == base::File::FILE_ERROR_EXISTS) {
      if (!base::DeleteFile(config_dir)) {
        LOG(ERROR) << "Failed to delete file at path: " << config_dir.value();
        return false;
      }
      if (!base::CreateDirectoryAndGetError(config_dir, &error)) {
        LOG(ERROR) << "Failed to create config directory after deleting "
                   << "the file at the same path: " << config_dir.value()
                   << ", error: " << base::File::ErrorToString(error);
        return false;
      }
    } else {
      LOG(ERROR) << "Failed to create config directory " << config_dir.value()
                 << ", error: " << base::File::ErrorToString(error);
      return false;
    }
  }

  // The config dir needs to be readable and executable by everyone so that
  // other users can read the unprivileged config. The directory isn't deleted
  // on failure, since it also contains other files, e.g. the host type record.
  if (!base::SetPosixFilePermissions(config_dir, 0755)) {
    PLOG(ERROR) << "Failed to set permissions on config directory: "
                << config_dir.value();
    return false;
  }

  // Write full config.
  base::FilePath config_path =
      DaemonControllerDelegateLinuxMultiProcess::GetPrivilegedConfigPath();
  if (!HostConfigToJsonFile(full_config, config_path)) {
    LOG(ERROR) << "Failed to update config file: " << config_path.value();
    return false;
  }

  // Implementation of HostConfigToJsonFile guarantees that the file has the
  // permission 600 since it is renamed from a file created with mktemp(), but
  // we explicitly set the permission here since it isn't documented.
  if (!base::SetPosixFilePermissions(config_path, 0600)) {
    PLOG(ERROR) << "Failed to set permissions on config file: "
                << config_path.value();
    base::DeleteFile(config_path);
    return false;
  }

  // Sync unprivileged config.
  base::DictValue unprivileged_config;
  for (const auto& key : DaemonController::kUnprivilegedConfigKeys) {
    if (const base::Value* value = full_config.Find(key)) {
      unprivileged_config.Set(key, value->Clone());
    }
  }
  base::FilePath unprivileged_path =
      DaemonControllerDelegateLinuxMultiProcess::GetUnprivilegedConfigPath();
  if (!HostConfigToJsonFile(unprivileged_config, unprivileged_path)) {
    LOG(ERROR) << "Failed to update unprivileged config file: "
               << unprivileged_path.value();
    return false;
  }

  // Ensure the unprivileged config is readable by everyone.
  if (!base::SetPosixFilePermissions(unprivileged_path, 0644)) {
    PLOG(ERROR) << "Failed to set permissions on unprivileged config file: "
                << unprivileged_path.value();
    base::DeleteFile(unprivileged_path);
    return false;
  }

  return true;
}

}  // namespace

// static
base::FilePath
DaemonControllerDelegateLinuxMultiProcess::GetPrivilegedConfigPath() {
  return GetMultiProcessHostGlobalConfigDir().Append(kDefaultHostConfigFile);
}

// static
base::FilePath
DaemonControllerDelegateLinuxMultiProcess::GetUnprivilegedConfigPath() {
  return GetMultiProcessHostGlobalConfigDir().Append(
      kDefaultUnprivilegedConfigFileName);
}

DaemonControllerDelegateLinuxMultiProcess::
    DaemonControllerDelegateLinuxMultiProcess(
        const HostType& host_type,
        std::string_view local_user_name,
        std::unique_ptr<HostTypeEnvironment> env)
    : host_type_(host_type),
      local_user_name_(local_user_name),
      env_(std::move(env)) {
  CHECK(host_type.is_multi_process());
  CHECK(env_);
}

DaemonControllerDelegateLinuxMultiProcess::
    ~DaemonControllerDelegateLinuxMultiProcess() = default;

DaemonController::State DaemonControllerDelegateLinuxMultiProcess::GetState() {
  // Check if the multi-process host service is running.
  std::string state = env_->GetUnitActiveState(kMultiProcessHostUnit);
  // See:
  // https://www.freedesktop.org/software/systemd/man/latest/systemctl.html
  if (state == "active" || state == "reloading" || state == "refreshing") {
    // These are the states for which `systemctl is-active` succeeds.
    return DaemonController::STATE_STARTED;
  } else if (state == "inactive") {
    return DaemonController::STATE_STOPPED;
  } else if (state == "failed") {
    LOG(ERROR) << kMultiProcessHostUnit << " is in failed state.";
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
  if (!base::PathExists(GetUnprivilegedConfigPath())) {
    return std::nullopt;
  }
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
  if (!is_privileged()) {
    LOG(ERROR) << "Root privileges required to set multi-process config.";
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  // Checked before the config is written, so that e.g. an unsupported local
  // user is reported before anything is changed.
  HostTypeContext context(*env_, local_user_name_, IsCorpHostConfig(config));
  if (!host_type_->ValidateStartHostHookContext(context)) {
    LOG(ERROR) << "Cannot set up the " << host_type_->name() << " host.";
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  // This also creates the config directory, which is where some host types
  // write the session routing config file.
  if (!WriteConfigs(config)) {
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  // If a multi-process host config existed, HostStarterBase::StopOldHost()
  // has already stopped the host service (see Stop()).
  if (!host_type_->RunStartHostHook(context)) {
    LOG(ERROR) << "Failed to start the " << host_type_->name() << " host.";
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }
  std::move(done).Run(DaemonController::RESULT_OK);
}

void DaemonControllerDelegateLinuxMultiProcess::UpdateConfig(
    base::DictValue config,
    DaemonController::CompletionCallback done) {
  if (!is_privileged()) {
    LOG(ERROR) << "Root privileges required to update multi-process config.";
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  base::FilePath config_path =
      DaemonControllerDelegateLinuxMultiProcess::GetPrivilegedConfigPath();
  std::optional<base::DictValue> full_config(
      HostConfigFromJsonFile(config_path));
  if (!full_config.has_value()) {
    LOG(ERROR) << "Failed to read existing config file: "
               << config_path.value();
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  full_config->Merge(std::move(config));

  if (!WriteConfigs(*full_config)) {
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  // We only kill the main process of the service so that the daemon is
  // restarted and picks up the new configuration, but any existing
  // graphical sessions (which are in separate processes) are not terminated.
  // SIGKILL instead of SIGTERM is used to ensure the daemon process is
  // terminated immediately without it attempting to shut down the sessions.
  if (!RunSystemctlCommand({"kill", "-s", "SIGKILL", "--kill-who=main",
                            kMultiProcessHostUnit})) {
    LOG(ERROR) << "Failed to restart " << kMultiProcessHostUnit;
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

  // Only stops the host unit, which all multi-process host types share, so
  // the host type's state is kept.
  std::move(done).Run(env_->DisableAndStopUnit(kMultiProcessHostUnit)
                          ? DaemonController::RESULT_OK
                          : DaemonController::RESULT_FAILED);
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

bool DaemonControllerDelegateLinuxMultiProcess::is_multi_process() const {
  return true;
}

}  // namespace remoting
