// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_WIN_H_
#define REMOTING_BASE_CRASH_CRASHPAD_WIN_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "remoting/base/crash/crashpad_database_manager.h"

namespace crashpad {
class CrashpadClient;
}  // namespace crashpad

namespace remoting {

class CrashpadWin : CrashpadDatabaseManager::Logger {
 public:
  CrashpadWin(const CrashpadWin&) = delete;
  CrashpadWin& operator=(const CrashpadWin&) = delete;

  bool Initialize();
  bool InitializeClient(const std::wstring& handler_pipe);
  std::wstring GetHandlerIPCPipe() const;
  bool CanUnprivilegedClientsConnect() const;

  static CrashpadWin& GetInstance();

  static bool GrantLocalServiceAccessToHandlerPipeForTesting(
      const std::wstring& handler_ipc_pipe);

 private:
  friend class base::NoDestructor<CrashpadWin>;

  CrashpadWin();
  ~CrashpadWin() override;

  void ConfigureProcessExceptionHandling();

  bool GetCrashpadHandlerPath(base::FilePath* handler_path);

  // CrashpadDatabaseManager::Logger overrides
  void Log(std::string_view message) const override;
  void LogError(std::string_view message) const override;

  bool initialization_attempted_ = false;
  bool local_service_pipe_access_granted_ = false;
  std::unique_ptr<crashpad::CrashpadClient> crashpad_client_;
};

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_CRASHPAD_WIN_H_
