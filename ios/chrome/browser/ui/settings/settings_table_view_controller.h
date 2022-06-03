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
@protocol BrowsingDataCommands;
@protocol SettingsMainPageCommands;
@class SigninInteractionController;

// This class is the TableView for the application settings.
@interface SettingsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// Initializes a new SettingsTableViewController. |browser| must not
// be nil and must not be associated with an off the record browser state.
- (instancetype)
    initWithBrowser:(Browser*)browser
         dispatcher:
             (id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>)
                 dispatcher NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_TABLE_VIEW_CONTROLLER_H_
