// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_consumer.h"

@class PriceNotificationsTableViewItem;

// Consumer for the PriceNotifications UI.
@protocol PriceNotificationsConsumer <LegacyChromeTableViewConsumer>

// Displays the item that is available to be tracked on the current site.
- (void)setTrackableItem:(PriceNotificationsTableViewItem*)trackableItem
       currentlyTracking:(BOOL)currentlyTracking;

// Adds and displays an item to the UI that the user has chosen to price track
// across sites. If `toBeginning` is true, then the item will be added to the
// beginning of the list. Otherwise, the item will be appended to the end.
- (void)addTrackedItem:(PriceNotificationsTableViewItem*)trackedItem
           toBeginning:(BOOL)beginning;

// In the event that the trackable item was successfully subscribed to, this
// function moves the trackable item from its current section to the tracked
// section.
- (void)didStartPriceTrackingForItem:
    (PriceNotificationsTableViewItem*)trackableItem;

// In the event that the tracked item was successfully unsubscribed to, this
// function removes the tracked item from its current section. If the user is on
// the website of the product that they stopped price tracking, the item will
// instead be moved from the tracked section to trackable section.
- (void)didStopPriceTrackingItem:(PriceNotificationsTableViewItem*)trackedItem
                   onCurrentSite:(BOOL)isViewingProductSite;

// In the event that `item` was not able to be subscribed to, this function
// re-enables user interactions on the item's cell.
- (void)resetPriceTrackingItem:(PriceNotificationsTableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_CONSUMER_H_
