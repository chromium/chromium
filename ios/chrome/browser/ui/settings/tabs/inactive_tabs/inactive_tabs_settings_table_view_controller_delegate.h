// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_TABLE_VIEW_CONTROLLER_DELEGATE_H_

@class InactiveTabsSettingsTableViewController;

// Delegate for events related to Inactive Tabs Settings View Controller.
@protocol InactiveTabsSettingsTableViewControllerDelegate <NSObject>

// Sends `threshold` to the model to handle logic and navigation.
- (void)inactiveTabsSettingsTableViewController:
            (InactiveTabsSettingsTableViewController*)
                inactiveTabsSettingsTableViewController
                 didSelectInactiveDaysThreshold:(int)threshold;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
