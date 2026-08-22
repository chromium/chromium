// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crash_reporting_crashpad.h"

#if BUILDFLAG(IS_WIN)
#include "remoting/base/crash/crashpad_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
#include "remoting/base/crash/crashpad_linux.h"
#endif  // BUILDFLAG(IS_LINUX)

namespace remoting {

// Not implemented for Mac, see https://crbug.com/714714
void InitializeCrashpadReporting() {
  // Touch the object to make sure it is initialized.
#if BUILDFLAG(IS_WIN)
  CrashpadWin::GetInstance().Initialize();
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX)
  CrashpadLinux::Initialize();
#endif  // BUILDFLAG(IS_LINUX)
}

#if BUILDFLAG(IS_LINUX)
bool InitializeCrashpadClient(base::ScopedFD handler_socket,
                              pid_t handler_pid) {
  return CrashpadLinux::InitializeClient(std::move(handler_socket),
                                         handler_pid);
}

bool GetCrashpadHandlerSocket(base::ScopedFD& socket, pid_t& pid) {
  return CrashpadLinux::GetHandlerSocket(socket, pid);
}
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace remoting
