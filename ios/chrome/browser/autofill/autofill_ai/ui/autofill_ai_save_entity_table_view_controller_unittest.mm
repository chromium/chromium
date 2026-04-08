// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
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

// Tests that the table view controller displays the footer with URLs when
// saving to wallet.
TEST_F(AutofillAISaveEntityTableViewControllerTest,
       DisplayFooterWithUrlsForSaving) {
  autofill::EntityInstance newEntity = GetTestVehicle(kCarMaker2);
  newEntity = newEntity.CopyWithNewRecordType(
      autofill::EntityInstance::RecordType::kServerWallet);

  [controller_ setNewEntity:std::move(newEntity)
                  oldEntity:std::nullopt
                  userEmail:u"test@example.com"];

  [controller_ loadViewIfNeeded];

  // Verify Footer.
  // The footer section index is 1 if there is only new entity.
  UIView* footerView = [controller_ tableView:controller_.tableView
                       viewForFooterInSection:1];
  EXPECT_TRUE([footerView isKindOfClass:[TableViewLinkHeaderFooterView class]]);

  TableViewLinkHeaderFooterView* linkFooterView =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(footerView);

  EXPECT_EQ(1U, linkFooterView.urls.count);
  EXPECT_EQ(autofill::GetManageYourInfoURL(), linkFooterView.urls[0].gurl);
}

// Tests that the table view controller displays the footer with URLs when
// updating in wallet.
TEST_F(AutofillAISaveEntityTableViewControllerTest,
       DisplayFooterWithUrlsForUpdate) {
  autofill::EntityInstance newEntity = GetTestVehicle(kCarMaker2);
  newEntity = newEntity.CopyWithNewRecordType(
      autofill::EntityInstance::RecordType::kServerWallet);

  autofill::EntityInstance oldEntity = GetTestVehicle(kCarMaker1);
  // The record type of old entity is not being used. Set it to ServerWallet
  // for completeness.
  oldEntity = oldEntity.CopyWithNewRecordType(
      autofill::EntityInstance::RecordType::kServerWallet);

  [controller_ setNewEntity:std::move(newEntity)
                  oldEntity:std::move(oldEntity)
                  userEmail:u"test@example.com"];

  [controller_ loadViewIfNeeded];

  // Verify Footer.
  // The footer section index is 2 if there are both new and old entities.
  UIView* footerView = [controller_ tableView:controller_.tableView
                       viewForFooterInSection:2];
  EXPECT_TRUE([footerView isKindOfClass:[TableViewLinkHeaderFooterView class]]);

  TableViewLinkHeaderFooterView* linkFooterView =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(footerView);

  EXPECT_EQ(1U, linkFooterView.urls.count);
  EXPECT_EQ(autofill::GetGoogleWalletPassesURL(), linkFooterView.urls[0].gurl);
}
