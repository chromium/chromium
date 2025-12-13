// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol LocationBarBadgeCoordinatorDelegate;
@class IncognitoBadgeViewController;
@class LocationBarBadgeMediator;
@class LocationBarBadgeViewController;

// Coordinator for the leading badge in the location bar. Handles expanding the
// leading badge into a chip. Responsible for all leading badges and chips
// within the omnibox. Multiple features will send badge configurations to
// prompt badge updates within LocationBarBadge.
@interface LocationBarBadgeCoordinator : ChromeCoordinator

// The delegate for this coordinator.
@property(nonatomic, weak) id<LocationBarBadgeCoordinatorDelegate> delegate;

// The mediator for location bar badge.
@property(nonatomic, strong) LocationBarBadgeMediator* mediator;

// The view controller for this coordinator.
@property(nonatomic, strong) LocationBarBadgeViewController* viewController;

// Adds incognito badge view controller to LocationBarBadgeViewController.
- (void)addIncognitoBadgeViewController:
    (IncognitoBadgeViewController*)incognitoViewController;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_BADGE_COORDINATOR_LOCATION_BAR_BADGE_COORDINATOR_H_
