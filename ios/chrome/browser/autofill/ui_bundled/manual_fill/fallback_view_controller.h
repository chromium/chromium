// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FALLBACK_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FALLBACK_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

@class TableViewTextHeaderFooterItem;

@protocol TableViewFaviconDataSource;

// This class presents a list of fallback item in a table view.
@interface FallbackViewController : LegacyChromeTableViewController

// Data source for images.
@property(nonatomic, weak) id<TableViewFaviconDataSource> imageDataSource;

// Header item displayed when there are no data items to show amongst passwords,
// cards and addresses and independent of plus address. Needs to be explicitly
// set to `nil` if should not be shown.
@property(nonatomic, strong)
    TableViewTextHeaderFooterItem* noRegularDataItemsToShowHeaderItem;

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Presents a given item as a header section above data.
- (void)presentHeaderItem:(TableViewHeaderFooterItem*)item;

// Presents given items in 'items' section.
- (void)presentDataItems:(NSArray<TableViewItem*>*)items;

// Presents given action items in 'actions' section.
- (void)presentActionItems:(NSArray<TableViewItem*>*)actions;

// Presents given plus address action items in the `plus address actions`
// section.
- (void)presentPlusAddressActionItems:(NSArray<TableViewItem*>*)actions;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_FALLBACK_VIEW_CONTROLLER_H_
