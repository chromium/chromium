// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"

#import "base/apple/foundation_util.h"

@implementation AutofillAIEntityEditDateItem

@synthesize attributeType = _attributeType;
@synthesize hasValidValueStatus = _hasValidValueStatus;
@dynamic delegate;

- (void)configureCell:(TableViewTextEditCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  if (self.editingEnabled) {
    UIDatePicker* datePicker = [[UIDatePicker alloc] init];
    datePicker.datePickerMode = UIDatePickerModeDate;
    datePicker.preferredDatePickerStyle = UIDatePickerStyleWheels;
    [datePicker addTarget:self
                   action:@selector(dateChanged:)
         forControlEvents:UIControlEventValueChanged];

    cell.textField.inputView = datePicker;

    if (self.dateValue) {
      datePicker.date = self.dateValue;
    }
  } else {
    cell.textField.inputView = nil;
  }
}

- (void)setEditingEnabled:(BOOL)editingEnabled {
  _editingEnabled = editingEnabled;
  self.textFieldEnabled = editingEnabled;
}

- (void)dateChanged:(UIDatePicker*)sender {
  [self.delegate didChangeDate:sender.date forItem:self];
}

- (void)setHasValidValueStatus:(BOOL)hasValidValueStatus {
  _hasValidValueStatus = hasValidValueStatus;
  [self setHasValidText:hasValidValueStatus];
}

@end
