// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/cells/expiration_date_edit_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/autofill/ui_bundled/cells/expiration_date_edit_item_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/expiration_date_picker.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

using ExpirationDateEditItemTest = PlatformTest;

// Tests that the cell's text field label is properly set after calling
// `ConfigureCell:`.
TEST_F(ExpirationDateEditItemTest, ConfigureCellSetsFieldName) {
  ExpirationDateEditItem* item =
      [[ExpirationDateEditItem alloc] initWithType:0];
  NSString* field_name_label_text = @"Expiration Date";

  item.fieldNameLabelText = field_name_label_text;

  id view = [[[item cellClass] alloc] init];
  ASSERT_TRUE([view isMemberOfClass:[ExpirationDateEditCell class]]);

  ExpirationDateEditCell* cell =
      base::apple::ObjCCastStrict<ExpirationDateEditCell>(view);
  EXPECT_EQ(0U, cell.textLabel.text.length);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];
  EXPECT_NSEQ(field_name_label_text, cell.textLabel.text);
}

// Tests that the configured cell text field is set with the formatted date and
// the item properties are set after a date is picked in the cell.
TEST_F(ExpirationDateEditItemTest, PickingDateUpdatesItemAndTextField) {
  ExpirationDateEditItem* item =
      [[ExpirationDateEditItem alloc] initWithType:0];
  id mockedDelegate =
      OCMStrictProtocolMock(@protocol(ExpirationDateEditItemDelegate));
  item.delegate = mockedDelegate;

  id view = [[[item cellClass] alloc] init];
  ExpirationDateEditCell* cell =
      base::apple::ObjCCastStrict<ExpirationDateEditCell>(view);

  NSString* month = @"10";
  NSString* year = @"9999";
  NSString* formatted_date = [NSString stringWithFormat:@"%@/%@", month, year];

  EXPECT_EQ(0U, cell.textField.text.length);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  OCMExpect([mockedDelegate expirationDateEditItemDidChange:item]);

  cell.expirationDatePicker.onDateSelected(month, year);

  EXPECT_NSEQ(formatted_date, cell.textField.text);
  EXPECT_NSEQ(month, item.month);
  EXPECT_NSEQ(year, item.year);

  EXPECT_OCMOCK_VERIFY(mockedDelegate);
}

// Verifies that the ExpirationDateEditCell exposes its contents to
// accessibility tools such as Voice Over.
TEST_F(ExpirationDateEditItemTest,
       ExpirationDateEditCellIsNotAccessibilityElement) {
  ExpirationDateEditItem* item =
      [[ExpirationDateEditItem alloc] initWithType:0];

  id view = [[[item cellClass] alloc] init];
  ASSERT_TRUE([view isMemberOfClass:[ExpirationDateEditCell class]]);

  ExpirationDateEditCell* cell =
      base::apple::ObjCCastStrict<ExpirationDateEditCell>(view);

  ChromeTableViewStyler* styler = [[ChromeTableViewStyler alloc] init];
  [item configureCell:cell withStyler:styler];

  EXPECT_FALSE(cell.isAccessibilityElement);
}

}  // namespace
