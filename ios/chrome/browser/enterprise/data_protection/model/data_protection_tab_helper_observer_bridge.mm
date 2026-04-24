// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_tab_helper_observer_bridge.h"

DataProtectionTabHelperObserverBridge::DataProtectionTabHelperObserverBridge(
    id<DataProtectionTabHelperObserving> observer,
    DataProtectionTabHelper* tab_helper)
    : observer_(observer) {
  if (tab_helper) {
    scoped_observation_.Observe(tab_helper);
  }
}

DataProtectionTabHelperObserverBridge::
    ~DataProtectionTabHelperObserverBridge() = default;

void DataProtectionTabHelperObserverBridge::ScreenshotProtectionDidChange(
    web::WebState* web_state,
    bool screenshot_protection_enabled) {
  if ([observer_ respondsToSelector:@selector
                 (screenshotProtectionDidChangeForWebState:isProtected:)]) {
    [observer_
        screenshotProtectionDidChangeForWebState:web_state
                                     isProtected:screenshot_protection_enabled];
  }
}

void DataProtectionTabHelperObserverBridge::DataProtectionTabHelperDestroyed(
    DataProtectionTabHelper* helper) {
  scoped_observation_.Reset();
}
