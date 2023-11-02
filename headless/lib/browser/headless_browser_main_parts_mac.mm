// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_browser_main_parts.h"

#import <Cocoa/Cocoa.h>

#include "headless/lib/browser/headless_shell_application_mac.h"
#include "services/device/public/cpp/geolocation/geolocation_manager_impl_mac.h"

namespace headless {

void HeadlessBrowserMainParts::PreCreateMainMessageLoop() {
  // Force hide dock and menu bar.
  [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
  if (!geolocation_manager_)
    geolocation_manager_ = device::GeolocationManagerImpl::Create();
}

}  // namespace headless
