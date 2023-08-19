// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_FEED_PROMOS_FEED_SIGN_IN_PROMO_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_NTP_FEED_PROMOS_FEED_SIGN_IN_PROMO_COORDINATOR_DELEGATE_H_

@class FeedSignInPromoCoordinator;

// Delegate for the feed sign in promo coordinator.
@protocol FeedSignInPromoCoordinatorDelegate <NSObject>

// Requests the delegate to stop the `coordinator`.
- (void)feedSignInPromoCoordinatorWantsToBeStopped:
    (FeedSignInPromoCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_FEED_PROMOS_FEED_SIGN_IN_PROMO_COORDINATOR_DELEGATE_H_
