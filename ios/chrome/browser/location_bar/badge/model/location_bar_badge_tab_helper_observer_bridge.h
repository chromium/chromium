// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_TAB_HELPER_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_TAB_HELPER_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/scoped_observation.h"
#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper_observer.h"

class LocationBarBadgeTabHelper;

// Protocol for Objective-C objects observing `LocationBarBadgeTabHelper`.
@protocol LocationBarBadgeTabHelperObserving <NSObject>
@optional

// Invoked when the displayed badge for `tabHelper` changes.
- (void)locationBarBadgeTabHelperDidUpdateDisplayedBadge:
    (LocationBarBadgeTabHelper*)tabHelper;

@end

// Bridge to forward `LocationBarBadgeTabHelperObserver` notifications to an
// Objective-C observer.
class LocationBarBadgeTabHelperObserverBridge
    : public LocationBarBadgeTabHelperObserver {
 public:
  // `tab_helper` must be non-null and must outlive this bridge. `observer` must
  // be non-null and is not retained.
  LocationBarBadgeTabHelperObserverBridge(
      LocationBarBadgeTabHelper* tab_helper,
      id<LocationBarBadgeTabHelperObserving> observer);
  ~LocationBarBadgeTabHelperObserverBridge() override;

  LocationBarBadgeTabHelperObserverBridge(
      const LocationBarBadgeTabHelperObserverBridge&) = delete;
  LocationBarBadgeTabHelperObserverBridge& operator=(
      const LocationBarBadgeTabHelperObserverBridge&) = delete;

  // `LocationBarBadgeTabHelperObserver`:
  void OnDisplayedBadgeChanged(LocationBarBadgeTabHelper* tab_helper) override;
  void LocationBarBadgeTabHelperDestroyed(
      LocationBarBadgeTabHelper* tab_helper) override;

 private:
  // Objective-C delegate notified of badge updates.
  __weak id<LocationBarBadgeTabHelperObserving> observer_ = nil;

  // Scoped observation managing the lifecycle of the C++ tab helper
  // observation. Declared after `observer_` so that the observation is
  // unregistered before the delegate is torn down.
  base::ScopedObservation<LocationBarBadgeTabHelper,
                          LocationBarBadgeTabHelperObserver>
      scoped_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_LOCATION_BAR_BADGE_TAB_HELPER_OBSERVER_BRIDGE_H_
