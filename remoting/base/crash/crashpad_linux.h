// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_
#define REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_

#include <sys/types.h>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "remoting/base/crash/crashpad_database_manager.h"

namespace remoting {

class CrashpadLinux : CrashpadDatabaseManager::Logger {
 public:
  CrashpadLinux();

  CrashpadLinux(const CrashpadLinux&) = delete;
  CrashpadLinux& operator=(const CrashpadLinux&) = delete;

  ~CrashpadLinux();

  bool Initialize();

  // Initializes Crashpad in a client / worker process by connecting the
  // Crashpad signal handler to an inherited or received handler socket.
  static bool InitializeClient(base::ScopedFD handler_socket,
                               pid_t handler_pid);

  // Gets a duplicated ScopedFD connected to the Crashpad handler and the
  // handler PID. Returns true if the handler socket was retrieved successfully.
  static bool GetHandlerSocket(base::ScopedFD& socket, pid_t& pid);

  void LogAndCleanupCrashpadDatabase();

  static CrashpadLinux& GetInstance();

 private:
  bool GetCrashpadHandlerPath(base::FilePath* handler_path);

  // CrashpadDatabaseManager::Logger overrides
  void Log(std::string message) const override;
  void LogError(std::string message) const override;

  remoting::CrashpadDatabaseManager database_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_
