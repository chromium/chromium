// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELLS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELLS_CONSTANTS_H_

#import <UIKit/UIKit.h>

// The font text style of the sublabel.
extern const UIFontTextStyle kTableViewSublabelFontStyle;

// The minimum height for a TableViewHeaderFooterView.
extern const CGFloat kTableViewHeaderFooterViewHeight;

// The minimum height for a TableViewCell.
extern const CGFloat kChromeTableViewCellHeight;

// The horizontal spacing between views and the container view of a cell.
extern const CGFloat kTableViewHorizontalSpacing;

// The vertical spacing for a cell containing only one label.
extern const CGFloat kTableViewOneLabelCellVerticalSpacing;

// The vertical spacing for a cell containing one label and one sub label.
extern const CGFloat kTableViewTwoLabelsCellVerticalSpacing;

// The vertical spacing between views and the container view of a cell.
extern const CGFloat kTableViewVerticalSpacing;

// The large vertical spacing between views and the container view of a cell.
extern const CGFloat kTableViewLargeVerticalSpacing;

// The horizontal spacing between subviews within the container view.
extern const CGFloat kTableViewSubViewHorizontalSpacing;

// Animation duration for highlighting selected section header.
extern const CGFloat kTableViewCellSelectionAnimationDuration;

// Setting the font size to 0 for a custom preferred font lets iOS manage
// sizing.
extern const CGFloat kUseDefaultFontSize;

// Spacing between text label and cell contentView.
extern const CGFloat kTableViewLabelVerticalTopSpacing;

// The width taken by the accessory view when it is displayed.
extern const CGFloat kTableViewAccessoryWidth;

// A masked password string(e.g. "••••••••").
extern NSString* const kMaskedPassword;

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELLS_CONSTANTS_H_
