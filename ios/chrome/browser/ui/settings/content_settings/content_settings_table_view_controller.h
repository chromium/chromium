// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_CONTENT_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_CONTENT_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class Browser;

// Controller for the UI that allows the user to change content settings like
// blocking popups.
@interface ContentSettingsTableViewController
    : SettingsRootTableViewController <SettingsControllerProtocol>

// The designated initializer. `browser` must not be null.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CONTENT_SETTINGS_CONTENT_SETTINGS_TABLE_VIEW_CONTROLLER_H_
