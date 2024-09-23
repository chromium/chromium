// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_EXPIRATION_DATE_EDIT_ITEM_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_EXPIRATION_DATE_EDIT_ITEM_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"

@protocol ExpirationDateEditItemDelegate;
@class ExpirationDatePicker;

// Model of an ExpirationDateEditCell.
// Defines a cell in a UITableView for inputting an expiration date.
// Once a date is picked, the object's delegate is notified.
@interface ExpirationDateEditItem : TableViewItem

// The delegate of this item that gets notified when an expiration date is
// picked.
@property(nonatomic, weak) id<ExpirationDateEditItemDelegate> delegate;

// Name of the input field.
// Displayed in a label next to the input field.
// This text explains to the user what information should be entered in the
// input field. e.g: "Expiration Date".
@property(nonatomic, copy) NSString* fieldNameLabelText;

// The selected expiration date month.
// Date format: M.
@property(nonatomic, readonly, copy) NSString* month;

// The selected expiration date year.
// Date format: yyyy.
@property(nonatomic, readonly, copy) NSString* year;

@end

// UITableViewCell for inputting an expiration date.
// Displays the following:
// -- Label with the name of the field.
// -- Text field editable using and ExpirationDatePicker.
@interface ExpirationDateEditCell : TableViewTextEditCell

// Input view of the cell's text field.
@property(nonatomic, readonly, strong)
    ExpirationDatePicker* expirationDatePicker;

// Updates the expiration date displayed in the cell's text field.
// If any of the arguments are null or empty, the empty string is displayed.
- (void)setMonth:(NSString*)month year:(NSString*)year;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_CELLS_EXPIRATION_DATE_EDIT_ITEM_H_
