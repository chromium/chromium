// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_TOOLBAR_ADD_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_TOOLBAR_ADD_H_

#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

// This category adds a method to SettingsRootTableViewController to create
// an Add button for the toolbar.
@interface SettingsRootTableViewController (ToolbarAdd)

// Builds and returns an Add button for the toolbar.
- (UIBarButtonItem*)addButtonWithAction:(SEL)action;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_TOOLBAR_ADD_H_
