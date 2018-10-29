// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELLS_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELLS_CONSTANTS_H_

#import <UIKit/UIKit.h>

// The minimum height for a TableViewHeaderFooterView.
extern const CGFloat kTableViewHeaderFooterViewHeight;

// The horizontal spacing between views and the container view of a cell.
extern const CGFloat kTableViewHorizontalSpacing;

// The vertical spacing between views and the container view of a cell.
extern const CGFloat kTableViewVerticalSpacing;

// The horizontal spacing between subviews within the container view.
extern const CGFloat kTableViewSubViewHorizontalSpacing;

// Animation duration for highlighting selected section header.
extern const CGFloat kTableViewCellSelectionAnimationDuration;

// Setting the font size to 0 for a custom preferred font lets iOS manage
// sizing.
extern const CGFloat kUseDefaultFontSize;

// Spacing between text label and cell contentView.
extern const CGFloat kTableViewLabelVerticalTopSpacing;

// Hex Value for light gray label text color.
extern const int kTableViewTextLabelColorLightGrey;

// Hex Value for the text color of the secondary labels (e.g. details, URL,
// metadata...).
extern const int kTableViewSecondaryLabelLightGrayTextColor;

// Hex Value for the tint color for switches.
extern const int kTableViewSwitchTintColor;

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_CELLS_CONSTANTS_H_
