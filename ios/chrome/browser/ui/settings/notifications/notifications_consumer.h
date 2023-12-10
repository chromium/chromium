// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_consumer.h"

@class TableViewItem;

// Consumer protocol for Notifications settings.
@protocol NotificationsConsumer <LegacyChromeTableViewConsumer>

// Initializes price tracking item.
- (void)setPriceTrackingItem:(TableViewItem*)priceTrackingItem;

// Initializes the content notifications item.
- (void)setContentNotificationsItem:(TableViewItem*)contentNotificationsItem;

// Initializes the content notifications footer item.
- (void)setContentNotificationsFooterItem:
    (TableViewHeaderFooterItem*)contentNotificationsFooterItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_CONSUMER_H_
