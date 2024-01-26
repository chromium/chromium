// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@interface ChromeTableViewController : UITableViewController

// Adds an empty table view in the center of the Table View which displays
// `message` with `image` on top.  `message` will be rendered using default
// styling.  This will remove any existing table view background views.
- (void)addEmptyTableViewWithMessage:(NSString*)message image:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CHROME_TABLE_VIEW_CONTROLLER_H_
