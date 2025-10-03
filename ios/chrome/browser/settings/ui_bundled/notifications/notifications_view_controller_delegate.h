// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_NOTIFICATIONS_NOTIFICATIONS_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_NOTIFICATIONS_NOTIFICATIONS_VIEW_CONTROLLER_DELEGATE_H_

@class TableViewItem;
@class TableViewSwitchItem;

// Delegate for NotificationsViewController instance to manage the
// model.
@protocol NotificationsViewControllerDelegate <NSObject>

// Sends `item` to the model to handle logic and navigation.
- (void)didSelectItem:(TableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_NOTIFICATIONS_NOTIFICATIONS_VIEW_CONTROLLER_DELEGATE_H_
