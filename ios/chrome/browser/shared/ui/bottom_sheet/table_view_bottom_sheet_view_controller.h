// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_TABLE_VIEW_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_TABLE_VIEW_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/bottom_sheet/bottom_sheet_view_controller.h"

// UI Base class for Bottom Sheets with a table view, such as Password or
// Payments bottom sheets.
@interface TableViewBottomSheetViewController
    : BottomSheetViewController <UITableViewDelegate>

// Height of the parent view controller.
@property(nonatomic, assign) CGFloat parentViewControllerHeight;

// Request to relaod data from the table view's data source.
- (void)reloadTableViewData;

// Returns the currently selected row.
- (NSInteger)selectedRow;

// Returns the width of the table view.
- (CGFloat)tableViewWidth;

// Returns the cell's separator inset for the provided index path.
- (UIEdgeInsets)separatorInsetForTableViewWidth:(CGFloat)tableViewWidth
                                    atIndexPath:(NSIndexPath*)indexPath;

// Returns the cell's accessory type for the provided index path.
- (UITableViewCellAccessoryType)accessoryType:(NSIndexPath*)indexPath;

// Sets appropriate margin sizes for password and payment autofill bottom sheets
- (void)adjustTransactionsPrimaryActionButtonHorizontalConstraints;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_TABLE_VIEW_BOTTOM_SHEET_VIEW_CONTROLLER_H_
