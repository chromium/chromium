// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_consumer.h"

@class PriceNotificationsTableViewItem;

// Consumer for the PriceNotifications UI.
@protocol PriceNotificationsConsumer <ChromeTableViewConsumer>

// Displays the item that is available to be tracked on the current site.
- (void)setTrackableItem:(PriceNotificationsTableViewItem*)trackableItem
       currentlyTracking:(BOOL)currentlyTracking;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_CONSUMER_H_
