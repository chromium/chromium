// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

@protocol ApplicationCommands;
class Browser;
@protocol BrowserCommands;
@protocol SettingsMainPageCommands;
@class SigninInteractionController;

// The accessibility identifier of the settings TableView.
extern NSString* const kSettingsTableViewId;

// The accessibility identifier of the sign in cell.
extern NSString* const kSettingsSignInCellId;

// The accessibility identifier of the account cell.
extern NSString* const kSettingsAccountCellId;

// The accessibility identifier of the Search Engine cell.
extern NSString* const kSettingsSearchEngineCellId;

// The accessibility identifier of the Voice Search cell.
extern NSString* const kSettingsVoiceSearchCellId;

// This class is the TableView for the application settings.
@interface SettingsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// Dispatcher for SettingsMainPageCommands. Defaults to self if not set.
// TODO(crbug.com/738881): Unify this with the dispatcher passed into the init.
@property(weak, nonatomic) id<SettingsMainPageCommands>
    settingsMainPageDispatcher;

// Initializes a new SettingsTableViewController. |browser| must not
// be nil and must not be associated with an off the record browser state.
- (instancetype)initWithBrowser:(Browser*)browser
                     dispatcher:
                         (id<ApplicationCommands, BrowserCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_TABLE_VIEW_CONTROLLER_H_
