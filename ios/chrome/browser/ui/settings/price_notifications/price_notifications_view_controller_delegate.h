// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_VIEW_CONTROLLER_DELEGATE_H_

@class TableViewItem;

// Delegate for PriceNotificationsViewController instance to manage the
// model.
@protocol PriceNotificationsViewControllerDelegate <NSObject>

// Sends `item` to the model to handle logic and navigation.
- (void)didSelectItem:(TableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRICE_NOTIFICATIONS_PRICE_NOTIFICATIONS_VIEW_CONTROLLER_DELEGATE_H_
