// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_consumer.h"

@class TableViewItem;
@class TableViewHeaderFooterItem;

// Consumer protocol for Notifications settings.
@protocol NotificationsConsumer <LegacyChromeTableViewConsumer>

// Initializes price tracking item.
- (void)setPriceTrackingItem:(TableViewItem*)priceTrackingItem;

// Initializes the content notifications item.
- (void)setContentNotificationsItem:(TableViewItem*)contentNotificationsItem;

// Initializes the tips notifications item.
- (void)setTipsNotificationsItem:(TableViewItem*)tipsNotificationsItem;

// Initializes the Safety Check notifications item.
- (void)setSafetyCheckItem:(TableViewItem*)safetyCheckItem;

// Initializes the tips notifications footer item.
- (void)setTipsNotificationsFooterItem:
    (TableViewHeaderFooterItem*)tipsNotificationsFooterItem;

// Initializes the send tab notifications item.
- (void)setSendTabNotificationsItem:(TableViewItem*)sendTabNotificationsItem;

// Called when an item is updated and needs to be reloaded.
- (void)reloadData;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_NOTIFICATIONS_CONSUMER_H_
