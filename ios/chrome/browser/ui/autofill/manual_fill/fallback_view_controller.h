// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FALLBACK_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FALLBACK_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol TableViewFaviconDataSource;

// This class presents a list of fallback item in a table view.
@interface FallbackViewController : ChromeTableViewController

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

// If set to YES, the controller will add negative content insets inverse to the
// ones added by UITableViewController to accommodate for the keyboard.
// Not needed and ignored on iOS >= 13.
@property(nonatomic, assign) BOOL contentInsetsAlwaysEqualToSafeArea;

// Data source for images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

// Presents given items in 'items' section.
- (void)presentDataItems:(NSArray<TableViewItem*>*)items;

// Presents given action items in 'actions' section.
- (void)presentActionItems:(NSArray<TableViewItem*>*)actions;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FALLBACK_VIEW_CONTROLLER_H_
