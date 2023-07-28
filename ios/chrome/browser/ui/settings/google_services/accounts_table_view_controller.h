// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol ApplicationCommands;
@protocol ApplicationSettingsCommands;
class Browser;

// TableView that handles the settings for accounts when the user is signed in
// to Chrome.
@interface AccountsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// ApplicationCommands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;

// If YES, AccountsTableViewController should not dismiss itself only for a
// sign-out reason. The parent coordinator is responsible to dismiss this
// coordinator when a sign-out happens.
@property(nonatomic, assign) BOOL signoutDismissalByParentCoordinator;

// `browser` must not be nil.
// If `closeSettingsOnAddAccount` is YES, then this account table view
// controller will close the settings view when an account is added.
- (instancetype)initWithBrowser:(Browser*)browser
      closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_GOOGLE_SERVICES_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
