// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/coordinator/test_price_insights_consumer.h"

#import "ios/chrome/browser/price_insights/ui/price_insights_item.h"

namespace commerce {
enum class PriceBucket;
}

@implementation TestPriceInsightsConsumer

- (void)didStartPriceTrackingWithNotification:(BOOL)granted {
  self.didPriceTrack = YES;
}

- (void)didStopPriceTracking {
  self.didPriceUntrack = YES;
}

- (void)didStartNavigationToWebpageWithPriceBucket:
    (commerce::PriceBucket)bucket {
  self.didNavigateToWebpage = YES;
}

- (void)presentPushNotificationPermissionAlertForItem:(PriceInsightsItem*)item {
  self.didPresentPushNotificationPermissionAlertForItem = YES;
}

- (void)presentStartPriceTrackingErrorAlertForItem:(PriceInsightsItem*)item {
  self.didPresentStartPriceTrackingErrorAlertForItem = YES;
}

- (void)presentStopPriceTrackingErrorAlertForItem:(PriceInsightsItem*)item {
  self.didPresentStopPriceTrackingErrorAlertForItem = YES;
}

@end
