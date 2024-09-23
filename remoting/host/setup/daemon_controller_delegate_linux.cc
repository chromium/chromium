// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller_delegate_linux.h"

#include <unistd.h>

#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/host/host_config.h"
#include "remoting/host/usage_stats_consent.h"

namespace remoting {

namespace {

#ifndef NDEBUG
const char kDaemonDevScript[] = "remoting/chrome-remote-desktop";
#endif  // NDEBUG

const char kDaemonScript[] =
    "/opt/google/chrome-remote-desktop/chrome-remote-desktop";

// The name of the command-line switch used to specify the host configuration
// file to use.
const char kHostConfigSwitchName[] = "host-config";

bool start_host_after_setup = true;

base::FilePath GetConfigPath() {
  base::CommandLine* current_process = base::CommandLine::ForCurrentProcess();
  if (current_process->HasSwitch(kHostConfigSwitchName)) {
    return current_process->GetSwitchValuePath(kHostConfigSwitchName);
  }
  std::string filename = GetHostHash() + ".json";
  return GetConfigDirectoryPath().Append(filename);
}

bool GetScriptPath(base::FilePath* result) {
#ifndef NDEBUG
  base::FilePath out_dir;
  base::PathService::Get(base::DIR_EXE, &out_dir);
  base::FilePath dev_exe = out_dir.AppendASCII(kDaemonDevScript);
  if (access(dev_exe.value().c_str(), X_OK) == 0) {
    *result = dev_exe;
    return true;
  }
#endif  // NDEBUG

  base::FilePath candidate_exe(kDaemonScript);
  if (access(candidate_exe.value().c_str(), X_OK) == 0) {
    *result = candidate_exe;
    return true;
  }
  return false;
}

bool RunHostScript(const std::vector<std::string>& args) {
  // As long as we're relying on running an external binary from the
  // PATH, don't do it as root.
  if (getuid() == 0) {
    LOG(ERROR) << "Refusing to run script as root.";
    return false;
  }
  base::FilePath script_path;
  if (!GetScriptPath(&script_path)) {
    LOG(ERROR) << "GetScriptPath() failed.";
    return false;
  }
  base::CommandLine command_line(script_path);
  for (unsigned int i = 0; i < args.size(); ++i) {
    command_line.AppendArg(args[i]);
  }

  std::string output;
  bool result = base::GetAppOutputAndError(command_line, &output);
  if (result) {
    LOG(INFO) << output;
  } else {
    LOG(ERROR) << output;
  }

  return result;
}

}  // namespace

DaemonControllerDelegateLinux::DaemonControllerDelegateLinux() = default;

DaemonControllerDelegateLinux::~DaemonControllerDelegateLinux() = default;

DaemonController::State DaemonControllerDelegateLinux::GetState() {
  base::FilePath script_path;
  if (!GetScriptPath(&script_path)) {
    LOG(ERROR) << "GetScriptPath() failed.";
    return DaemonController::STATE_UNKNOWN;
  }
  base::CommandLine command_line(script_path);
  command_line.AppendArg("--get-status");
  command_line.AppendArg("--config=" + GetConfigPath().value());

  std::string status;
  int exit_code = 0;
  if (!base::GetAppOutputWithExitCode(command_line, &status, &exit_code) ||
      exit_code != 0) {
    LOG(ERROR) << "Failed to run \"" << command_line.GetCommandLineString()
               << "\". Exit code: " << exit_code;
    return DaemonController::STATE_UNKNOWN;
  }

  base::TrimWhitespaceASCII(status, base::TRIM_ALL, &status);

  if (status == "STARTED") {
    return DaemonController::STATE_STARTED;
  } else if (status == "STOPPED") {
    return DaemonController::STATE_STOPPED;
  } else if (status == "NOT_IMPLEMENTED") {
    // Chrome Remote Desktop is not currently supported on the underlying Linux
    // Distro.
    return DaemonController::STATE_NOT_IMPLEMENTED;
  } else {
    LOG(ERROR) << "Unknown status string returned from  \""
               << command_line.GetCommandLineString() << "\": " << status;
    return DaemonController::STATE_UNKNOWN;
  }
}

std::optional<base::Value::Dict> DaemonControllerDelegateLinux::GetConfig() {
  std::optional<base::Value::Dict> host_config(
      HostConfigFromJsonFile(GetConfigPath()));
  if (!host_config.has_value()) {
    return std::nullopt;
  }

  base::Value::Dict result;
  std::string* value = host_config->FindString(kHostIdConfigPath);
  if (value) {
    result.Set(kHostIdConfigPath, *value);
  }

  value = host_config->FindString(kServiceAccountConfigPath);
  if (value) {
    // Set both keys for compatibility purposes.
    result.Set(kServiceAccountConfigPath, *value);
    result.Set(kDeprecatedXmppLoginConfigPath, *value);
  }

  return result;
}

void DaemonControllerDelegateLinux::CheckPermission(
    bool it2me,
    DaemonController::BoolCallback callback) {
  std::move(callback).Run(true);
}

void DaemonControllerDelegateLinux::SetConfigAndStart(
    base::Value::Dict config,
    bool consent,
    DaemonController::CompletionCallback done) {
  // Ensure the configuration directory exists.
  base::FilePath config_dir = GetConfigPath().DirName();
  if (!base::DirectoryExists(config_dir)) {
    base::File::Error error;
    if (!base::CreateDirectoryAndGetError(config_dir, &error)) {
      LOG(ERROR) << "Failed to create config directory " << config_dir.value()
                 << ", error: " << base::File::ErrorToString(error);
      std::move(done).Run(DaemonController::RESULT_FAILED);
      return;
    }
  }

  // Write config.
  if (!HostConfigToJsonFile(std::move(config), GetConfigPath())) {
    LOG(ERROR) << "Failed to update config file.";
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  if (start_host_after_setup) {
    // Finally start the host.
    std::vector<std::string> args = {"--enable-and-start"};

    // TODO(rkjnsn): At this point, the host is configured and just requires an
    // administrator to enable and start it. If that fails here, e.g., due to no
    // policy kit agent running, it might be nice to tell the user what they
    // need to do so they can perform the last step manually (or have an
    // administrator do it, if the user isn't one).
    if (!RunHostScript(args)) {
      LOG(ERROR) << "Failed to start host.";
      std::move(done).Run(DaemonController::RESULT_FAILED);
      return;
    }
  }

  std::move(done).Run(DaemonController::RESULT_OK);
}

void DaemonControllerDelegateLinux::UpdateConfig(
    base::Value::Dict config,
    DaemonController::CompletionCallback done) {
  std::optional<base::Value::Dict> new_config(
      HostConfigFromJsonFile(GetConfigPath()));
  if (!new_config.has_value()) {
    LOG(ERROR) << "Failed to read existing config file.";
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  new_config->Merge(std::move(config));

  if (!HostConfigToJsonFile(std::move(*new_config), GetConfigPath())) {
    LOG(ERROR) << "Failed to update config file.";
    std::move(done).Run(DaemonController::RESULT_FAILED);
    return;
  }

  std::vector<std::string> args = {"--reload",
                                   "--config=" + GetConfigPath().value()};
  DaemonController::AsyncResult result = DaemonController::RESULT_FAILED;
  if (RunHostScript(args)) {
    result = DaemonController::RESULT_OK;
  }

  std::move(done).Run(result);
}

void DaemonControllerDelegateLinux::Stop(
    DaemonController::CompletionCallback done) {
  std::vector<std::string> args = {"--stop",
                                   "--config=" + GetConfigPath().value()};
  DaemonController::AsyncResult result = DaemonController::RESULT_FAILED;
  if (RunHostScript(args)) {
    result = DaemonController::RESULT_OK;
  }

  std::move(done).Run(result);
}

DaemonController::UsageStatsConsent
DaemonControllerDelegateLinux::GetUsageStatsConsent() {
  // Crash dump collection is not implemented on Linux yet.
  // http://crbug.com/130678.
  DaemonController::UsageStatsConsent consent;
  consent.supported = false;
  consent.allowed = false;
  consent.set_by_policy = false;
  return consent;
}

void DaemonControllerDelegateLinux::set_start_host_after_setup(
    bool start_host) {
  start_host_after_setup = start_host;
}

scoped_refptr<DaemonController> DaemonController::Create() {
  return new DaemonController(
      base::WrapUnique(new DaemonControllerDelegateLinux()));
}

}  // namespace remoting
