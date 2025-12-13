// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_TABS_TABS_SETTINGS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_TABS_TABS_SETTINGS_TABLE_VIEW_CONTROLLER_DELEGATE_H_

@class TabsSettingsTableViewController;

// Delegate for events related to Tabs Settings View Controller.
@protocol TabsSettingsTableViewControllerDelegate <NSObject>

// Tells to the model to handle logic and navigation for inactive tabs settings.
- (void)tabsSettingsTableViewControllerDidSelectInactiveTabsSettings:
    (TabsSettingsTableViewController*)tabsSettingsTableViewController;

// Tells to the model to handle logic that switch toggle for automatically open
// tab groups settings did change.
- (void)tabsSettingsTableViewController:
            (TabsSettingsTableViewController*)tabsSettingsTableViewController
             didUpdateAutoOpenTabGroups:(BOOL)autoOpenTabGroups;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_TABS_TABS_SETTINGS_TABLE_VIEW_CONTROLLER_DELEGATE_H_
