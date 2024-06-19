// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/cells/expiration_date_edit_item.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/expiration_date_edit_item+Testing.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/expiration_date_edit_item_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/expiration_date_picker.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"

@interface ExpirationDateEditItem ()

// Making both properties writable privately.
@property(nonatomic, readwrite, copy) NSString* month;
@property(nonatomic, readwrite, copy) NSString* year;

@end

@implementation ExpirationDateEditItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = ExpirationDateEditCell.class;
  }
  return self;
}

- (void)configureCell:(ExpirationDateEditCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  cell.textLabel.text = self.fieldNameLabelText;
  [cell setMonth:self.month year:self.year];

  if (self.fieldNameLabelText.length) {
    cell.textField.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@_textField", self.fieldNameLabelText];
  }

  if (styler.cellBackgroundColor) {
    cell.textLabel.backgroundColor = styler.cellBackgroundColor;
    cell.textField.backgroundColor = styler.cellBackgroundColor;
  } else {
    cell.textLabel.backgroundColor = styler.tableViewBackgroundColor;
    cell.textField.backgroundColor = styler.tableViewBackgroundColor;
  }

  cell.textField.textColor = [UIColor colorNamed:kTextPrimaryColor];
  cell.textField.enabled = YES;
  // Prevent Voice Over from announcing autocorrection.
  cell.textField.autocorrectionType = UITextAutocorrectionTypeNo;

  [cell setIcon:TableViewTextEditItemIconTypeNone];

  cell.isAccessibilityElement = NO;

  __weak ExpirationDateEditItem* weakSelf = self;
  __weak ExpirationDateEditCell* weakCell = cell;
  cell.expirationDatePicker.onDateSelected =
      ^(NSString* month, NSString* year) {
        if (!weakSelf || !weakCell) {
          return;
        }
        auto* strongSelf = weakSelf;
        auto* strongCell = weakCell;
        strongSelf.month = month;
        strongSelf.year = year;
        [strongCell setMonth:month year:year];
        [strongSelf.delegate expirationDateEditItemDidChange:strongSelf];
      };
}

@end

@interface ExpirationDateEditCell () <UITextFieldDelegate>

@end

@implementation ExpirationDateEditCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (self) {
    _expirationDatePicker =
        [[ExpirationDatePicker alloc] initWithFrame:CGRectZero];
    _expirationDatePicker.backgroundColor = [UIColor clearColor];

    // Use the expiration date picker as input view instead of a keypad.
    self.textField.inputView = self.expirationDatePicker;
    self.textField.clearButtonMode = UITextFieldViewModeNever;
    self.textField.delegate = self;
  }

  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.textField.clearButtonMode = UITextFieldViewModeNever;
  self.expirationDatePicker.onDateSelected = nil;
  self.textField.text = @"";
}

#pragma mark - Public

- (void)setMonth:(NSString*)month year:(NSString*)year {
  if (month.length && year.length) {
    NSString* dateSeparator =
        l10n_util::GetNSString(IDS_AUTOFILL_EXPIRATION_DATE_SEPARATOR);
    self.textField.text =
        [NSString stringWithFormat:@"%@%@%@", month, dateSeparator, year];
  } else {
    self.textField.text = @"";
  }
}

#pragma mark - UITextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)string {
  return NO; /* Prevent any input from outside the date picker. */
}

@end
