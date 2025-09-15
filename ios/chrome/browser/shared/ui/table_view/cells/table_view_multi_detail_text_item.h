// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_MULTI_DETAIL_TEXT_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_MULTI_DETAIL_TEXT_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewMultiDetailTextItem contains the model data for a
// TableViewCellContentView.
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

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_MULTI_DETAIL_TEXT_ITEM_H_
