// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/material_components/app_bar_view_controller_presenting.h"
#import "ios/chrome/browser/ui/settings/settings_root_view_controlling.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// SettingsRootTableViewController is a base class for integrating UITableViews
// into the Settings UI.  It handles the configuration and display of the MDC
// AppBar.
@interface SettingsRootTableViewController
    : ChromeTableViewController<AppBarViewControllerPresenting,
                                SettingsRootViewControlling,
                                TableViewLinkHeaderFooterItemDelegate>
@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_H_
