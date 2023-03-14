// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewTextItem contains the model data for a TableViewTextCell.
@interface TableViewTextItem : TableViewItem

// Text Alignment for the cell's textLabel. Default is NSTextAlignmentNatural.
@property(nonatomic, assign) NSTextAlignment textAlignment;

// UIColor for the cell's textLabel. Default is
// [UIColor colorNamed:kTextPrimaryColor]. ChromeTableViewStyler's
// `cellTitleColor` takes precedence over the default color, but not over
// `textColor`.
@property(nonatomic, strong) UIColor* textColor;

@property(nonatomic, strong) NSString* text;

// Sets the font for the `text`. Default preferredFontForTextStyle is
// `UIFontTextStyleBody`.
@property(nonatomic, strong) UIFont* textFont;

// If set to YES, `text` will be shown as "••••••" with fixed length.
@property(nonatomic, assign) BOOL masked;

// Whether this item is enabled. If it is not enabled, the corresponding cell
// has its user interaction disabled. Enabled by default.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// Sets the `checked` property in the cell.
@property(nonatomic, assign) BOOL checked;

@end

// TableViewCell that displays a text label.
@interface TableViewTextCell : TableViewCell

// The text to display.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// Whether to show the checkmark accessory view.
@property(nonatomic, assign) BOOL checked;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_
