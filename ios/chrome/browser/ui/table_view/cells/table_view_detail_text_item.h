// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"

// TableViewDetailTextItem contains the model data for a
// TableViewDetailTextCell.
@interface TableViewDetailTextItem : TableViewItem

// Text Alignment for the cell's textLabel. Default is NSTextAlignmentNatural.
@property(nonatomic, assign) NSTextAlignment textAlignment;

// The accessory type to display on the trailing edge of the cell.
@property(nonatomic) UITableViewCellAccessoryType accessoryType;

// UIColor for the cell's textLabel. Default is
// kTableViewTextLabelColorLightGrey. ChromeTableViewStyler's |cellTitleColor|
// takes precedence over the default color, but not over |textColor|.
@property(nonatomic, assign) UIColor* textColor;
// Main text to be displayed.
@property(nonatomic, strong) NSString* text;

// UIColor for the cell's detailTextLabel. Default is
// kTableViewSecondaryLabelLightGrayTextColor.
@property(nonatomic, assign) UIColor* detailTextColor;
// Detail text to be displayed.
@property(nonatomic, strong) NSString* detailText;

@end

// UITableViewCell that displays two text labels on top of each other. The text
// labels are displaying on one line if the preferred content size isn't an
// Accessibility category. Otherwise they are displayed on an unlimited number
// of lines.
@interface TableViewDetailTextCell : UITableViewCell

// The text to display.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// The detail text to display.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_TEXT_ITEM_H_
