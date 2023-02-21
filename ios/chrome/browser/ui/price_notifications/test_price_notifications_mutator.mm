// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/test_price_notifications_mutator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
