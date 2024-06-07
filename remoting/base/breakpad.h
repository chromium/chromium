// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_BREAKPAD_H_
#define REMOTING_BASE_BREAKPAD_H_

#include <string>

#include "build/build_config.h"
#include "build/buildflag.h"

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
void InitializeCrashReporting();

#if BUILDFLAG(IS_WIN)
// Initializes a client for out-of-process (OOP) crash reporting using the
// server process which owns the pipe referenced by |crash_server_pipe_handle|.
// This is used for processes which do not have permission to write to the
// file system. Any number of OOP crash clients can be active at a given time.
// The crash server must be available before calling this method, otherwise
// crash dumps will not be generated.
void InitializeOopCrashClient(const std::string& crash_server_pipe_handle);
// Initializes the server for out-of-process (OOP) crash reporting. Note that
// only one instance of this class should be running on the machine at a time,
// most likely the Daemon process, otherwise named pipe creation will fail.
void InitializeOopCrashServer();
#endif

}  // namespace remoting

#endif  // REMOTING_BASE_BREAKPAD_H_
