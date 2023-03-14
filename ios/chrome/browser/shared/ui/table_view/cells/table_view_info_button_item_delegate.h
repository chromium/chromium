// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_ITEM_DELEGATE_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_ITEM_DELEGATE_H_

@class TableViewItem;

@protocol TableViewInfoButtonItemDelegate <NSObject>

// Handles the info button tap inside the cell.
- (void)handleTappedInfoButtonForItem:(TableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_INFO_BUTTON_ITEM_DELEGATE_H_
