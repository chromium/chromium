// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_CONSUMER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_consumer.h"

@class TableViewItem;

// Consumer protocol for Price Notifications settings.
@protocol PriceNotificationsConsumer <ChromeTableViewConsumer>

// Initializes price tracking item.
- (void)setPriceTrackingItem:(TableViewItem*)priceTrackingItem;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_CONSUMER_H_
