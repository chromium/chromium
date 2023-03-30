// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_consumer.h"

@protocol InactiveTabsSettingsTableViewControllerDelegate;

// Class that handles the UI for options that require four states:  'Never
// Move', 'After 7 Days', 'After 14 Days' and 'After 21 Days'.
@interface InactiveTabsSettingsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol,
                                       InactiveTabsSettingsConsumer>

// The delegate receives events related to this view controller.
@property(nonatomic, weak) id<InactiveTabsSettingsTableViewControllerDelegate>
    delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_INACTIVE_TABS_INACTIVE_TABS_SETTINGS_TABLE_VIEW_CONTROLLER_H_
