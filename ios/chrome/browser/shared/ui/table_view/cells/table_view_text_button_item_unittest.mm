// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {
using TableViewTextButtonItemTest = PlatformTest;
}

// Tests that the UILabels are set properly after a call to `configureCell:`.
TEST_F(TableViewTextButtonItemTest, SetProperties) {
  NSString* text = @"You need to do something.";
  NSString* buttonText = @"Tap to do something.";

  TableViewTextButtonItem* item =
      [[TableViewTextButtonItem alloc] initWithType:0];
  item.text = text;
  item.buttonText = buttonText;

  ASSERT_TRUE(item.dimBackgroundWhenDisabled);

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[TableViewTextButtonCell class]]);

  TableViewTextButtonCell* textButtonCell =
      base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
  EXPECT_FALSE(textButtonCell.textLabel.text);
  EXPECT_FALSE(textButtonCell.button.configuration.title);

  [item configureCell:textButtonCell
           withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, textButtonCell.textLabel.text);
  EXPECT_NSEQ(buttonText, textButtonCell.button.configuration.title);
}
