// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_BANDWIDTH_MANAGEMENT_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_BANDWIDTH_MANAGEMENT_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// Controller for the UI that allows the user to change settings that affect
// bandwidth usage: prefetching and the data reduction proxy.
@interface BandwidthManagementTableViewController
    : SettingsRootTableViewController

// The designated initializer. |browserState| must not be nil.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_BANDWIDTH_MANAGEMENT_TABLE_VIEW_CONTROLLER_H_
