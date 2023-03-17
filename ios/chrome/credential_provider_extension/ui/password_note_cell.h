// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSWORD_NOTE_CELL_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSWORD_NOTE_CELL_H_

#import <UIKit/UIKit.h>

@class PasswordNoteCell;

@protocol PasswordNoteCellDelegate

// Alerts the delegate every time the text field changes in this cell.
- (void)textViewDidChangeInCell:(PasswordNoteCell*)cell;

@end

@interface PasswordNoteCell : UITableViewCell

// Reuse ID for registering this class in table views.
@property(nonatomic, class, readonly) NSString* reuseID;

// Label at the leading edge of the cell.
@property(nonatomic, strong) UILabel* textLabel;

// Displays error icon when the typed text view is not valid, it is nil
// otherwise. Placed at the trailing edge of the cell, next to the label.
@property(nonatomic, strong) UIImageView* iconView;

// Text field below the label.
@property(nonatomic, strong) UITextView* textView;

// Delegate for this cell.
@property(nonatomic, weak) id<PasswordNoteCellDelegate> delegate;

- (void)configureCell;

// Displays error icon and sets text color to red when state is not valid.
// Otherwise, hides icon and sets normal text color.
- (void)setValid:(BOOL)validState;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_PASSWORD_NOTE_CELL_H_
