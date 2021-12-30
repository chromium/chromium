// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/switches.h"

#include "build/chromeos_buildflags.h"

namespace extensions {
namespace switches {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Allow roaming in the cellular network.
const char kAppShellAllowRoaming[] = "app-shell-allow-roaming";

// Size for the host window to create (i.e. "800x600").
const char kAppShellHostWindowSize[] = "app-shell-host-window-size";

// SSID of the preferred WiFi network.
const char kAppShellPreferredNetwork[] = "app-shell-preferred-network";
#endif

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
// The directory breakpad should store minidumps in.
const char kCrashDumpsDir[] = "crash-dumps-dir";

// Enables metrics and crash reporting.
const char kEnableReporting[] = "enable-reporting";
#endif

}  // namespace switches
}  // namespace extensions
