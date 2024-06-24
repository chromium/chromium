// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/unit_conversion/ui_bundled/unit_type_value_field_cell.h"

namespace {

// Leading constraint of the unit's value text field.
const CGFloat kUnitValueTextFieldLeadingOffset = 20;

// Cells height anchors constraint.
const CGFloat kUnitTypeValueFieldCellHeightAnchor = 60;

}  // namespace

@implementation UnitTypeValueFieldCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    // Set up the numerical text field.
    _unitValueTextField = [[UITextField alloc] initWithFrame:CGRectZero];
    _unitValueTextField.keyboardType = UIKeyboardTypeDecimalPad;
    _unitValueTextField.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
    [self.contentView addSubview:_unitValueTextField];

    _unitValueTextField.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [_unitValueTextField.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kUnitValueTextFieldLeadingOffset],
      [_unitValueTextField.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_unitValueTextField.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor],
      [self.contentView.heightAnchor constraintGreaterThanOrEqualToConstant:
                                         kUnitTypeValueFieldCellHeightAnchor],
      [self.contentView.heightAnchor
          constraintGreaterThanOrEqualToAnchor:_unitValueTextField
                                                   .heightAnchor],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _unitValueTextField.enabled = YES;
  [_unitValueTextField removeTarget:nil
                             action:nil
                   forControlEvents:UIControlEventEditingChanged];
}

@end
