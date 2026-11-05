// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper_observer_bridge.h"

#import "base/check.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper.h"

LocationBarBadgeTabHelperObserverBridge::
    LocationBarBadgeTabHelperObserverBridge(
        LocationBarBadgeTabHelper* tab_helper,
        id<LocationBarBadgeTabHelperObserving> observer)
    : observer_(observer) {
  CHECK(tab_helper);
  CHECK(observer_);
  scoped_observation_.Observe(tab_helper);
}

LocationBarBadgeTabHelperObserverBridge::
    ~LocationBarBadgeTabHelperObserverBridge() = default;

void LocationBarBadgeTabHelperObserverBridge::OnDisplayedBadgeChanged(
    LocationBarBadgeTabHelper* tab_helper) {
  id<LocationBarBadgeTabHelperObserving> observer = observer_;
  if ([observer
          respondsToSelector:
              @selector(locationBarBadgeTabHelperDidUpdateDisplayedBadge:)]) {
    [observer locationBarBadgeTabHelperDidUpdateDisplayedBadge:tab_helper];
  }
}

void LocationBarBadgeTabHelperObserverBridge::
    LocationBarBadgeTabHelperDestroyed(LocationBarBadgeTabHelper* tab_helper) {
  scoped_observation_.Reset();
}
