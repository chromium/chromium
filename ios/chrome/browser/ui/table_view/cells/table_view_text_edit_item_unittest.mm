// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_edit_item.h"

#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using TableViewTextEditItemTest = PlatformTest;

// Tests that the label and text field are set properly after a call to
// |configureCell:|.
TEST_F(TableViewTextEditItemTest, ConfigureCell) {
  TableViewTextEditItem* item = [[TableViewTextEditItem alloc] initWithType:0];
  NSString* name = @"Name";
  NSString* value = @"Value";
  BOOL enabled = NO;

  item.textFieldName = name;
  item.textFieldValue = value;
  item.textFieldEnabled = enabled;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextEditCell class]]);

  TableViewTextEditCell* textEditCell = cell;
  EXPECT_EQ(0U, textEditCell.textLabel.text.length);
  EXPECT_EQ(0U, textEditCell.textField.text.length);
  EXPECT_TRUE(textEditCell.textField.enabled);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(name, textEditCell.textLabel.text);
  EXPECT_NSEQ(value, textEditCell.textField.text);
  EXPECT_FALSE(textEditCell.textField.enabled);
}
