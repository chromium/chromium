// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using TableViewMultiLineTextEditItemTest = PlatformTest;

// Tests that the label and text field are set properly after a call to
// `configureCell:`.
TEST_F(TableViewMultiLineTextEditItemTest, ConfigureCell) {
  TableViewMultiLineTextEditItem* item =
      [[TableViewMultiLineTextEditItem alloc] initWithType:0];
  NSString* label = @"Label";
  NSString* text = @"Text";
  BOOL enabled = NO;

  item.label = label;
  item.text = text;
  item.editingEnabled = enabled;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewMultiLineTextEditCell class]]);

  TableViewMultiLineTextEditCell* textEditCell = cell;
  EXPECT_EQ(0U, textEditCell.textLabel.text.length);
  EXPECT_EQ(0U, textEditCell.textView.text.length);
  EXPECT_TRUE(textEditCell.textView.isEditable);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(label, textEditCell.textLabel.text);
  EXPECT_NSEQ(text, textEditCell.textView.text);
  EXPECT_FALSE(textEditCell.textView.isEditable);
}
