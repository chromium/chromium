// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace headless {

void HeadlessBrowserMainParts::PostCreateMainMessageLoop() {
#if defined(USE_DBUS) && !BUILDFLAG(IS_CHROMEOS_ASH)
  bluez::BluezDBusManager::Initialize(/*system_bus=*/nullptr);
#endif
}

}  // namespace headless
