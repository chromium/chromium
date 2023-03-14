// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// Accessory symbol at the leading edge of the cell. These are SF Symbols only.
typedef NS_ENUM(NSInteger, TableViewDetailTextCellAccessorySymbol) {
  TableViewDetailTextCellAccessorySymbolNone = 0,
  TableViewDetailTextCellAccessorySymbolChevron,
  TableViewDetailTextCellAccessorySymbolExternalLink,
};

// TableViewDetailTextItem contains the model data for a
// TableViewDetailTextCell.
@interface TableViewDetailTextItem : TableViewItem

// Text Alignment for the cell's textLabel. Default is NSTextAlignmentNatural.
@property(nonatomic, assign) NSTextAlignment textAlignment;

// YES if multiple lines are allowed in the detail text.
@property(nonatomic, assign) BOOL allowMultilineDetailText;

// UIColor for the cell's textLabel. Default is
// [UIColor colorNamed:kTextPrimaryColor]. ChromeTableViewStyler's
// `cellTitleColor` takes precedence over the default color, but not over
// `textColor`.
@property(nonatomic, strong) UIColor* textColor;
// Main text to be displayed.
@property(nonatomic, strong) NSString* text;

// UIColor for the cell's detailTextLabel. Default is
// [UIColor colorNamed:kTextSecondaryColor].
@property(nonatomic, strong) UIColor* detailTextColor;
// Detail text to be displayed.
@property(nonatomic, strong) NSString* detailText;

// The accessory symbol associated with this item.
// The default color is `kTextQuaternaryColor` and is not currently
// configurable.
@property(nonatomic, assign)
    TableViewDetailTextCellAccessorySymbol accessorySymbol;

@end

// TableViewCell that displays two text labels on top of each other. The text
// labels are displaying on one line if the preferred content size isn't an
// Accessibility category. Otherwise they are displayed on an unlimited number
// of lines.
@interface TableViewDetailTextCell : TableViewCell

// The text to display.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// The detail text to display.
@property(nonatomic, readonly, strong) UILabel* detailTextLabel;

// YES if multiple lines are allowed in the detail text.
@property(nonatomic, assign) BOOL allowMultilineDetailText;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_DETAIL_TEXT_ITEM_H_
