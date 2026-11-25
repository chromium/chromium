// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_
#define REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_

#include <stddef.h>
#include <sys/types.h>

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"

namespace remoting {

inline constexpr char kCrashpadConfigFileName[] = "crashpad.json";
inline constexpr char kCrashpadUploadUrlKey[] = "upload_url";
inline constexpr char kCrashpadDisableRateLimitKey[] = "disable_rate_limit";
inline constexpr size_t kMaxCrashpadConfigBytes = 16 * 1024;

class CrashpadLinux {
 public:
  struct Config {
    std::optional<std::string> upload_url;
    bool disable_rate_limit = false;
  };

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

  // Returns the path to the optional admin-controlled Crashpad override JSON
  // file (`/etc/chrome-remote-desktop/crashpad.json` in production).
  static base::FilePath GetConfigFilePath();

  // Reads and validates optional admin-controlled Crashpad overrides via
  // `ReadSecureFile()`. Both `/etc/chrome-remote-desktop` and `crashpad.json`
  // must be owned by root (UID 0), not symlinks, and not group- or
  // world-writable. `crashpad.json` should be mode 0644 (and its parent
  // directory 0755) so non-root standalone processes (e.g. the IT2Me native
  // messaging host) can also read the overrides. Invalid values or insecure
  // files are logged and ignored.
  static Config ReadConfig();

  static Config ReadConfigFileForTesting(const base::FilePath& path,
                                         uid_t expected_owner_uid);

 private:
  static bool GetCrashpadHandlerPath(base::FilePath* handler_path);
};

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_
