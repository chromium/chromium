// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASH_REPORTING_CRASHPAD_H_
#define REMOTING_BASE_CRASH_CRASH_REPORTING_CRASHPAD_H_

#include "build/build_config.h"
#include "build/buildflag.h"

namespace remoting {

// Query and log the entries in the crash database.
// This will also verify that crash entries were uploaded and will clean up
// old entries to help reduce disk space usage.
void LogAndCleanupCrashDatabase();

// Initializes collection and upload of crash reports. The caller has to ensure
// that the user has agreed to crash dump reporting.
//
// Crash reporting has to be initialized as early as possible (e.g. the first
// thing in main()) to catch crashes occurring during process startup.
// Crashes which occur during the global static construction phase will not
// be caught and reported. This should not be a problem as static non-POD
// objects are not allowed by the style guide and exceptions to this rule are
// rare.
void InitializeCrashpadReporting();

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_CRASH_REPORTING_CRASHPAD_H_
