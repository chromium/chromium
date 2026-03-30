// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_edit_date_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/date_picker_content_configuration.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/label_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"

@implementation AutofillAIEntityEditDateItem

@synthesize attributeType = _attributeType;

- (void)configureCell:(LegacyTableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];

  if ([cell.contentConfiguration
          isKindOfClass:[TableViewCellContentConfiguration class]]) {
    TableViewCellContentConfiguration* contentConfig =
        base::apple::ObjCCast<TableViewCellContentConfiguration>(
            cell.contentConfiguration);

    contentConfig.trailingText = nil;
    if (self.editingEnabled) {
      DatePickerContentConfiguration* datePickerConfig =
          [[DatePickerContentConfiguration alloc] init];
      datePickerConfig.date = self.dateValue ?: [NSDate date];
      datePickerConfig.target = self;
      datePickerConfig.selector = @selector(dateChanged:);
      datePickerConfig.userInteractionEnabled = YES;

      contentConfig.trailingConfiguration = datePickerConfig;
    } else {
      LabelContentConfiguration* labelConfig =
          [[LabelContentConfiguration alloc] init];
      labelConfig.text = self.detailText;

      contentConfig.trailingConfiguration = labelConfig;
    }

    cell.contentConfiguration = contentConfig;
  } else {
    cell.detailTextLabel.text = self.detailText;
    cell.contentConfiguration = nil;
  }
}

- (void)dateChanged:(UIDatePicker*)sender {
  [self.delegate didChangeDate:sender.date forItem:self];
}

@end
