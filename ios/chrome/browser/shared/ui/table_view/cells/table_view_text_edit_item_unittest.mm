// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

using TableViewTextEditItemTest = PlatformTest;

// Tests that the label and text field are set properly after a call to
// `configureCell:`.
TEST_F(TableViewTextEditItemTest, ConfigureCell) {
  TableViewTextEditItem* item = [[TableViewTextEditItem alloc] initWithType:0];
  NSString* name = @"Name";
  NSString* value = @"Value";
  BOOL enabled = NO;

  item.fieldNameLabelText = name;
  item.textFieldValue = value;
  item.textFieldEnabled = enabled;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextEditCell class]]);

  TableViewTextEditCell* textEditCell = cell;
  EXPECT_EQ(0U, textEditCell.textLabel.text.length);
  EXPECT_EQ(0U, textEditCell.textField.text.length);
  EXPECT_TRUE(textEditCell.textField.enabled);

  [item configureCell:cell];
  EXPECT_NSEQ(name, textEditCell.textLabel.text);
  EXPECT_NSEQ(value, textEditCell.textField.text);
  EXPECT_FALSE(textEditCell.textField.enabled);
  EXPECT_NSEQ(name, textEditCell.textField.accessibilityLabel);
}

// Tests that the required property adds "Required" to the accessibilityLabel
// of the text field.
TEST_F(TableViewTextEditItemTest, ConfigureCellRequired) {
  TableViewTextEditItem* item = [[TableViewTextEditItem alloc] initWithType:0];
  NSString* name = @"Name";
  NSString* value = @"Value";

  item.fieldNameLabelText = name;
  item.textFieldValue = value;
  item.required = YES;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextEditCell class]]);

  [item configureCell:cell];

  TableViewTextEditCell* textEditCell = cell;
  NSString* expectedLabel = [NSString stringWithFormat:@"%@*", name];
  EXPECT_NSEQ(expectedLabel, textEditCell.textLabel.text);

  NSString* requiredText =
      l10n_util::GetNSString(IDS_IOS_FIELD_REQUIRED_ACCESSIBILITY_LABEL);
  NSString* expectedAccessibilityLabel =
      [NSString stringWithFormat:@"%@, %@", name, requiredText];
  EXPECT_NSEQ(expectedAccessibilityLabel,
              textEditCell.textField.accessibilityLabel);
}
