// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content_shell" command-line switches.

#ifndef CONTENT_SHELL_COMMON_SHELL_SWITCHES_H_
#define CONTENT_SHELL_COMMON_SHELL_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

// Makes Content Shell use the given path for its data directory.
//
// NOTE: "user-data-dir" is used to align with Chromedriver's behavior. Please
// do NOT change this to another value.
//
// NOTE: The same value is also used at Java-side in
// ContentShellBrowserTestActivity.java#getUserDataDirectoryCommandLineSwitch().
inline constexpr char kContentShellUserDataDir[] = "user-data-dir";

// The directory Crashpad should store minidumps in.
//
// iOS and tvOS default to app's cache directory.
inline constexpr char kCrashDumpsDir[] = "crash-dumps-dir";

// Disables the check for the system font when specified.
inline constexpr char kDisableSystemFontCheck[] = "disable-system-font-check";

// Exposes the window.internals object to JavaScript for interactive development
// and debugging of web tests that rely on it.
inline constexpr char kExposeInternalsForTesting[] =
    "expose-internals-for-testing";

// Size for the content_shell's host window (i.e. "800x600").
inline constexpr char kContentShellHostWindowSize[] =
    "content-shell-host-window-size";

// Hides toolbar from content_shell's host window.
inline constexpr char kContentShellHideToolbar[] = "content-shell-hide-toolbar";

// Enables APIs guarded with the [IsolatedContext] IDL attribute for the given
// comma-separated list of origins.
inline constexpr char kIsolatedContextOrigins[] = "isolated-context-origins";

// Use the given address instead of the default loopback for accepting remote
// debugging connections. Note that the remote debugging protocol does not
// perform any authentication, so exposing it too widely can be a security
// risk.
inline constexpr char kRemoteDebuggingAddress[] = "remote-debugging-address";

// Runs Content Shell in web test mode, injecting test-only behaviour for
// blink web tests.
inline constexpr char kRunWebTests[] = "run-web-tests";

// Register the provided scheme as a standard scheme.
inline constexpr char kTestRegisterStandardScheme[] =
    "test-register-standard-scheme";

// Helper that returns true if kRunWebTests is present in the command line,
// meaning Content Shell is running in web test mode.
bool IsRunWebTestsSwitchPresent();

}  // namespace switches

#endif  // CONTENT_SHELL_COMMON_SHELL_SWITCHES_H_
