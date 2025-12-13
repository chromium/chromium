// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_COMMON_WEB_TEST_SWITCHES_H_
#define CONTENT_WEB_TEST_COMMON_WEB_TEST_SWITCHES_H_

namespace switches {

// Allow access to external pages during web tests.
inline constexpr char kAllowExternalPages[] = "allow-external-pages";

// When specified to "enable-leak-detection" command-line option,
// causes the leak detector to cause immediate crash when found leak.
inline constexpr char kCrashOnFailure[] = "crash-on-failure";

// Run devtools tests in debug mode (not bundled and minified)
inline constexpr char kDebugDevTools[] = "debug-devtools";

// Enable accelerated 2D canvas.
inline constexpr char kEnableAccelerated2DCanvas[] =
    "enable-accelerated-2d-canvas";

// Enable font antialiasing for pixel tests.
inline constexpr char kEnableFontAntialiasing[] = "enable-font-antialiasing";

// Always use the complex text path for web tests.
inline constexpr char kAlwaysUseComplexText[] = "always-use-complex-text";

// Enables the leak detection of loading webpages. This allows us to check
// whether or not reloading a webpage releases web-related objects correctly.
inline constexpr char kEnableLeakDetection[] = "enable-leak-detection";

// Specifies the path to a file containing a Chrome DevTools protocol log.
// Each line in the log file is expected to be a protocol message in the JSON
// format. The test runner will use this log file to script the backend for any
// inspector-protocol tests that run. Usually you would want to run a single
// test using the log to reproduce timeouts or crashes.
inline constexpr char kInspectorProtocolLog[] = "inspector-protocol-log";

// Encode binary web test results (images, audio) using base64.
inline constexpr char kEncodeBinary[] = "encode-binary";

// Disables the automatic origin isolation of web platform test domains.
// We normally origin-isolate them for better test coverage, but tests of opt-in
// origin isolation need to disable this.
inline constexpr char kDisableAutoWPTOriginIsolation[] =
    "disable-auto-wpt-origin-isolation";

// Forces each web test to be run in a new BrowsingInstance. Required for origin
// isolation web tests where the BrowsingInstance retains state from origin
// isolation requests, but this flag may benefit other web tests.
inline constexpr char kResetBrowsingInstanceBetweenTests[] =
    "reset-browsing-instance-between-tests";

// This makes us disable some web-platform runtime features so that we test
// content_shell as if it was a stable release. It is only followed when
// kRunWebTest is set. For the features' level, see
// third_party/blink/renderer/platform/RuntimeEnabledFeatures.md
inline constexpr char kStableReleaseMode[] = "stable-release-mode";

// Disables the shell from beginning in headless mode. Tests will then attempt
// to use the hardware GPU for rendering. This is only followed when
// kRunWebTests is set.
inline constexpr char kDisableHeadlessMode[] = "disable-headless-mode";

}  // namespace switches

#endif  // CONTENT_WEB_TEST_COMMON_WEB_TEST_SWITCHES_H_
