// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_linux.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/values.h"
#include "remoting/base/crash/crashpad_database_manager.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/loggable.h"
#include "remoting/base/logging.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/secure_file_reader.h"
#include "remoting/base/username.h"
#include "remoting/base/version.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

namespace remoting {

namespace {

constexpr char kChromotingCrashpadHandler[] = "crashpad-handler";
constexpr char kDefaultCrashpadUploadUrl[] =
    "https://clients2.google.com/cr/report";

CrashpadLinux::Config ReadAndValidateConfigFile(const base::FilePath& path,
                                                uid_t expected_owner_uid) {
  CrashpadLinux::Config config;
  auto contents =
      ReadSecureFile(path, expected_owner_uid, kMaxCrashpadConfigBytes);
  if (!contents.has_value()) {
    LOG(ERROR) << "Ignoring Crashpad config at " << path << ": "
               << contents.error();
    return config;
  }
  if (!contents->has_value()) {
    return config;
  }

  auto json = base::JSONReader::ReadAndReturnValueWithError(
      **contents, base::JSON_PARSE_RFC);
  if (!json.has_value()) {
    LOG(ERROR) << "Failed to parse Crashpad config JSON in " << path << ": "
               << json.error().message;
    return config;
  }
  if (!json->is_dict()) {
    LOG(ERROR) << "Crashpad config " << path
               << " root JSON value is not a dictionary";
    return config;
  }
  const base::DictValue& dict = json->GetDict();

  if (const base::Value* url_val = dict.Find(kCrashpadUploadUrlKey)) {
    if (!url_val->is_string()) {
      LOG(ERROR) << "Ignoring non-string Crashpad " << kCrashpadUploadUrlKey
                 << " in " << path
                 << " (type=" << base::Value::GetTypeName(url_val->type())
                 << ")";
    } else if (IsValidCrashpadUploadUrlOverride(url_val->GetString())) {
      config.upload_url = url_val->GetString();
    } else {
      LOG(ERROR) << "Ignoring invalid Crashpad " << kCrashpadUploadUrlKey
                 << " in " << path;
    }
  }

  if (const base::Value* rate_val = dict.Find(kCrashpadDisableRateLimitKey)) {
    if (!rate_val->is_bool()) {
      LOG(ERROR) << "Ignoring non-boolean Crashpad "
                 << kCrashpadDisableRateLimitKey << " in " << path
                 << " (type=" << base::Value::GetTypeName(rate_val->type())
                 << ")";
    } else {
      config.disable_rate_limit = rate_val->GetBool();
    }
  }

  for (const auto [key, value] : dict) {
    if (key != kCrashpadUploadUrlKey && key != kCrashpadDisableRateLimitKey) {
      LOG(WARNING) << "Unknown key in Crashpad config " << path << ": " << key;
    }
  }

  return config;
}

bool SetupCrashpadDirectory() {
  if (getuid() != 0) {
    // Non-root ME2ME worker processes connect to the Crashpad handler via an
    // inherited socket and do not initialize the database directory.
    // Standalone user-mode processes (e.g. IT2Me native messaging host) write
    // to a user-owned directory (e.g. `$XDG_RUNTIME_DIR/crd_crashpad` or
    // `~/.config/...`), which is created on demand by Crashpad during database
    // initialization. Elevated setup is only required for root daemons to
    // pre-create the unified directory
    // `/var/lib/chrome-remote-desktop/crashpad` and chown it to
    // `_crd_crashpad`.
    return true;
  }
  base::FilePath path = GetCrashpadDatabasePath();
  auto delete_path = [&path](std::string_view reason) {
    if (!base::DeletePathRecursively(path)) {
      PLOG(ERROR) << "Failed to delete insecure directory " << path
                  << " after failure: " << reason;
    } else {
      LOG(ERROR) << "Insecure directory " << path
                 << " deleted due to setup failure: " << reason;
    }
  };

  if (base::PathExists(path) && !base::DirectoryExists(path)) {
    delete_path("Path exists but is not a directory");
  }

  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(path, &error)) {
    LOG(ERROR) << "Failed to create " << path << ": "
               << base::File::ErrorToString(error);
    return false;
  }

  auto user_info = GetPasswdUserInfo(GetCrashpadProcessUsername());
  if (!user_info.has_value()) {
    delete_path(user_info.error().ToString());
    return false;
  }

  if (HANDLE_EINTR(
          chown(path.value().c_str(), user_info->uid, user_info->gid)) != 0) {
    delete_path("chown failed");
    return false;
  }

  // Make sure the directory is only accessible by the owner.
  if (HANDLE_EINTR(chmod(path.value().c_str(), 0700)) != 0) {
    delete_path("chmod failed");
    return false;
  }
  return true;
}

}  // namespace

// static
bool CrashpadLinux::Initialize() {
  if (getuid() == 0 && !SetupCrashpadDirectory()) {
    LOG(ERROR) << "Failed to setup Crashpad directory.";
    return false;
  }

  // Leave metrics_path empty because this option is not used (or supported) on
  // non-Chromium builds.
  base::FilePath metrics_path;

  std::map<std::string, std::string> annotations;
  annotations["prod"] = "Chromoting_Linux";
  annotations["ver"] = REMOTING_VERSION_STRING;
  annotations["plat"] = std::string("Linux");

  const Config config = ReadConfig();
  const std::string upload_url =
      config.upload_url.value_or(kDefaultCrashpadUploadUrl);
  if (config.upload_url.has_value()) {
    LOG(WARNING) << "Overriding Crashpad upload URL via " << GetConfigFilePath()
                 << ": " << upload_url;
  }

  std::vector<std::string> arguments;
  // Make sure Crashpad's generate_dump tool includes monitor-self annotations.
  // This creates a second crashpad instance that monitors the handler so it can
  // report crashes in the handler.
  arguments.push_back("--monitor-self-annotation=ptype=crashpad-handler");
  if (config.disable_rate_limit) {
    arguments.push_back("--no-rate-limit");
    LOG(WARNING) << "Disabling Crashpad upload rate limit via "
                 << GetConfigFilePath();
  }

  base::FilePath handler_path;
  if (!GetCrashpadHandlerPath(&handler_path)) {
    return false;
  }

  crashpad::CrashpadClient client;
  if (!client.StartHandler(handler_path, GetCrashpadDatabasePath(),
                           metrics_path, upload_url, annotations, arguments,
                           false, false)) {
    LOG(ERROR) << "Failed to start Crashpad handler.";
    return false;
  }

  HOST_LOG << "Crashpad handler started.";
  return true;
}

// static
bool CrashpadLinux::InitializeClient(base::ScopedFD handler_socket,
                                     pid_t handler_pid) {
  if (!handler_socket.is_valid()) {
    LOG(ERROR) << "Invalid Crashpad handler socket.";
    return false;
  }

  crashpad::CrashpadClient client;
  // SIGSYS handling is reserved for the Seccomp-BPF sandbox, which uses it to
  // emulate or deny syscalls.
  client.SetUnhandledSignals({SIGSYS});
  if (!client.SetHandlerSocket(std::move(handler_socket), handler_pid)) {
    LOG(ERROR) << "Failed to set Crashpad handler socket.";
    return false;
  }

  HOST_LOG << "Crashpad client initialized with handler PID: " << handler_pid;
  return true;
}

// static
bool CrashpadLinux::GetHandlerSocket(base::ScopedFD& socket, pid_t& pid) {
  int raw_sock = -1;
  pid_t raw_pid = -1;
  if (!crashpad::CrashpadClient::GetHandlerSocket(&raw_sock, &raw_pid) ||
      raw_sock < 0) {
    VLOG(1) << "Crashpad handler socket is not available.";
    return false;
  }

  socket = base::ScopedFD(HANDLE_EINTR(fcntl(raw_sock, F_DUPFD_CLOEXEC, 0)));
  if (!socket.is_valid()) {
    PLOG(ERROR) << "Failed to dup Crashpad handler socket";
    return false;
  }

  pid = raw_pid;
  return true;
}

// static
base::FilePath CrashpadLinux::GetConfigFilePath() {
  return GetMultiProcessHostGlobalConfigDir().Append(kCrashpadConfigFileName);
}

// static
CrashpadLinux::Config CrashpadLinux::ReadConfig() {
  return ReadAndValidateConfigFile(GetConfigFilePath(),
                                   /*expected_owner_uid=*/0);
}

// static
CrashpadLinux::Config CrashpadLinux::ReadConfigFileForTesting(
    const base::FilePath& path,
    uid_t expected_owner_uid) {
  return ReadAndValidateConfigFile(path, expected_owner_uid);
}

// static
bool CrashpadLinux::GetCrashpadHandlerPath(base::FilePath* handler_path) {
  if (!base::PathService::Get(base::DIR_EXE, handler_path)) {
    LOG(ERROR) << "Unable to get exe dir for crashpad handler";
    return false;
  }
  *handler_path = handler_path->Append(kChromotingCrashpadHandler);
  return true;
}

}  // namespace remoting
