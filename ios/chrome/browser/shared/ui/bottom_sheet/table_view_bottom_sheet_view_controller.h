// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_TABLE_VIEW_BOTTOM_SHEET_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_TABLE_VIEW_BOTTOM_SHEET_VIEW_CONTROLLER_H_

#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"

// UI Base class for Bottom Sheets with a table view, such as Password or
// Payments bottom sheets.
@interface TableViewBottomSheetViewController
    : ConfirmationAlertViewController <UITableViewDelegate>

// Creates the table view which will display suggestions on the bottom sheet.
- (UITableView*)createTableView;

// Performs the expand bottom sheet animation.
- (void)expand:(NSInteger)numberOfRows;

// Returns the estimated height of a single row in the table view.
- (CGFloat)tableViewEstimatedRowHeight;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_BOTTOM_SHEET_TABLE_VIEW_BOTTOM_SHEET_VIEW_CONTROLLER_H_
