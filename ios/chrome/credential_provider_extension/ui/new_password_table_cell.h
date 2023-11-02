// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_TABLE_CELL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_TABLE_CELL_H_

#import <UIKit/UIKit.h>

typedef NS_ENUM(NSInteger, NewPasswordTableCellType) {
  NewPasswordTableCellTypeUsername,
  NewPasswordTableCellTypePassword,
  NewPasswordTableCellTypeSuggestStrongPassword,
  NewPasswordTableCellTypeNumRows,
};

@class NewPasswordTableCell;

@protocol NewPasswordTableCellDelegate

// Alerts the delegate when the text field in this cell starts editing.
- (void)textFieldDidBeginEditingInCell:(NewPasswordTableCell*)cell;

// Alerts the delegate every time the text field changes in this cell.
- (void)textFieldDidChangeInCell:(NewPasswordTableCell*)cell;

// Allows the delegate to handle any behavior that should be triggered on
// pressing the return button. Returns YES if the text field should use the
// default return button behavior, and NO otherwise.
- (BOOL)textFieldShouldReturnInCell:(NewPasswordTableCell*)cell;

@end

@interface NewPasswordTableCell : UITableViewCell

// Reuse ID for registering this class in table views.
@property(nonatomic, class, readonly) NSString* reuseID;

// Field that holds the user-entered text.
@property(nonatomic, strong) UITextField* textField;

// Delegate for this cell.
@property(nonatomic, weak) id<NewPasswordTableCellDelegate> delegate;

// Sets the cell up to show the given type.
- (void)setCellType:(NewPasswordTableCellType)cellType;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_NEW_PASSWORD_TABLE_CELL_H_
