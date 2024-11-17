// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class Browser;

// This class is the TableView for the application settings.
@interface SettingsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// Initializes a new SettingsTableViewController. `browser` must not
// be nil and must not be associated with an off the record profile.
- (instancetype)initWithBrowser:(Browser*)browser
       hasDefaultBrowserBlueDot:(BOOL)hasDefaultBrowserBlueDot
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_TABLE_VIEW_CONTROLLER_H_
