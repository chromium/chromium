// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/price_insights/coordinator/test_price_insights_consumer.h"

@implementation TestPriceInsightsConsumer

- (void)didStartPriceTrackingWithNotification:(BOOL)granted {
  self.didPriceTrack = YES;
}

- (void)didStopPriceTracking {
  self.didPriceUntrack = YES;
}

- (void)didStartNavigationToWebpage {
  self.didNavigateToWebpage = YES;
}

- (void)presentPushNotificationPermissionAlert {
}

- (void)presentStartPriceTrackingErrorAlertForItem:(PriceInsightsItem*)item {
  self.didPresentStartPriceTrackingErrorAlertForItem = YES;
}

- (void)presentStopPriceTrackingErrorAlertForItem:(PriceInsightsItem*)item {
  self.didPresentStopPriceTrackingErrorAlertForItem = YES;
}

@end
