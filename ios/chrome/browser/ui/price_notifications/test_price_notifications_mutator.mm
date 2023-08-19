// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/test_price_notifications_mutator.h"

@implementation TestPriceNotificationsMutator

- (void)trackItem:(PriceNotificationsTableViewItem*)item {
}

- (void)stopTrackingItem:(PriceNotificationsTableViewItem*)item {
}

- (void)navigateToWebpageForItem:(PriceNotificationsTableViewItem*)item {
  self.didNavigateToItemPage = YES;
}

- (void)navigateToBookmarks {
}

@end
