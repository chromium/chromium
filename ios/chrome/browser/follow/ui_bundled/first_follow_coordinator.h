// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FOLLOW_UI_BUNDLED_FIRST_FOLLOW_COORDINATOR_H_
#define IOS_CHROME_BROWSER_FOLLOW_UI_BUNDLED_FIRST_FOLLOW_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class FollowedWebSite;

// Coordinator for the First Follow feature. This feature informs the user about
// the feed and following websites the first few times the user follows any
// channel.
@interface FirstFollowCoordinator : ChromeCoordinator

// Designated initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                           followedWebSite:(FollowedWebSite*)followedWebSite
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_FOLLOW_UI_BUNDLED_FIRST_FOLLOW_COORDINATOR_H_
