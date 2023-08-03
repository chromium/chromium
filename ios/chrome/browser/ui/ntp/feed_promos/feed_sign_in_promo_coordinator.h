// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_PROMOS_FEED_SIGN_IN_PROMO_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_PROMOS_FEED_SIGN_IN_PROMO_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol FeedSignInPromoCoordinatorDelegate;

// Coordinator for feed Sign-in promo feature. This feature informs the user
// that they need to sign in to get personalized content.
@interface FeedSignInPromoCoordinator : ChromeCoordinator

// Delegate that is in charge of stopping the coordinator.
@property(nonatomic, weak) id<FeedSignInPromoCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_PROMOS_FEED_SIGN_IN_PROMO_COORDINATOR_H_
