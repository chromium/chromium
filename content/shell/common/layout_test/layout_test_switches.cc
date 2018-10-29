// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/layout_test/layout_test_switches.h"

namespace switches {

// Allow access to external pages during layout tests.
const char kAllowExternalPages[] = "allow-external-pages";

#if defined(OS_ANDROID)
// Redirect stderr to the given port. Only supported on Android.
const char kAndroidStderrPort[] = "android-stderr-port";

// Redirect stdin to the given port. Only supported on Android.
const char kAndroidStdinPort[] = "android-stdin-port";

// Redirect stdout to the given port. Only supported on Android.
const char kAndroidStdoutPort[] = "android-stdout-port";
#endif // defined(OS_ANDROID)

// When specified to "enable-leak-detection" command-line option,
// causes the leak detector to cause immediate crash when found leak.
const char kCrashOnFailure[] = "crash-on-failure";

// Run devtools tests in debug mode (not bundled and minified)
const char kDebugDevTools[] = "debug-devtools";

// Specify a custom path to devtools for devtools tests
const char kCustomDevToolsFrontend[] = "custom-devtools-frontend";

// Enable accelerated 2D canvas.
const char kEnableAccelerated2DCanvas[] = "enable-accelerated-2d-canvas";

// Enable font antialiasing for pixel tests.
const char kEnableFontAntialiasing[] = "enable-font-antialiasing";

// Always use the complex text path for layout tests.
const char kAlwaysUseComplexText[] = "always-use-complex-text";

// Enables the leak detection of loading webpages. This allows us to check
// whether or not reloading a webpage releases web-related objects correctly.
const char kEnableLeakDetection[] = "enable-leak-detection";

// Encode binary layout test results (images, audio) using base64.
const char kEncodeBinary[] = "encode-binary";

// Request the render trees of pages to be dumped as text once they have
// finished loading.
const char kRunWebTests[] = "run-web-tests";

// This makes us disable some web-platform runtime features so that we test
// content_shell as if it was a stable release. It is only followed when
// kRunLayoutTest is set. For the features' level, see
// http://dev.chromium.org/blink/runtime-enabled-features.
const char kStableReleaseMode[] = "stable-release-mode";

// Test files are in //third_party/blink/web_tests, not in
// //third_party/WebKit/LayoutTests.
// TODO(tkent): Remove this flag after the move.
const char kTestsInBlink[] = "tests-in-blink";

// Enable pixel dumps via "real" surface readbacks, instead of synchronously
// compositing and reading back pixels.
const char kEnableDisplayCompositorPixelDump[] =
    "enable-display-compositor-pixel-dump";

}  // namespace switches
