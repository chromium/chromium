// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_MULTI_LINE_TEXT_EDIT_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_MULTI_LINE_TEXT_EDIT_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@protocol TableViewMultiLineTextEditItemDelegate;

// Contains label and field data displayed in TableViewMultiLineTextEditCell.
@interface TableViewMultiLineTextEditItem : TableViewItem

// The delegate for this table view multi-line text edit item.
@property(nonatomic, weak) id<TableViewMultiLineTextEditItemDelegate> delegate;

// Name of the text field. Displayed in a label next to the field.
@property(nonatomic, copy) NSString* label;

// The value of the multi-line text field.
@property(nonatomic, copy) NSString* text;

// Whether the text field is enabled for editing.
@property(nonatomic, assign) BOOL editingEnabled;

// Whether the text typed in `textView` is valid. YES by default.
@property(nonatomic, assign) BOOL validText;

// Whether interaction with the text field is enabled. YES by default.
@property(nonatomic, assign) BOOL textFieldInteractionEnabled;

@end

// Implements a TableViewCell that displays a label in the first line and a
// multi-line text below it.
@interface TableViewMultiLineTextEditCell : TableViewCell

// Label at the leading edge of the cell.
@property(nonatomic, readonly, strong) UILabel* textLabel;

// Text field below the label.
@property(nonatomic, readonly, strong) UITextView* textView;

// Displays error icon when the typed text view is not valid, it is nil
// otherwise. Placed at the trailing edge of the cell, next to the label.
@property(nonatomic, strong) UIImageView* iconView;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_MULTI_LINE_TEXT_EDIT_ITEM_H_
