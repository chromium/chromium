// Copyright 2018 The Chromium Authors
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

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Data source for images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

// Presents a given item as a header section above data.
- (void)presentHeaderItem:(TableViewItem*)item;

// Presents given items in 'items' section.
- (void)presentDataItems:(NSArray<TableViewItem*>*)items;

// Presents given action items in 'actions' section.
- (void)presentActionItems:(NSArray<TableViewItem*>*)actions;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_FALLBACK_VIEW_CONTROLLER_H_
