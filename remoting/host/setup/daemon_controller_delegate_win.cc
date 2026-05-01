// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller_delegate_win.h"

#include <stddef.h>
#include <windows.h>
#include <aclapi.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_info.h"
#include "base/strings/string_util_win.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "base/win/scoped_handle.h"
#include "remoting/base/branding.h"
#include "remoting/base/is_google_email.h"
#include "remoting/base/scoped_sc_handle_win.h"
#include "remoting/host/config_file_watcher.h"
#include "remoting/host/host_config.h"
#include "remoting/host/usage_stats_consent.h"
#include "remoting/host/win/security_descriptor.h"

namespace remoting {

namespace {

// The maximum size of the configuration file. "1MB ought to be enough" for any
// reasonable configuration we will ever need. 1MB is low enough to make the
// probability of out of memory situation fairly low. OOM is still possible and
// we will crash if it occurs.
const size_t kMaxConfigFileSize = 1024 * 1024;

// The host configuration file security descriptor that enables full access to
// Local System and built-in administrators only.
const char kConfigFileSecurityDescriptor[] =
    "O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)";

// The host configuration directory security descriptor that enables full access
// to Local System and built-in administrators, and read/execute access to
// built-in users (to allow traversal and reading the unprivileged config).
const char kConfigDirSecurityDescriptor[] =
    "O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGX;;;BU)";

const char kUnprivilegedConfigFileSecurityDescriptor[] =
    "O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;AU)";

// Helper to check if a handle refers to a reparse point (junction or symlink).
// This is used to prevent Local Privilege Escalation (LPE) via junction
// following.
bool IsHandleReparsePoint(HANDLE handle) {
  BY_HANDLE_FILE_INFORMATION info;
  if (!GetFileInformationByHandle(handle, &info)) {
    PLOG(ERROR) << "GetFileInformationByHandle failed";
    // Fail safe: return true if we can't verify the attributes.
    return true;
  }
  return (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
}

// Verifies that the final path of the handle matches the expected path.
// This prevents "intermediate junction" attacks where a component of the
// path above the leaf is a junction.
bool IsPathSafe(HANDLE handle, const base::FilePath& expected_path) {
  std::vector<wchar_t> buffer(MAX_PATH);
  DWORD result = GetFinalPathNameByHandleW(handle, buffer.data(),
                                           static_cast<DWORD>(buffer.size()),
                                           FILE_NAME_NORMALIZED);
  if (result >= buffer.size()) {
    buffer.resize(result);
    result = GetFinalPathNameByHandleW(handle, buffer.data(),
                                       static_cast<DWORD>(buffer.size()),
                                       FILE_NAME_NORMALIZED);
  }

  if (result == 0 || result >= buffer.size()) {
    PLOG(ERROR) << "GetFinalPathNameByHandleW failed";
    return false;
  }

  std::wstring final_path(buffer.data(), result);
  std::wstring expected_path_str = expected_path.value();

  // GetFinalPathNameByHandle prepends \\?\ to the path.
  if (base::StartsWith(final_path, L"\\\\?\\UNC\\", base::CompareCase::SENSITIVE)) {
    final_path = L"\\\\" + final_path.substr(8);
  } else if (base::StartsWith(final_path, L"\\\\?\\",
                             base::CompareCase::SENSITIVE)) {
    final_path = final_path.substr(4);
  }

  // Use _wcsicmp for robust case-insensitive comparison on Windows.
  if (_wcsicmp(final_path.c_str(), expected_path_str.c_str()) != 0) {
    LOG(ERROR) << "Path mismatch detected. Expected: " << expected_path_str
               << ", Final: " << final_path;
    return false;
  }

  return true;
}

// Reads and parses the configuration file securely without following junctions.
bool ReadConfig(const base::FilePath& filename, base::DictValue& config_out) {
  // Use FILE_FLAG_OPEN_REPARSE_POINT to open the junction itself if it exists,
  // rather than following it to a potentially sensitive target.
  base::win::ScopedHandle file(CreateFileW(
      filename.value().c_str(), GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));

  if (!file.is_valid()) {
    DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND) {
      LOG(INFO) << "'" << filename.value() << "' does not exist, skipping read.";
    } else {
      PLOG(ERROR) << "Failed to open '" << filename.value() << "'.";
    }
    return false;
  }

  if (IsHandleReparsePoint(file.Get())) {
    LOG(ERROR) << "Config file '" << filename.value() << "' is a reparse point.";
    return false;
  }

  if (!IsPathSafe(file.Get(), filename)) {
    LOG(ERROR) << "Config file path is insecure: " << filename.value();
    return false;
  }

  BY_HANDLE_FILE_INFORMATION info;
  if (!GetFileInformationByHandle(file.Get(), &info)) {
    PLOG(ERROR) << "GetFileInformationByHandle failed for '" << filename.value()
                << "'.";
    return false;
  }

  uint64_t file_size =
      (static_cast<uint64_t>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
  if (file_size == 0) {
    LOG(ERROR) << "Config file '" << filename.value() << "' is empty.";
    return false;
  }
  if (file_size > kMaxConfigFileSize) {
    LOG(ERROR) << "Config file '" << filename.value() << "' is too large.";
    return false;
  }

  std::string file_content;
  file_content.resize(static_cast<size_t>(file_size));
  DWORD bytes_read;
  if (!::ReadFile(file.Get(), file_content.data(),
                  static_cast<DWORD>(file_content.size()), &bytes_read,
                  nullptr)) {
    PLOG(ERROR) << "Failed to read '" << filename.value() << "'.";
    return false;
  }
  file_content.resize(bytes_read);

  std::optional<base::DictValue> config = HostConfigFromJson(file_content);
  if (!config.has_value()) {
    LOG(ERROR) << "Config file: '" << filename.value()
               << "' is empty or corrupt.";
    return false;
  }

  config_out = std::move(*config);
  return true;
}

// Sets the Owner, Group, and DACL on a handle to prevent non-admins from
// tampering with it or reverting security settings.
bool SetDirAcl(HANDLE handle, const char* sddl) {
  ScopedSd sd = ConvertSddlToSd(sddl);
  if (!sd) {
    PLOG(ERROR) << "Failed to convert SDDL to SD: " << sddl;
    return false;
  }

  PACL dacl = nullptr;
  BOOL dacl_present = FALSE;
  BOOL dacl_defaulted = FALSE;
  if (!GetSecurityDescriptorDacl(sd.get(), &dacl_present, &dacl,
                                 &dacl_defaulted)) {
    PLOG(ERROR) << "GetSecurityDescriptorDacl failed";
    return false;
  }

  PSID owner = nullptr;
  BOOL owner_defaulted = FALSE;
  if (!GetSecurityDescriptorOwner(sd.get(), &owner, &owner_defaulted)) {
    PLOG(ERROR) << "GetSecurityDescriptorOwner failed";
    return false;
  }

  PSID group = nullptr;
  BOOL group_defaulted = FALSE;
  if (!GetSecurityDescriptorGroup(sd.get(), &group, &group_defaulted)) {
    PLOG(ERROR) << "GetSecurityDescriptorGroup failed";
    return false;
  }

  // Set Owner, Group, and DACL. Setting the Owner prevents an unprivileged
  // creator of the directory from regaining control.
  DWORD result = SetSecurityInfo(
      handle, SE_FILE_OBJECT,
      OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
          DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
      owner, group, dacl, nullptr);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "SetSecurityInfo failed: " << result;
    return false;
  }
  return true;
}

// Writes a config file atomically and safely.
bool WriteConfigSafe(const base::FilePath& target_path,
                     const std::string& content,
                     const char* sddl) {
  base::FilePath config_dir = target_path.DirName();

  // 1. Verify config_dir is not a reparse point.
  // We omit FILE_SHARE_DELETE to prevent the directory from being renamed
  // while we have it open, ensuring our later move operation is not redirected.
  base::win::ScopedHandle dir_handle(CreateFileW(
      config_dir.value().c_str(), FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr));

  if (!dir_handle.is_valid()) {
    PLOG(ERROR) << "Failed to open config directory: " << config_dir.value();
    return false;
  }

  if (IsHandleReparsePoint(dir_handle.Get())) {
    LOG(ERROR) << "Config directory is a reparse point: " << config_dir.value();
    return false;
  }

  if (!IsPathSafe(dir_handle.Get(), config_dir)) {
    LOG(ERROR) << "Config directory path is insecure: " << config_dir.value();
    return false;
  }

  // 2. Create a secure temporary file with a random name.
  ScopedSd sd = ConvertSddlToSd(sddl);
  if (!sd) {
    LOG(ERROR) << "Failed to convert SDDL to security descriptor.";
    return false;
  }

  SECURITY_ATTRIBUTES sa = {sizeof(sa), sd.get(), FALSE};
  base::FilePath temp_path;
  base::win::ScopedHandle temp_file;

  for (int i = 0; i < 5; ++i) {
    std::string random_name =
        base::UnguessableToken::Create().ToString() + ".json~";
    temp_path = config_dir.AppendASCII(random_name);

    // CREATE_NEW and FILE_FLAG_OPEN_REPARSE_POINT ensure we don't follow an
    // existing junction or overwrite an existing file.
    temp_file.Set(::CreateFileW(
        temp_path.value().c_str(), GENERIC_WRITE, 0, &sa, CREATE_NEW,
        FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));

    if (temp_file.is_valid()) {
      break;
    }

    if (::GetLastError() != ERROR_FILE_EXISTS) {
      PLOG(ERROR) << "Failed to create secure temp file in "
                  << config_dir.value();
      return false;
    }
  }

  if (!temp_file.is_valid()) {
    LOG(ERROR) << "Failed to generate a unique secure temp file name.";
    return false;
  }

  // 3. Write content and flush to disk.
  DWORD written;
  if (!::WriteFile(temp_file.Get(), content.c_str(),
                   static_cast<DWORD>(content.length()), &written, nullptr) ||
      written != content.length()) {
    PLOG(ERROR) << "Failed to write to temp file: " << temp_path.value();
    temp_file.Close();
    base::DeleteFile(temp_path);
    return false;
  }

  if (!::FlushFileBuffers(temp_file.Get())) {
    PLOG(ERROR) << "Failed to flush temp file buffers: " << temp_path.value();
    temp_file.Close();
    base::DeleteFile(temp_path);
    return false;
  }

  temp_file.Close();

  // 4. Atomic replacement.
  // We use MoveFileExW with MOVEFILE_REPLACE_EXISTING because it is atomic
  // on the same volume and preserves the security descriptor of the temporary
  // file (unlike ReplaceFileW which inherits the target's ACLs).
  if (!::MoveFileExW(temp_path.value().c_str(), target_path.value().c_str(),
                     MOVEFILE_REPLACE_EXISTING)) {
    PLOG(ERROR) << "Failed to move temp file to target: " << target_path.value();
    base::DeleteFile(temp_path);
    return false;
  }

  return true;
}

// Writes the configuration file up to |kMaxConfigFileSize| in size.
bool WriteConfig(const base::FilePath& config_dir,
                 const base::DictValue& config) {
  std::string config_json = HostConfigToJson(config);
  if (config_json.length() > kMaxConfigFileSize) {
    LOG(ERROR) << "Config is larger than the max size: " << kMaxConfigFileSize;
    return false;
  }

  // Ensure the required fields are present.
  if (!config.FindString(kHostIdConfigPath)) {
    LOG(ERROR) << "Config is missing " << kHostIdConfigPath;
    return false;
  }
  const std::string* host_owner = config.FindString(kHostOwnerConfigPath);
  if (!host_owner) {
    LOG(ERROR) << "Config is missing " << kHostOwnerConfigPath;
    return false;
  }
  if (!config.FindString(kHostSecretHashConfigPath) &&
      !IsGoogleEmail(*host_owner)) {
    // PIN authentication is not needed for Google hosts so we only want to
    // fail if a secret_hash value isn't present for a non-Google host.
    LOG(ERROR) << "Config is missing " << kHostSecretHashConfigPath;
    return false;
  }
  if (!config.FindString(kServiceAccountConfigPath) &&
      !config.FindString(kDeprecatedXmppLoginConfigPath)) {
    LOG(ERROR) << "Config is missing " << kServiceAccountConfigPath << " and "
               << kDeprecatedXmppLoginConfigPath;
    return false;
  }

  // Write full configuration.
  base::FilePath full_config_path = config_dir.Append(kDefaultHostConfigFile);
  if (!WriteConfigSafe(full_config_path, config_json,
                       kConfigFileSecurityDescriptor)) {
    return false;
  }

  // Extract the unprivileged fields from the configuration.
  base::DictValue unprivileged_config;
  for (const auto& key : DaemonController::GetUnprivilegedConfigKeys()) {
    if (const base::Value* value = config.Find(key)) {
      unprivileged_config.Set(key, value->Clone());
    }
  }

  // Write the unprivileged configuration file.
  base::FilePath unprivileged_config_path =
      config_dir.Append(kDefaultUnprivilegedConfigFileName);
  return WriteConfigSafe(unprivileged_config_path,
                         HostConfigToJson(unprivileged_config),
                         kUnprivilegedConfigFileSecurityDescriptor);
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
  }
}

ScopedScHandle OpenService(DWORD access) {
  // Open the service and query its current state.
  ScopedScHandle scmanager(
      ::OpenSCManagerW(nullptr, SERVICES_ACTIVE_DATABASE,
                       SC_MANAGER_CONNECT | SC_MANAGER_ENUMERATE_SERVICE));
  if (!scmanager.is_valid()) {
    PLOG(ERROR) << "Failed to connect to the service control manager";
    return ScopedScHandle();
  }

  ScopedScHandle service(
      ::OpenServiceW(scmanager.Get(), kWindowsServiceName, access));
  if (!service.is_valid()) {
    PLOG(ERROR) << "Failed to open the '" << kWindowsServiceName << "' service";
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
  DWORD access = SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_START |
                 SERVICE_STOP;
  ScopedScHandle service = OpenService(access);
  if (!service.is_valid()) {
    return false;
  }

  // Change the service start type to 'auto'.
  if (!::ChangeServiceConfigW(service.Get(), SERVICE_NO_CHANGE,
                              SERVICE_AUTO_START, SERVICE_NO_CHANGE, nullptr,
                              nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr)) {
    PLOG(ERROR) << "Failed to change the '" << kWindowsServiceName
                << "' service start type to 'auto'";
    return false;
  }

  // Start the service.
  if (!StartService(service.Get(), 0, nullptr)) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_ALREADY_RUNNING) {
      LOG(ERROR) << "Failed to start the '" << kWindowsServiceName
                 << "' service: " << error;

      return false;
    }
  }

  return true;
}

bool StopDaemon() {
  DWORD access = SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_START |
                 SERVICE_STOP;
  ScopedScHandle service = OpenService(access);
  if (!service.is_valid()) {
    return false;
  }

  // Change the service start type to 'manual'.
  if (!::ChangeServiceConfigW(service.Get(), SERVICE_NO_CHANGE,
                              SERVICE_DEMAND_START, SERVICE_NO_CHANGE, nullptr,
                              nullptr, nullptr, nullptr, nullptr, nullptr,
                              nullptr)) {
    PLOG(ERROR) << "Failed to change the '" << kWindowsServiceName
                << "' service start type to 'manual'";
    return false;
  }

  // Stop the service.
  SERVICE_STATUS status;
  if (!ControlService(service.Get(), SERVICE_CONTROL_STOP, &status)) {
    DWORD error = GetLastError();
    if (error != ERROR_SERVICE_NOT_ACTIVE) {
      LOG(ERROR) << "Failed to stop the '" << kWindowsServiceName
                 << "' service: " << error;
      return false;
    }
  }

  return true;
}

}  // namespace

DaemonControllerDelegateWin::DaemonControllerDelegateWin()
    : config_dir_(GetConfigDir()) {}

DaemonControllerDelegateWin::DaemonControllerDelegateWin(
    const base::FilePath& config_dir)
    : config_dir_(config_dir) {}

DaemonControllerDelegateWin::~DaemonControllerDelegateWin() {}

DaemonController::State DaemonControllerDelegateWin::GetState() {
  // TODO(alexeypa): Make the thread alertable, so we can switch to APC
  // notifications rather than polling.
  ScopedScHandle service = OpenService(SERVICE_QUERY_STATUS);
  if (!service.is_valid()) {
    return DaemonController::STATE_UNKNOWN;
  }

  SERVICE_STATUS status;
  if (!::QueryServiceStatus(service.Get(), &status)) {
    PLOG(ERROR) << "Failed to query the state of the '" << kWindowsServiceName
                << "' service";
    return DaemonController::STATE_UNKNOWN;
  }

  return ConvertToDaemonState(status.dwCurrentState);
}

std::optional<base::DictValue> DaemonControllerDelegateWin::GetConfig() {
  base::DictValue config;
  if (!ReadConfig(config_dir_.Append(kDefaultUnprivilegedConfigFileName),
                  config)) {
    return std::nullopt;
  }

  return std::move(config);
}

void DaemonControllerDelegateWin::UpdateConfig(
    base::DictValue updated_config,
    DaemonController::CompletionCallback done) {
  // Get the old config.
  base::DictValue config;
  if (!ReadConfig(config_dir_.Append(kDefaultHostConfigFile), config)) {
    InvokeCompletionCallback(std::move(done), false);
    return;
  }

  // Merge items from the new config into the existing config.
  config.Merge(std::move(updated_config));

  // Write the updated config.
  bool result = WriteConfig(config_dir_, config);

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

bool DaemonControllerDelegateWin::is_privileged() const {
  return base::IsCurrentProcessElevated();
}

void DaemonControllerDelegateWin::CheckPermission(
    bool it2me,
    DaemonController::BoolCallback callback) {
  std::move(callback).Run(true);
}

void DaemonControllerDelegateWin::SetConfigAndStart(
    base::DictValue config,
    bool consent,
    DaemonController::CompletionCallback done) {
  // Record the user's consent.
  if (!remoting::SetUsageStatsConsent(consent)) {
    InvokeCompletionCallback(std::move(done), false);
    return;
  }

  // Determine the config directory path and create it if necessary.
  if (!base::PathExists(config_dir_)) {
    if (!base::CreateDirectory(config_dir_)) {
      PLOG(ERROR) << "Failed to create the config directory.";
      InvokeCompletionCallback(std::move(done), false);
      return;
    }
  }

  // Open the directory handle securely (no junction following).
  // We need READ_CONTROL | WRITE_DAC | WRITE_OWNER | FILE_READ_ATTRIBUTES
  // to set the ACL and verify the path.
  // We omit FILE_SHARE_DELETE to prevent the directory from being renamed
  // during this operation.
  base::win::ScopedHandle dir_handle(CreateFileW(
      config_dir_.value().c_str(),
      READ_CONTROL | WRITE_DAC | WRITE_OWNER | FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr));

  if (!dir_handle.is_valid()) {
    PLOG(ERROR) << "Failed to open config directory for ACL setting: "
                << config_dir_.value();
    InvokeCompletionCallback(std::move(done), false);
    return;
  }

  if (IsHandleReparsePoint(dir_handle.Get())) {
    LOG(ERROR) << "Config directory is a reparse point: " << config_dir_.value();
    InvokeCompletionCallback(std::move(done), false);
    return;
  }

  if (!IsPathSafe(dir_handle.Get(), config_dir_)) {
    LOG(ERROR) << "Config directory path is insecure: " << config_dir_.value();
    InvokeCompletionCallback(std::move(done), false);
    return;
  }

  // Set strict ACLs on the directory, including the Owner and Group.
  if (!SetDirAcl(dir_handle.Get(), kConfigDirSecurityDescriptor)) {
    InvokeCompletionCallback(std::move(done), false);
    return;
  }

  // Set the configuration.
  if (!WriteConfig(config_dir_, config)) {
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
