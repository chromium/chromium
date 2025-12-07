// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_TEST_PRICE_INSIGHTS_CONSUMER_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_TEST_PRICE_INSIGHTS_CONSUMER_H_

#import "ios/chrome/browser/price_insights/coordinator/price_insights_consumer.h"

@interface TestPriceInsightsConsumer : NSObject <PriceInsightsConsumer>

// Indicates whether the mediator successfully tracked the product url.
@property(nonatomic, assign) BOOL didPriceTrack;

// Indicates whether the mediator successfully untracked the product url.
@property(nonatomic, assign) BOOL didPriceUntrack;

// Indicates whether the mediator successfully nagivated to the given webpage.
@property(nonatomic, assign) BOOL didNavigateToWebpage;

// Indicates whether the mediator unsuccessfully tracked the product url and
// presents an error alert.
@property(nonatomic, assign)
    BOOL didPresentStartPriceTrackingErrorSnackbarForItem;

// Indicates whether the mediator unsuccessfully untracked the product url and
// presents an error alert.
@property(nonatomic, assign)
    BOOL didPresentStopPriceTrackingErrorSnackbarForItem;

// Indicates whether the mediator successfully showed the notification prompt.
@property(nonatomic, assign)
    BOOL didPresentPushNotificationPermissionAlertForItem;

@end

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_TEST_PRICE_INSIGHTS_CONSUMER_H_
