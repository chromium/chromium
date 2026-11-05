// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_FAKE_LOCATION_BAR_BADGE_OBSERVER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_FAKE_LOCATION_BAR_BADGE_OBSERVER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/location_bar/badge/model/location_bar_badge_tab_helper_observer_bridge.h"

// Fake Objective-C observer implementing `LocationBarBadgeTabHelperObserving`.
@interface FakeLocationBarBadgeObserver
    : NSObject <LocationBarBadgeTabHelperObserving>

// The number of times `locationBarBadgeTabHelperDidUpdateDisplayedBadge:` was
// invoked.
@property(nonatomic, assign) int callCount;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_MODEL_FAKE_LOCATION_BAR_BADGE_OBSERVER_H_
