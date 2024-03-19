// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/common/web_test_switches.h"

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

// Specifies the path to a file containing a Chrome DevTools protocol log.
// Each line in the log file is expected to be a protocol message in the JSON
// format. The test runner will use this log file to script the backend for any
// inspector-protocol tests that run. Usually you would want to run a single
// test using the log to reproduce timeouts or crashes.
const char kInspectorProtocolLog[] = "inspector-protocol-log";

// Encode binary web test results (images, audio) using base64.
const char kEncodeBinary[] = "encode-binary";

// Disables the automatic origin isolation of web platform test domains.
// We normally origin-isolate them for better test coverage, but tests of opt-in
// origin isolation need to disable this.
const char kDisableAutoWPTOriginIsolation[] =
    "disable-auto-wpt-origin-isolation";

// Forces each web test to be run in a new BrowsingInstance. Required for origin
// isolation web tests where the BrowsingInstance retains state from origin
// isolation requests, but this flag may benefit other web tests.
const char kResetBrowsingInstanceBetweenTests[] =
    "reset-browsing-instance-between-tests";

// This makes us disable some web-platform runtime features so that we test
// content_shell as if it was a stable release. It is only followed when
// kRunWebTest is set. For the features' level, see
// third_party/blink/renderer/platform/RuntimeEnabledFeatures.md
const char kStableReleaseMode[] = "stable-release-mode";

// Disables the shell from beginning in headless mode. Tests will then attempt
// to use the hardware GPU for rendering. This is only followed when
// kRunWebTests is set.
const char kDisableHeadlessMode[] = "disable-headless-mode";

}  // namespace switches
