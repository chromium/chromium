// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_date_picker_input_view.h"

@implementation AutofillAIEntityEditDateItem

@synthesize attributeType = _attributeType;
@synthesize hasValidValueStatus = _hasValidValueStatus;
@dynamic delegate;

- (void)setEditingEnabled:(BOOL)editingEnabled {
  _editingEnabled = editingEnabled;
  self.textFieldEnabled = editingEnabled;
}

#pragma mark - TableViewItem

- (void)configureCell:(TableViewTextEditCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  if (self.editingEnabled) {
    cell.textField.inputView = [self createCustomInputView];
  } else {
    cell.textField.inputView = nil;
  }
}

#pragma mark - AutofillAIEntityFieldItem

- (void)setHasValidValueStatus:(BOOL)hasValidValueStatus {
  _hasValidValueStatus = hasValidValueStatus;
  [self setHasValidText:hasValidValueStatus];
}

#pragma mark - Private

// Creates the custom input view containing a navigation bar and a date picker.
- (UIView*)createCustomInputView {
  return [[AutofillAIDatePickerInputView alloc]
           initWithDate:self.dateValue
                  title:self.fieldNameLabelText
                 target:self
             dateAction:@selector(dateChanged:)
      clearButtonAction:@selector(clearDate)
       doneButtonAction:@selector(dismissPicker)];
}

// Handles the value changed event for the date picker.
- (void)dateChanged:(UIDatePicker*)sender {
  [self.delegate didChangeDate:sender.date forItem:self];
}

// Handles the tap on the Clear button to clear the date.
- (void)clearDate {
  [self.delegate didChangeDate:nil forItem:self];
}

// Handles the tap on the Done button to dismiss the picker.
- (void)dismissPicker {
  [self.delegate didDismissDateItem:self];
}

@end
