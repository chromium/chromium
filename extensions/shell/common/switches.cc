// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/switches.h"

namespace extensions {
namespace switches {

#if defined(OS_CHROMEOS)
// Allow roaming in the cellular network.
const char kAppShellAllowRoaming[] = "app-shell-allow-roaming";

// Size for the host window to create (i.e. "800x600").
const char kAppShellHostWindowSize[] = "app-shell-host-window-size";

// SSID of the preferred WiFi network.
const char kAppShellPreferredNetwork[] = "app-shell-preferred-network";
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// The directory breakpad should store minidumps in.
const char kCrashDumpsDir[] = "crash-dumps-dir";

// Enables metrics and crash reporting.
const char kEnableReporting[] = "enable-reporting";
#endif

}  // namespace switches
}  // namespace extensions
