// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_consumer.h"

@class TableViewHeaderFooterItem;
@class TableViewItem;

// Consumer protocol for Content Notifications settings menu.
@protocol ContentNotificationsConsumer <LegacyChromeTableViewConsumer>

// Initializes the content notifications item.
- (void)setContentNotificationsItem:(TableViewItem*)contentNotificationsItem;

// Initializes the sports notifications item.
- (void)setSportsNotificationsItem:(TableViewItem*)sportsNotificationsItem;

// Initializes the content notifications footer item.
- (void)setContentNotificationsFooterItem:
    (TableViewHeaderFooterItem*)contentNotificationsFooterItem;

// Called when an item is updated and needs to be reloaded.
- (void)reloadData;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONSUMER_H_
