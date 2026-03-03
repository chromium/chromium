// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

constexpr std::u16string_view kCarMaker1 = u"Car Maker 1";
constexpr std::u16string_view kCarMaker2 = u"Car Maker 2";

autofill::EntityInstance GetTestVehicle(std::u16string_view maker) {
  autofill::test::VehicleOptions options;
  options.make = maker.data();
  return autofill::test::GetVehicleEntityInstance(options);
}

}  // namespace

class AutofillAISaveEntityTableViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    controller_ = [[AutofillAISaveEntityTableViewController alloc]
        initWithStyle:ChromeTableViewStyle()];
  }

  AutofillAISaveEntityTableViewController* controller_;
};

// Tests that the table view controller displays the new and old sections.
TEST_F(AutofillAISaveEntityTableViewControllerTest, DisplayNewAndOldSections) {
  autofill::EntityInstance newEntity = GetTestVehicle(kCarMaker2);
  autofill::EntityInstance oldEntity = GetTestVehicle(kCarMaker1);

  [controller_ setNewEntity:std::move(newEntity)
                  oldEntity:std::move(oldEntity)
                  userEmail:u"test@example.com"];

  [controller_ loadViewIfNeeded];

  UITableViewDiffableDataSource<NSNumber*, TableViewItem*>* dataSource =
      base::apple::ObjCCastStrict<
          UITableViewDiffableDataSource<NSNumber*, TableViewItem*>>(
          controller_.tableView.dataSource);

  // Verify New Info Section.
  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  TableViewTextEditItem* newItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(
          [dataSource itemIdentifierForIndexPath:indexPath]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(kCarMaker2), newItem.textFieldValue);

  // Verify Old Info Section.
  indexPath = [NSIndexPath indexPathForRow:0 inSection:1];
  TableViewTextEditItem* oldItem =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(
          [dataSource itemIdentifierForIndexPath:indexPath]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(kCarMaker1), oldItem.textFieldValue);
}
