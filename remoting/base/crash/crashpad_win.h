// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_WIN_H_
#define REMOTING_BASE_CRASH_CRASHPAD_WIN_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "remoting/base/crash/crashpad_database_manager.h"

namespace crashpad {
class CrashpadClient;
}  // namespace crashpad

namespace remoting {

// Matches the OFFICIAL_BUILD registry/directory branding split in
// remoting/base/branding.cc, remoting/base/host_settings_win.cc, and
// remoting/host/daemon_process_win.cc.
#if defined(OFFICIAL_BUILD)
inline constexpr wchar_t kCrashpadRegistryKeyPath[] =
    L"SOFTWARE\\Google\\Chrome Remote Desktop\\crashpad";
#else
inline constexpr wchar_t kCrashpadRegistryKeyPath[] =
    L"SOFTWARE\\Chromoting\\crashpad";
#endif
inline constexpr wchar_t kCrashpadUploadUrlValue[] = L"UploadUrl";
inline constexpr wchar_t kCrashpadDisableRateLimitValue[] = L"DisableRateLimit";

class CrashpadWin : CrashpadDatabaseManager::Logger {
 public:
  struct RegistryConfig {
    std::optional<std::string> upload_url;
    bool disable_rate_limit = false;
  };

  CrashpadWin(const CrashpadWin&) = delete;
  CrashpadWin& operator=(const CrashpadWin&) = delete;

  bool Initialize();
  bool InitializeClient(const std::wstring& handler_pipe);
  std::wstring GetHandlerIPCPipe() const;
  bool CanUnprivilegedClientsConnect() const;

  static CrashpadWin& GetInstance();

  // Reads and validates optional admin-controlled Crashpad overrides from
  // HKLM\...\crashpad using the default registry view of the process
  // (WOW6432Node for a 32-bit host on 64-bit Windows), matching how the
  // ...\logging and HostSettings keys are read. Unreadable keys or invalid
  // UploadUrl and DisableRateLimit values are logged and ignored.
  static RegistryConfig ReadRegistryConfig();

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
