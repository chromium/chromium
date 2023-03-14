// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_MULTI_DETAIL_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_MULTI_DETAIL_TEXT_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewMultiDetailTextItem contains the model data for a
// TableViewMultiDetailTextCell.
@interface TableViewMultiDetailTextItem : TableViewItem

// Main text to be displayed.
@property(nonatomic, copy) NSString* text;
// Leading detail text to be displayed.
@property(nonatomic, copy) NSString* leadingDetailText;
// Trailing detail text to be displayed.
@property(nonatomic, copy) NSString* trailingDetailText;

// Text color for the trailing detail text.
@property(nonatomic, strong) UIColor* trailingDetailTextColor;

@end

// TableViewCell that displays two leading text labels on top of each other and
// one trailing text label. The leading text labels are displayed on an
// unlimited number of lines.
@interface TableViewMultiDetailTextCell : TableViewCell

@property(nonatomic, readonly, strong) UILabel* textLabel;
@property(nonatomic, readonly, strong) UILabel* leadingDetailTextLabel;
@property(nonatomic, readonly, strong) UILabel* trailingDetailTextLabel;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_MULTI_DETAIL_TEXT_ITEM_H_
