// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content_shell" command-line switches.

#ifndef CONTENT_SHELL_COMMON_SHELL_SWITCHES_H_
#define CONTENT_SHELL_COMMON_SHELL_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

extern const char kContentShellUserDataDir[];
extern const char kCrashDumpsDir[];
extern const char kDisableSystemFontCheck[];
extern const char kExposeInternalsForTesting[];
extern const char kContentShellHostWindowSize[];
extern const char kContentShellHideToolbar[];
extern const char kIsolatedContextOrigins[];
extern const char kRemoteDebuggingAddress[];
extern const char kRunWebTests[];
extern const char kTestRegisterStandardScheme[];

// Helper that returns true if kRunWebTests is present in the command line,
// meaning Content Shell is running in web test mode.
bool IsRunWebTestsSwitchPresent();

}  // namespace switches

#endif  // CONTENT_SHELL_COMMON_SHELL_SWITCHES_H_
