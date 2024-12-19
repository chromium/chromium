// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_CRASH_CRASHPAD_H_
#define REMOTING_BASE_CRASH_CRASHPAD_H_

namespace remoting {

// Initializes collection and upload of crash reports. This will only be
// called if the user has already opted in to having their crash dumps
// uploaded.
//
// Crash reporting has to be initialized as early as possible (e.g. the first
// thing in main()) to catch crashes occurring during process startup.
// Crashes which occur during the global static construction phase will not
// be caught and reported. This should not be a problem as static non-POD
// objects are not allowed by the style guide and exceptions to this rule are
// rare.
void InitializeCrashReporting();

}  // namespace remoting

#endif  // REMOTING_BASE_CRASH_CRASHPAD_H_
