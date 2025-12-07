// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_
#define REMOTING_BASE_CRASH_CRASHPAD_LINUX_H_

#include "base/files/file_path.h"
#include "remoting/base/crash/crashpad_database_manager.h"

namespace remoting {

class CrashpadLinux : CrashpadDatabaseManager::Logger {
 public:
  CrashpadLinux();

  CrashpadLinux(const CrashpadLinux&) = delete;
  CrashpadLinux& operator=(const CrashpadLinux&) = delete;

  bool Initialize();
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
