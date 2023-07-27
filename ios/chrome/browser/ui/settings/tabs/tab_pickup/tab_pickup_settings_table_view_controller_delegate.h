// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_TABLE_VIEW_CONTROLLER_DELEGATE_H_

@class TabPickupSettingsTableViewController;

// Delegate for events related to tab pickup settings view controller.
@protocol TabPickupSettingsTableViewControllerDelegate <NSObject>

// Sends the `enabled` state of the tab pickup feature to the model.
- (void)tabPickupSettingsTableViewController:
            (TabPickupSettingsTableViewController*)
                tabPickupSettingsTableViewController
                          didEnableTabPickup:(bool)enabled;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
