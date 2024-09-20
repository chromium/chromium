// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_CONSUMER_H_
#define IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_CONSUMER_H_

#import <UIKit/UIKit.h>

@class PriceInsightsItem;

namespace commerce {
enum class PriceBucket;
}

// Consumer for the Price Insights.
@protocol PriceInsightsConsumer <NSObject>

// Notifies the modulator that the user successfully tracked a price with or
// without notifications being granted by the user. It also can show a snackbar
// to inform the user of the completion of the tracking status.
- (void)didStartPriceTrackingWithNotification:(BOOL)granted
                               showCompletion:(BOOL)showCompletion;

// Notifies the modulator that the trackable item was successfully unsubscribed
// to.
- (void)didStopPriceTracking;

// Notifies the modulator that webpage navigation has started.
- (void)didStartNavigationToWebpageWithPriceBucket:
    (commerce::PriceBucket)bucket;

// Displays a UIAlert in the modulator that directs the user to the OS
// permission settings to enable push notification permissions.
- (void)presentPushNotificationPermissionAlert;

// Displays a snackbar in the modulator that indicates to the user that an error
// has occurred during the price tracking subscription process.
- (void)presentStartPriceTrackingErrorSnackbar;

// Displays a snackbar in the modulator that indicates to the user that an error
// has occurred during the price tracking subscription cancellation process.
- (void)presentStopPriceTrackingErrorSnackbar;

@end

#endif  // IOS_CHROME_BROWSER_PRICE_INSIGHTS_COORDINATOR_PRICE_INSIGHTS_CONSUMER_H_
