// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

// The accessibility identifier of the view controller's view.
extern NSString* const kSettingsAccountsTableViewId;
// The accessibility identifier of the add account cell.
extern NSString* const kSettingsAccountsTableViewAddAccountCellId;
// The accessibility identifier of the signout cell.
extern NSString* const kSettingsAccountsTableViewSignoutCellId;
// The accessibility identifier of the sync account cell.
extern NSString* const kSettingsAccountsTableViewSyncCellId;

@protocol ApplicationCommands;
@protocol ApplicationSettingsCommands;
class Browser;

// TableView that handles the settings for accounts when the user is signed in
// to Chrome.
@interface AccountsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// |browser| must not be nil.
// If |closeSettingsOnAddAccount| is YES, then this account table view
// controller will close the settings view when an account is added.
- (instancetype)initWithBrowser:(Browser*)browser
      closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
