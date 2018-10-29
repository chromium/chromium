// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "layout_test" command-line switches.

#ifndef CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_SWITCHES_H_
#define CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_SWITCHES_H_

#include <string>
#include <vector>

#include "build/build_config.h"

namespace switches {

extern const char kAllowExternalPages[];
#if defined(OS_ANDROID)
extern const char kAndroidStderrPort[];
extern const char kAndroidStdinPort[];
extern const char kAndroidStdoutPort[];
#endif // defined(OS_ANDROID)
extern const char kCrashOnFailure[];
extern const char kCustomDevToolsFrontend[];
extern const char kDebugDevTools[];
extern const char kEnableAccelerated2DCanvas[];
extern const char kEnableFontAntialiasing[];
extern const char kAlwaysUseComplexText[];
extern const char kEnableLeakDetection[];
extern const char kEncodeBinary[];
extern const char kRunWebTests[];
extern const char kStableReleaseMode[];
extern const char kTestsInBlink[];
extern const char kEnableDisplayCompositorPixelDump[];

}  // namespace switches

#endif  // CONTENT_SHELL_COMMON_LAYOUT_TEST_LAYOUT_TEST_SWITCHES_H_
