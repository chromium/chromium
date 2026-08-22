// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASH_REPORTING_CRASHPAD_H_
#define REMOTING_BASE_CRASH_CRASH_REPORTING_CRASHPAD_H_

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_LINUX)
#include <sys/types.h>

#include "base/files/scoped_file.h"
#endif  // BUILDFLAG(IS_LINUX)

namespace remoting {


// Initializes collection and upload of crash reports. The caller has to ensure
// that the user has agreed to crash dump reporting.
//
// Crash reporting has to be initialized as early as possible (e.g. the first
// thing in main()) to catch crashes occurring during process startup.
// Crashes which occur during the global static construction phase will not
// be caught and reported. This should not be a problem as static non-POD
// objects are not allowed by the style guide and exceptions to this rule are
// rare.
//
// For Linux multi-process host, the daemon process should first call this
// method, then call GetCrashpadHandlerSocket() and pass the socket and PID to
// worker processes. Worker processes should then call
// InitializeCrashpadClient() instead of this method to initialize crashpad.
void InitializeCrashpadReporting();

#if BUILDFLAG(IS_LINUX)
// Initializes Crashpad in a client process using an inherited or received
// socket.
// You should not call this function on the process where
// InitializeCrashpadReporting() is called.
bool InitializeCrashpadClient(base::ScopedFD handler_socket, pid_t handler_pid);

// Gets a duplicated ScopedFD connected to the Crashpad handler and the handler
// PID. Returns true if the handler socket was retrieved successfully.
bool GetCrashpadHandlerSocket(base::ScopedFD& socket, pid_t& pid);
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_CRASH_REPORTING_CRASHPAD_H_
