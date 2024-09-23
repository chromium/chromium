// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_observer_bridge.h"

InfobarBadgeTabHelperObserverBridge::InfobarBadgeTabHelperObserverBridge(
    id<InfobarBadgeTabHelperObserving> observer)
    : observer_(observer) {}

InfobarBadgeTabHelperObserverBridge::~InfobarBadgeTabHelperObserverBridge() =
    default;

void InfobarBadgeTabHelperObserverBridge::InfobarBadgesUpdated(
    InfobarBadgeTabHelper* tab_helper) {
  if ([observer_ respondsToSelector:@selector(infobarBadgesUpdated:)]) {
    [observer_ infobarBadgesUpdated:tab_helper];
  }
}
