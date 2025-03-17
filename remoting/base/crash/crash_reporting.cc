// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crash_reporting.h"

#if BUILDFLAG(IS_WIN)
#include "remoting/base/crash/breakpad_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
#if defined(REMOTING_ENABLE_CRASHPAD)
#include "remoting/base/crash/crashpad_linux.h"
#else
#include "remoting/base/crash/breakpad_linux.h"
#endif  // REMOTING_ENABLE_CRASHPAD
#endif  // BUILDFLAG(IS_LINUX)

namespace remoting {

void LogAndCleanupCrashDatabase() {
#if BUILDFLAG(IS_LINUX) && defined(REMOTING_ENABLE_CRASHPAD)
  CrashpadLinux::GetInstance().LogAndCleanupCrashpadDatabase();
#endif  // BUILDFLAG(IS_LINUX) && REMOTING_ENABLE_CRASHPAD
}

// Not implemented for Mac, see https://crbug.com/714714
void InitializeCrashReporting() {
  // Touch the object to make sure it is initialized.
#if BUILDFLAG(IS_WIN)
  BreakpadWin::GetInstance().Initialize();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
#if defined(REMOTING_ENABLE_CRASHPAD)
  CrashpadLinux::GetInstance().Initialize();
#else
  BreakpadLinux::GetInstance();
#endif  // REMOTING_ENABLE_CRASHPAD
#endif  // BUILDFLAG(IS_LINUX)
}

#if BUILDFLAG(IS_WIN)
void InitializeOopCrashClient(const std::string& server_pipe_handle) {
  // Touch the object to make sure it is initialized.
  BreakpadWin::GetInstance().Initialize(server_pipe_handle);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace remoting
