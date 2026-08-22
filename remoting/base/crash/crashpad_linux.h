// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_
#define REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_

#include <sys/types.h>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"

namespace remoting {

class CrashpadLinux {
 public:
  CrashpadLinux() = delete;
  CrashpadLinux(const CrashpadLinux&) = delete;
  CrashpadLinux& operator=(const CrashpadLinux&) = delete;

  // Initializes Crashpad in the supervisor / host process by spawning the
  // Crashpad handler subprocess.
  static bool Initialize();

  // Initializes Crashpad in a client / worker process by connecting the
  // Crashpad signal handler to an inherited or received handler socket.
  static bool InitializeClient(base::ScopedFD handler_socket,
                               pid_t handler_pid);

  // Gets a duplicated `base::ScopedFD` connected to the Crashpad handler and
  // the handler PID. Returns true if the handler socket was retrieved
  // successfully.
  static bool GetHandlerSocket(base::ScopedFD& socket, pid_t& pid);

 private:
  static bool GetCrashpadHandlerPath(base::FilePath* handler_path);
};

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_
