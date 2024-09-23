// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bookmarks/ui_bundled/cells/bookmark_text_field_item.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

using BookmarkTextFieldItemTest = PlatformTest;

TEST_F(BookmarkTextFieldItemTest, DelegateGetsTextFieldEvents) {
  BookmarkTextFieldItem* item = [[BookmarkTextFieldItem alloc] initWithType:0];
  BookmarkTextFieldCell* cell =
      [[BookmarkTextFieldCell alloc] initWithFrame:CGRectZero];
  id mockDelegate =
      [OCMockObject mockForProtocol:@protocol(BookmarkTextFieldItemDelegate)];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];

  item.delegate = mockDelegate;
  [item configureCell:cell withStyler:styler];
  EXPECT_EQ(mockDelegate, cell.textField.delegate);

  [[mockDelegate expect] textDidChangeForItem:item];
  cell.textField.text = @"Foo";
}

TEST_F(BookmarkTextFieldItemTest, TextFieldGetsText) {
  BookmarkTextFieldItem* item = [[BookmarkTextFieldItem alloc] initWithType:0];
  BookmarkTextFieldCell* cell =
      [[BookmarkTextFieldCell alloc] initWithFrame:CGRectZero];
  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];

  item.text = @"Foo";
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(@"Foo", cell.textField.text);
}

}  // namespace
