// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_VIEW_CONTROLLER_DELEGATE_H_

@class TableViewItem;
@class TableViewSwitchItem;

// Delegate for NotificationsViewController instance to manage the
// model.
@protocol ContentNotificationsViewControllerDelegate <NSObject>

// Sends switch toggle response to the model so that it can be updated.
- (void)didToggleSwitchItem:(TableViewSwitchItem*)item withValue:(BOOL)value;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_NOTIFICATIONS_CONTENT_NOTIFICATIONS_CONTENT_NOTIFICATIONS_VIEW_CONTROLLER_DELEGATE_H_
