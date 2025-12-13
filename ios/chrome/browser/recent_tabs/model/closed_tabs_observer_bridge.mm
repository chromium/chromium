// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/recent_tabs/model/closed_tabs_observer_bridge.h"

namespace recent_tabs {

#pragma mark - ClosedTabsObserverBridge

ClosedTabsObserverBridge::ClosedTabsObserverBridge(
    id<ClosedTabsObserving> owner)
    : owner_(owner) {}

ClosedTabsObserverBridge::~ClosedTabsObserverBridge() {}

void ClosedTabsObserverBridge::TabRestoreServiceChanged(
    sessions::TabRestoreService* service) {
  [owner_ tabRestoreServiceChanged:service];
}

void ClosedTabsObserverBridge::TabRestoreServiceDestroyed(
    sessions::TabRestoreService* service) {
  [owner_ tabRestoreServiceDestroyed:service];
}

}  // namespace recent_tabs
