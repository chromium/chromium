// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller_delegate_win.h"

#include <stddef.h>

#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "base/win/scoped_bstr.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/branding.h"
#include "remoting/host/host_config.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/win/security_descriptor.h"

namespace remoting {

namespace {

// The maximum size of the configuration file. "1MB ought to be enough" for any
// reasonable configuration we will ever need. 1MB is low enough to make
// the probability of out of memory situation fairly low. OOM is still possible
// and we will crash if it occurs.
const size_t kMaxConfigFileSize = 1024 * 1024;

// The host configuration file name.
const base::FilePath::CharType kConfigFileName[] =
    FILE_PATH_LITERAL("host.json");

// The unprivileged configuration file name.
const base::FilePath::CharType kUnprivilegedConfigFileName[] =
    FILE_PATH_LITERAL("host_unprivileged.json");

// The extension for the temporary file.
const base::FilePath::CharType kTempFileExtension[] =
    FILE_PATH_LITERAL("json~");

// The host configuration file security descriptor that enables full access to
// Local System and built-in administrators only.
const char kConfigFileSecurityDescriptor[] =
    "O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)";

const char kUnprivilegedConfigFileSecurityDescriptor[] =
    "O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;AU)";

// Configuration keys.

// The configuration keys that cannot be specified in UpdateConfig().
const char* const kReadonlyKeys[] = {
  kHostIdConfigPath, kHostOwnerConfigPath, kHostOwnerEmailConfigPath,
  kXmppLoginConfigPath };

// The configuration keys whose values may be read by GetConfig().
const char* const kUnprivilegedConfigKeys[] = {
  kHostIdConfigPath, kXmppLoginConfigPath };

// Reads and parses the configuration file up to |kMaxConfigFileSize| in
// size.
bool ReadConfig(const base::FilePath& filename, base::Value::Dict& config_out) {
  std::string file_content;
  if (!base::ReadFileToStringWithMaxSize(filename, &file_content,
                                         kMaxConfigFileSize)) {
    PLOG(ERROR) << "Failed to read '" << filename.value() << "'.";
    return false;
  }

  // Parse the JSON configuration, expecting it to contain a dictionary.
  absl::optional<base::Value> value =
      base::JSONReader::Read(file_content, base::JSON_ALLOW_TRAILING_COMMAS);

  if (!value || !value->is_dict()) {
    LOG(ERROR) << "Failed to parse '" << filename.value() << "'.";
    return false;
  }

  config_out = std::move(*value).TakeDict();
  return true;
}

base::FilePath GetTempLocationFor(const base::FilePath& filename) {
  return filename.ReplaceExtension(kTempFileExtension);
}

// Writes a config file to a temporary location.
bool WriteConfigFileToTemp(const base::FilePath& filename,
                           const char* security_descriptor,
                           const std::string& content) {
  // Create the security descriptor for the configuration file.
  ScopedSd sd = ConvertSddlToSd(security_descriptor);
  if (!sd) {
    PLOG(ERROR)
        << "Failed to create a security descriptor for the configuration file";
    return false;
  }

  SECURITY_ATTRIBUTES security_attributes = {0};
  security_attributes.nLength = sizeof(security_attributes);
  security_attributes.lpSecurityDescriptor = sd.get();
  security_attributes.bInheritHandle = FALSE;

  // Create a temporary file and write configuration to it.
  base::FilePath tempname = GetTempLocationFor(filename);
  base::win::ScopedHandle file(
      CreateFileW(tempname.value().c_str(),
                  GENERIC_WRITE,
                  0,
                  &security_attributes,
                  CREATE_ALWAYS,
                  FILE_FLAG_SEQUENTIAL_SCAN,
                  nullptr));

  if (!file.IsValid()) {
    PLOG(ERROR) << "Failed to create '" << filename.value() << "'";
    return false;
  }

  DWORD written;
  if (!::WriteFile(file.Get(), content.c_str(), content.length(),
                   &written, nullptr)) {
    PLOG(ERROR) << "Failed to write to '" << filename.value() << "'";
    return false;
  }

  return true;
}

// Moves a config file from its temporary location to its permanent location.
bool MoveConfigFileFromTemp(const base::FilePath& filename) {
  // Now that the configuration is stored successfully replace the actual
  // configuration file.
  base::FilePath tempname = GetTempLocationFor(filename);
  if (!MoveFileExW(tempname.value().c_str(),
                   filename.value().c_str(),
                   MOVEFILE_REPLACE_EXISTING)) {
      PLOG(ERROR) << "Failed to rename '" << tempname.value() << "' to '"
                  << filename.value() << "'";
      return false;
  }

  return true;
}

// Writes the configuration file up to |kMaxConfigFileSize| in size.
bool WriteConfig(const std::string& content) {
  if (content.length() > kMaxConfigFileSize) {
      return false;
  }

  // Extract the configuration data that the user will verify.
  absl::optional<base::Value> config_value = base::JSONReader::Read(content);
  if (!config_value || !config_value->is_dict()) {
    return false;
  }

  base::Value::Dict& config_dict = config_value->GetDict();

  std::string* email;
  if (!(email = config_dict.FindString(kHostOwnerEmailConfigPath)) &&
      !(email = config_dict.FindString(kHostOwnerConfigPath)) &&
      !(email = config_dict.FindString(kXmppLoginConfigPath))) {
    return false;
  }
  std::string* host_id = config_dict.FindString(kHostIdConfigPath);
  std::string* host_secret_hash =
      config_dict.FindString(kHostSecretHashConfigPath);
  if (!host_id || !host_secret_hash) {
    return false;
  }

  // Extract the unprivileged fields from the configuration.
  base::Value::Dict unprivileged_config_dict;
  for (const char* key : kUnprivilegedConfigKeys) {
    if (std::string* value = config_dict.FindString(key)) {
      unprivileged_config_dict.Set(key, std::move(*value));
    }
  }
  std::string unprivileged_config_str;
  base::JSONWriter::Write(unprivileged_config_dict, &unprivileged_config_str);

  // Write the full configuration file to a temporary location.
  base::FilePath full_config_file_path =
      remoting::GetConfigDir().Append(kConfigFileName);
  if (!WriteConfigFileToTemp(full_config_file_path,
                             kConfigFileSecurityDescriptor,
                             content)) {
    return false;
  }

  // Write the unprivileged configuration file to a temporary location.
  base::FilePath unprivileged_config_file_path =
      remoting::GetConfigDir().Append(kUnprivilegedConfigFileName);
  if (!WriteConfigFileToTemp(unprivileged_config_file_path,
                             kUnprivilegedConfigFileSecurityDescriptor,
                             unprivileged_config_str)) {
    return false;
  }

  // Move the full and unprivileged configuration files to their permanent
  // locations.
  return MoveConfigFileFromTemp(full_config_file_path) &&
         MoveConfigFileFromTemp(unprivileged_config_file_path);
}

DaemonController::State ConvertToDaemonState(DWORD service_state) {
  switch (service_state) {
  case SERVICE_RUNNING:
    return DaemonController::STATE_STARTED;

  case SERVICE_CONTINUE_PENDING:
  case SERVICE_START_PENDING:
    return DaemonController::STATE_STARTING;

  case SERVICE_PAUSE_PENDING:
  case SERVICE_STOP_PENDING:
    return DaemonController::STATE_STOPPING;

  case SERVICE_PAUSED:
  case SERVICE_STOPPED:
    return DaemonController::STATE_STOPPED;

  default:
    NOTREACHED();
    return DaemonController::STATE_UNKNOWN;
  }
}

ScopedScHandle OpenService(DWORD access) {
  // Open the service and query its current state.
  ScopedScHandle scmanager(
      ::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE,
                       SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));
  if (!scmanager.IsValid()) {
    PLOG(ERROR) << "Failed to connect to the service control manager";
    return ScopedScHandle();
  }

  ScopedScHandle service(::OpenServiceW(scmanager.Get(), kWindowsServiceName,
                                        access));
  if (!service.IsValid()) {
    PLOG(ERROR) << "Failed to open to the '" << kWindowsServiceName
                << "' service";
  }

  return service;
}

void InvokeCompletionCallback(DaemonController::CompletionCallback done,
                              bool success) {
  DaemonController::AsyncResult async_result =
      success ? DaemonController::RESULT_OK : DaemonController::RESULT_FAILED;
  std::move(done).Run(async_result);
}

bool StartDaemon() {
  DWORD access = SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS |
                 SERVICE_START | SERVICE_STOP;
  ScopedScHandle service = OpenService(access);
  if (!service.IsValid())
    return false;

  // Change the service start type to 'auto'.
  if (!::ChangeServiceConfigW(service.Get(),
                              SERVICE_NO_CHANGE,
                              SERVICE_AUTO_START,
                              SERVICE_NO_CHANGE,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr)) {
    PLOG(ERROR) << "Failed to change the '" << kWindowsServiceName
                << "'service start type to 'auto'";
    return false;
  }

  // Start the service.
  if (!StartService(service.Get(), 0, nullptr)) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_ALREADY_RUNNING) {
      LOG(ERROR) << "Failed to start the '" << kWindowsServiceName
                  << "'service: " << error;

      return false;
    }
  }

  return true;
}

bool StopDaemon() {
  DWORD access = SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS |
                 SERVICE_START | SERVICE_STOP;
  ScopedScHandle service = OpenService(access);
  if (!service.IsValid())
    return false;

  // Change the service start type to 'manual'.
  if (!::ChangeServiceConfigW(service.Get(),
                              SERVICE_NO_CHANGE,
                              SERVICE_DEMAND_START,
                              SERVICE_NO_CHANGE,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr,
                              nullptr)) {
    PLOG(ERROR) << "Failed to change the '" << kWindowsServiceName
                << "'service start type to 'manual'";
    return false;
  }

  // Stop the service.
  SERVICE_STATUS status;
  if (!ControlService(service.Get(), SERVICE_CONTROL_STOP, &status)) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_NOT_ACTIVE) {
      LOG(ERROR) << "Failed to stop the '" << kWindowsServiceName
                  << "'service: " << error;
      return false;
    }
  }

  return true;
}

}  // namespace

DaemonControllerDelegateWin::DaemonControllerDelegateWin() {
}

DaemonControllerDelegateWin::~DaemonControllerDelegateWin() {
}

DaemonController::State DaemonControllerDelegateWin::GetState() {
  // TODO(alexeypa): Make the thread alertable, so we can switch to APC
  // notifications rather than polling.
  ScopedScHandle service = OpenService(SERVICE_QUERY_STATUS);
  if (!service.IsValid())
    return DaemonController::STATE_UNKNOWN;

  SERVICE_STATUS status;
  if (!::QueryServiceStatus(service.Get(), &status)) {
    PLOG(ERROR) << "Failed to query the state of the '"
                << kWindowsServiceName << "' service";
    return DaemonController::STATE_UNKNOWN;
  }

  return ConvertToDaemonState(status.dwCurrentState);
}

absl::optional<base::Value::Dict> DaemonControllerDelegateWin::GetConfig() {
  base::FilePath config_dir = remoting::GetConfigDir();

  // Read the unprivileged part of host configuration.
  base::Value::Dict config;
  if (!ReadConfig(config_dir.Append(kUnprivilegedConfigFileName), config))
    return absl::nullopt;

  return config;
}

void DaemonControllerDelegateWin::UpdateConfig(
    base::Value::Dict config,
    DaemonController::CompletionCallback done) {
  // Check for bad keys.
  for (size_t i = 0; i < std::size(kReadonlyKeys); ++i) {
    if (config.Find(kReadonlyKeys[i])) {
      LOG(ERROR) << "Cannot update config: '" << kReadonlyKeys[i]
                 << "' is read only.";
      InvokeCompletionCallback(std::move(done), false);
      return;
    }
  }
  // Get the old config.
  base::FilePath config_dir = remoting::GetConfigDir();
  base::Value::Dict config_old;
  if (!ReadConfig(config_dir.Append(kConfigFileName), config_old)) {
    InvokeCompletionCallback(std::move(done), false);
    return;
  }

  // Merge items from the given config into the old config.
  config_old.Merge(std::move(config));

  // Write the updated config.
  std::string config_updated_str;
  base::JSONWriter::Write(config_old, &config_updated_str);
  bool result = WriteConfig(config_updated_str);

  InvokeCompletionCallback(std::move(done), result);
}

void DaemonControllerDelegateWin::Stop(
    DaemonController::CompletionCallback done) {
  bool result = StopDaemon();

  InvokeCompletionCallback(std::move(done), result);
}

DaemonController::UsageStatsConsent
DaemonControllerDelegateWin::GetUsageStatsConsent() {
  DaemonController::UsageStatsConsent consent;
  consent.supported = true;
  consent.allowed = true;
  consent.set_by_policy = false;

  // Get the recorded user's consent.
  bool allowed;
  bool set_by_policy;
  // If the user's consent is not recorded yet, assume that the user didn't
  // consent to collecting crash dumps.
  if (remoting::GetUsageStatsConsent(&allowed, &set_by_policy)) {
    consent.allowed = allowed;
    consent.set_by_policy = set_by_policy;
  }

  return consent;
}

void DaemonControllerDelegateWin::CheckPermission(
    bool it2me,
    DaemonController::BoolCallback callback) {
  std::move(callback).Run(true);
}

void DaemonControllerDelegateWin::SetConfigAndStart(
    base::Value::Dict config,
    bool consent,
    DaemonController::CompletionCallback done) {
  // Record the user's consent.
  if (!remoting::SetUsageStatsConsent(consent)) {
    InvokeCompletionCallback(std::move(done), false);
    return;
  }

  // Set the configuration.
  std::string config_str;
  base::JSONWriter::Write(config, &config_str);

  // Determine the config directory path and create it if necessary.
  base::FilePath config_dir = remoting::GetConfigDir();
  if (!base::CreateDirectory(config_dir)) {
    PLOG(ERROR) << "Failed to create the config directory.";
    InvokeCompletionCallback(std::move(done), false);
    return;
  }

  if (!WriteConfig(config_str)) {
    InvokeCompletionCallback(std::move(done), false);
    return;
  }

  // Start daemon.
  InvokeCompletionCallback(std::move(done), StartDaemon());
}

scoped_refptr<DaemonController> DaemonController::Create() {
  return new DaemonController(
      base::WrapUnique(new DaemonControllerDelegateWin()));
}

}  // namespace remoting
