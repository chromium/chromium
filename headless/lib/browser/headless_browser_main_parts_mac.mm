// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#import <Cocoa/Cocoa.h>

#include "headless/lib/browser/headless_shell_application_mac.h"
#include "services/device/public/cpp/geolocation/system_geolocation_source_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace headless {

void HeadlessBrowserMainParts::PreCreateMainMessageLoop() {
  // Force hide dock and menu bar.
  NSApp.activationPolicy = NSApplicationActivationPolicyAccessory;
  if (!geolocation_manager_)
    geolocation_manager_ =
        device::SystemGeolocationSourceMac::CreateGeolocationManagerOnMac();
}

}  // namespace headless
