// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crashpad_linux.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/time/time.h"
#include "remoting/base/crash/crashpad_database_manager.h"
#include "remoting/base/file_path_util_linux.h"
#include "remoting/base/logging.h"
#include "remoting/base/passwd_utils.h"
#include "remoting/base/username.h"
#include "remoting/base/version.h"
#include "third_party/crashpad/crashpad/client/crashpad_client.h"

namespace remoting {

namespace {

constexpr char kChromotingCrashpadHandler[] = "crashpad-handler";
constexpr char kDefaultCrashpadUploadUrl[] =
    "https://clients2.google.com/cr/report";

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

  std::vector<std::string> arguments;
  // Make sure Crashpad's generate_dump tool includes monitor-self annotations.
  // This creates a second crashpad instance that monitors the handler so it can
  // report crashes in the handler.
  arguments.push_back("--monitor-self-annotation=ptype=crashpad-handler");

  base::FilePath handler_path;
  if (!GetCrashpadHandlerPath(&handler_path)) {
    return false;
  }

  crashpad::CrashpadClient client;
  if (!client.StartHandler(handler_path, GetCrashpadDatabasePath(),
                           metrics_path, kDefaultCrashpadUploadUrl, annotations,
                           arguments, false, false)) {
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
bool CrashpadLinux::GetCrashpadHandlerPath(base::FilePath* handler_path) {
  if (!base::PathService::Get(base::DIR_EXE, handler_path)) {
    LOG(ERROR) << "Unable to get exe dir for crashpad handler";
    return false;
  }
  *handler_path = handler_path->Append(kChromotingCrashpadHandler);
  return true;
}

}  // namespace remoting
