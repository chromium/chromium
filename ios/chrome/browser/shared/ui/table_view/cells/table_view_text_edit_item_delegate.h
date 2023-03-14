// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_EDIT_ITEM_DELEGATE_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_EDIT_ITEM_DELEGATE_H_

@class TableViewTextEditItem;

// Delegate that handles the editing of a table view text edit item.
@protocol TableViewTextEditItemDelegate

// Notfies that the user has begun editing a table view item textfield.
- (void)tableViewItemDidBeginEditing:(TableViewTextEditItem*)tableViewItem;

// Notfies that the user has changed a table view item textfield.
- (void)tableViewItemDidChange:(TableViewTextEditItem*)tableViewItem;

// Notfies that the user has ended editing a table view item textfield.
- (void)tableViewItemDidEndEditing:(TableViewTextEditItem*)tableViewItem;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_EDIT_ITEM_DELEGATE_H_
