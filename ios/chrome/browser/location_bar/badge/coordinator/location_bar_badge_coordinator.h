// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_COORDINATOR_H_

#import "ios/chrome/browser/location_bar/badge/coordinator/location_bar_badge_mediator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// Coordinator for the location bar badge.
@interface LocationBarBadgeCoordinator : ChromeCoordinator

// The mediator for location bar badge.
@property(nonatomic, strong) LocationBarBadgeMediator* mediator;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_COORDINATOR_H_
