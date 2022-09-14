// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// View controller that displays What's New features and chrome tips in a table
// view.
@interface WhatsNewTableViewController : ChromeTableViewController

// Initializers.
- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end
#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_TABLE_VIEW_CONTROLLER_H_