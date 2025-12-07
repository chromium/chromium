// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_WIN_H_
#define REMOTING_BASE_CRASH_CRASHPAD_WIN_H_

#include "base/files/file_path.h"
#include "remoting/base/crash/crashpad_database_manager.h"

namespace remoting {

class CrashpadWin : CrashpadDatabaseManager::Logger {
 public:
  CrashpadWin();

  CrashpadWin(const CrashpadWin&) = delete;
  CrashpadWin& operator=(const CrashpadWin&) = delete;

  bool Initialize();
  void LogAndCleanupCrashpadDatabase();

  static CrashpadWin& GetInstance();

 private:
  bool GetCrashpadHandlerPath(base::FilePath* handler_path);

  // CrashpadDatabaseManager::Logger overrides
  void Log(std::string message) const override;
  void LogError(std::string message) const override;

  remoting::CrashpadDatabaseManager database_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_CRASHPAD_WIN_H_
