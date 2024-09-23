// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_TABLE_VIEW_BOTTOM_SHEET_VIEW_CONTROLLER_SUBCLASSING_H_
#define IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_TABLE_VIEW_BOTTOM_SHEET_VIEW_CONTROLLER_SUBCLASSING_H_

@class UITableView;

// Interface for concrete subclasses of TableViewBottomSheetViewController.
@interface TableViewBottomSheetViewController (Subclassing)

// Creates the table view which will display suggestions on the bottom sheet.
- (UITableView*)createTableView;

// Number of rows in the table view.
- (NSUInteger)rowCount;

// Computes the height of a row at the given index.
- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_TABLE_VIEW_BOTTOM_SHEET_VIEW_CONTROLLER_SUBCLASSING_H_
