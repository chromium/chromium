// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/common/shell_switches.h"

#include "base/command_line.h"

namespace switches {

// Makes Content Shell use the given path for its data directory.
// NOTE: If changing this value, change the corresponding Java-side value in
// ContentShellBrowserTestActivity.java#getUserDataDirectoryCommandLineSwitch()
// to match.
const char kContentShellDataPath[] = "data-path";

// The directory breakpad should store minidumps in.
const char kCrashDumpsDir[] = "crash-dumps-dir";

// Disables the check for the system font when specified.
const char kDisableSystemFontCheck[] = "disable-system-font-check";

// Exposes the window.internals object to JavaScript for interactive development
// and debugging of web tests that rely on it.
const char kExposeInternalsForTesting[] = "expose-internals-for-testing";

// Size for the content_shell's host window (i.e. "800x600").
const char kContentShellHostWindowSize[] = "content-shell-host-window-size";

// Hides toolbar from content_shell's host window.
const char kContentShellHideToolbar[] = "content-shell-hide-toolbar";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Let DevTools front-end talk to the target of type "tab" rather than
// "frame" when inspecting a WebContents.
const char kContentShellDevToolsTabTarget[] =
    "content-shell-devtools-tab-target";
#endif

// Enables APIs guarded with the [IsolatedContext] IDL attribute for the given
// comma-separated list of origins.
const char kIsolatedContextOrigins[] = "isolated-context-origins";

// Use the given address instead of the default loopback for accepting remote
// debugging connections. Note that the remote debugging protocol does not
// perform any authentication, so exposing it too widely can be a security
// risk.
const char kRemoteDebuggingAddress[] = "remote-debugging-address";

// Runs Content Shell in web test mode, injecting test-only behaviour for
// blink web tests.
const char kRunWebTests[] = "run-web-tests";

bool IsRunWebTestsSwitchPresent() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kRunWebTests);
}

}  // namespace switches
