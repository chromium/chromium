// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewTextItem contains the model data for a cell displaying a text.
@interface TableViewTextItem : TableViewItem

// UIColor for the cell's textLabel. Default is
// [UIColor colorNamed:kTextPrimaryColor].
@property(nonatomic, strong) UIColor* textColor;

@property(nonatomic, copy) NSString* text;

// Changes the font to use "headline". This is a workaround to mimic headers and
// should not be used. Default is NO.
@property(nonatomic, assign) BOOL useHeadlineFont;

// If set to YES, `text` will be shown as "••••••" with fixed length.
@property(nonatomic, assign) BOOL masked;

// Whether this item is enabled. If it is not enabled, the corresponding cell
// has its user interaction disabled. Enabled by default.
@property(nonatomic, assign, getter=isEnabled) BOOL enabled;

// Sets the `checked` property in the cell.
@property(nonatomic, assign) BOOL checked;

// Sets the number of line for the cell title. Default is 1.
@property(nonatomic, assign) NSInteger titleNumberOfLines;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_ITEM_H_
