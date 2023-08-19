// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/tab_pickup/tab_pickup_settings_consumer.h"

@protocol TabPickupSettingsTableViewControllerDelegate;

// Controller for the UI that allows the user to update tab pickup settings.
@interface TabPickupSettingsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol,
                                       TabPickupSettingsConsumer>

// The delegate receives events related to this view controller.
@property(nonatomic, weak) id<TabPickupSettingsTableViewControllerDelegate>
    delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_TABS_TAB_PICKUP_TAB_PICKUP_SETTINGS_TABLE_VIEW_CONTROLLER_H_
