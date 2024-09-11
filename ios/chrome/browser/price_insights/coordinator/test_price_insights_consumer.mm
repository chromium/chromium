// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/coordinator/test_price_insights_consumer.h"

#import "ios/chrome/browser/price_insights/ui/price_insights_item.h"

namespace commerce {
enum class PriceBucket;
}

@implementation TestPriceInsightsConsumer

- (void)didStartPriceTrackingWithNotification:(BOOL)granted
                               showCompletion:(BOOL)showCompletion {
  self.didPriceTrack = YES;
}

- (void)didStopPriceTracking {
  self.didPriceUntrack = YES;
}

- (void)didStartNavigationToWebpageWithPriceBucket:
    (commerce::PriceBucket)bucket {
  self.didNavigateToWebpage = YES;
}

- (void)presentPushNotificationPermissionAlert {
  self.didPresentPushNotificationPermissionAlertForItem = YES;
}

- (void)presentStartPriceTrackingErrorSnackbar {
  self.didPresentStartPriceTrackingErrorSnackbarForItem = YES;
}

- (void)presentStopPriceTrackingErrorSnackbar {
  self.didPresentStopPriceTrackingErrorSnackbarForItem = YES;
}

@end
