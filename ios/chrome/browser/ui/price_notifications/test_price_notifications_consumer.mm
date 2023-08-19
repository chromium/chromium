// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/test_price_notifications_consumer.h"

@implementation TestPriceNotificationsConsumer

- (void)setTrackableItem:(PriceNotificationsTableViewItem*)trackableItem
       currentlyTracking:(BOOL)currentlyTracking {
  self.didExecuteAction = YES;
  self.trackableItem = trackableItem;
  self.isCurrentlyTrackingVisibleProduct = currentlyTracking;
}

- (void)addTrackedItem:(PriceNotificationsTableViewItem*)trackedItem
           toBeginning:(BOOL)beginning {
}

- (void)didStartPriceTrackingForItem:
    (PriceNotificationsTableViewItem*)trackableItem {
}

- (void)didStopPriceTrackingItem:(PriceNotificationsTableViewItem*)trackedItem
                   onCurrentSite:(BOOL)isViewingProductSite {
  self.didExecuteAction = YES;
  if (isViewingProductSite) {
    self.trackableItem = trackedItem;
  }
}

- (void)resetPriceTrackingItem:(PriceNotificationsTableViewItem*)item {
}

- (void)reconfigureCellsForItems:(NSArray*)items {
}

- (void)reloadCellsForItems:(NSArray*)items
           withRowAnimation:(UITableViewRowAnimation)rowAnimation {
}

@end
