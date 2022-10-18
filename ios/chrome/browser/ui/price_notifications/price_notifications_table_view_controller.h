// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/price_notifications/price_notifications_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// View controller that displays PriceNotifications list items in a table view.
@interface PriceNotificationsTableViewController
    : ChromeTableViewController <PriceNotificationsConsumer>

@end

#endif  // IOS_CHROME_BROWSER_UI_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_TABLE_VIEW_CONTROLLER_H_
