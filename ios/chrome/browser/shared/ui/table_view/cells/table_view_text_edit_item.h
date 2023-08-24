// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_EDIT_ITEM_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_EDIT_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

@protocol TableViewTextEditItemDelegate;

// Icon type for the item.
typedef NS_ENUM(NSInteger, TableViewTextEditItemIconType) {
  TableViewTextEditItemIconTypeNone,
  TableViewTextEditItemIconTypeEdit,
  TableViewTextEditItemIconTypeError,
};

// Item to represent and configure an TableViewTextEditItem. It features a label
// and a text field.
@interface TableViewTextEditItem : TableViewItem

// The delegate for this table view text edit item.
@property(nonatomic, weak) id<TableViewTextEditItemDelegate> delegate;

// Name of the text field.
// Displayed in a label next to the text field.
@property(nonatomic, copy) NSString* fieldNameLabelText;

// The placeholder of the text field.
@property(nonatomic, copy) NSString* textFieldPlaceholder;

// The value of the text field.
@property(nonatomic, copy) NSString* textFieldValue;

// Text color for the text field of the cell. Default is [UIColor
// colorNamed:kTextPrimaryColor].
@property(nonatomic, strong) UIColor* textFieldTextColor;

// An icon identifying the text field or its current value, if any.
@property(nonatomic, copy) UIImage* identifyingIcon;

// If YES the identifyingIcon will be enabled as a button. Disabled by default.
@property(nonatomic, assign) BOOL identifyingIconEnabled;

// If set the String will be used as the identifyingIcon button A11y label.
@property(nonatomic, copy) NSString* identifyingIconAccessibilityLabel;

// Whether to hide or display the trailing icon.
// Changing this value can change the text color for the text field.
@property(nonatomic, assign) BOOL hideIcon;

// Whether this field is required. If YES, an "*" is appended to the name of the
// text field to indicate that the field is required. It is also used for
// validation purposes.
@property(nonatomic, getter=isRequired) BOOL required;

// Boolean value that indicates whether the text entered by the user in the text
// field is hidden.
@property(nonatomic, assign) BOOL textFieldSecureTextEntry;

// Whether the text field is enabled for editing.
@property(nonatomic, getter=isTextFieldEnabled) BOOL textFieldEnabled;

// Controls the display of the return key when the keyboard is displaying.
@property(nonatomic, assign) UIReturnKeyType returnKeyType;

// Keyboard type to be displayed when the text field becomes first responder.
@property(nonatomic, assign) UIKeyboardType keyboardType;

// Controls autocapitalization behavior of the text field.
@property(nonatomic, assign)
    UITextAutocapitalizationType autoCapitalizationType;

// Background color used by the textField. If none is set the
// cellBackgroundColor will be used as background.
@property(nonatomic, strong) UIColor* textFieldBackgroundColor;

// Custom textfield accessibility identifier.
@property(nonatomic, copy) NSString* customTextfieldAccessibilityIdentifier;

// Whether the aspect of the cell should mark the text as valid.
- (void)setHasValidText:(BOOL)hasValidText;

@end

// TableViewTextEditCell implements an TableViewCell subclass containing a label
// and a text field.
@interface TableViewTextEditCell : TableViewCell

// Label at the leading edge of the cell. It displays the item's
// fieldNameLabelText.
@property(nonatomic, strong) UILabel* textLabel;

// Text field at the trailing edge of the cell. It displays the item's
// `textFieldValue`.
@property(nonatomic, readonly, strong) UITextField* textField;

// Identifying button. UIButton containing the icon
// identifying `textField` or its current value. It is located at the most
// trailing position of the Cell.
@property(nonatomic, readonly, strong) UIButton* identifyingIconButton;

// UIImageView containing the icon indicating that `textField` is editable.
@property(nonatomic, strong) UIImageView* iconView;

// Sets `self.identifyingIconButton` icon.
- (void)setIdentifyingIcon:(UIImage*)icon;

// Sets the icon view image to the type specified by `iconType`.
- (void)setIcon:(TableViewTextEditItemIconType)iconType;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_CELLS_TABLE_VIEW_TEXT_EDIT_ITEM_H_
