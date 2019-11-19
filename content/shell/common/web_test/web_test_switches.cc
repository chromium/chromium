// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/web_test/web_test_switches.h"

namespace switches {

// Allow access to external pages during web tests.
const char kAllowExternalPages[] = "allow-external-pages";

// When specified to "enable-leak-detection" command-line option,
// causes the leak detector to cause immediate crash when found leak.
const char kCrashOnFailure[] = "crash-on-failure";

// Run devtools tests in debug mode (not bundled and minified)
const char kDebugDevTools[] = "debug-devtools";

// Enable accelerated 2D canvas.
const char kEnableAccelerated2DCanvas[] = "enable-accelerated-2d-canvas";

// Enable font antialiasing for pixel tests.
const char kEnableFontAntialiasing[] = "enable-font-antialiasing";

// Always use the complex text path for web tests.
const char kAlwaysUseComplexText[] = "always-use-complex-text";

// Enables the leak detection of loading webpages. This allows us to check
// whether or not reloading a webpage releases web-related objects correctly.
const char kEnableLeakDetection[] = "enable-leak-detection";

// Encode binary web test results (images, audio) using base64.
const char kEncodeBinary[] = "encode-binary";

// Request the render trees of pages to be dumped as text once they have
// finished loading.
const char kRunWebTests[] = "run-web-tests";

// This makes us disable some web-platform runtime features so that we test
// content_shell as if it was a stable release. It is only followed when
// kRunWebTest is set. For the features' level, see
// http://dev.chromium.org/blink/runtime-enabled-features.
const char kStableReleaseMode[] = "stable-release-mode";

// Disables the shell from beginning in headless mode. Tests will then attempt
// to use the hardware GPU for rendering. This is only followed when
// kRunWebTests is set.
const char kDisableHeadlessMode[] = "disable-headless-mode";

}  // namespace switches
