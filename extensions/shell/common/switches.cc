// Copyright 2014 The Chromium Authors
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

}  // namespace switches
}  // namespace extensions
