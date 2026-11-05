// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

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

  // Returns the link footer view for `section`.
  TableViewLinkHeaderFooterView* GetLinkFooterView(NSInteger section) {
    UIView* footerView = [controller_ tableView:controller_.tableView
                         viewForFooterInSection:section];
    EXPECT_TRUE(
        [footerView isKindOfClass:[TableViewLinkHeaderFooterView class]]);
    return base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(
        footerView);
  }

  AutofillAISaveEntityTableViewController* controller_;
};

// Tests that the table view controller displays the new and old sections.
TEST_F(AutofillAISaveEntityTableViewControllerTest, DisplayNewAndOldSections) {
  autofill::EntityInstance newEntity = GetTestVehicle(kCarMaker2);
  autofill::EntityInstance oldEntity = GetTestVehicle(kCarMaker1);

  [controller_ setNewEntity:std::move(newEntity)
                  oldEntity:std::move(oldEntity)];

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

  // No footer view is returned when `footerText` has not been set.
  EXPECT_EQ(nil, [controller_ tableView:controller_.tableView
                     viewForFooterInSection:2]);
}

// Tests that the table view controller displays the footer text and URLs pushed
// via `setFooterText:urls:`.
TEST_F(AutofillAISaveEntityTableViewControllerTest,
       DisplayFooterWithTextAndUrls) {
  [controller_ setNewEntity:GetTestVehicle(kCarMaker2) oldEntity:std::nullopt];

  NSMutableArray<CrURL*>* urls = [NSMutableArray array];
  NSString* footerText =
      autofill::GetSaveEntityToWalletFooterTextAppendingURLsTo(
          @"test@example.com", /*is_private_pass=*/false, urls);
  [controller_ setFooterText:footerText urls:urls];

  [controller_ loadViewIfNeeded];

  // The footer section index is 1 if there is only new entity.
  TableViewLinkHeaderFooterView* linkFooterView =
      GetLinkFooterView(/*section=*/1);

  ASSERT_EQ(1U, linkFooterView.urls.count);
  EXPECT_EQ(autofill::GetManageYourInfoURL(), linkFooterView.urls[0].gurl);
}

// Tests that setting the footer after the view is loaded reloads the footer
// section.
TEST_F(AutofillAISaveEntityTableViewControllerTest,
       SetFooterAfterViewLoadedUpdatesFooter) {
  [controller_ setNewEntity:GetTestVehicle(kCarMaker2) oldEntity:std::nullopt];
  [controller_ loadViewIfNeeded];

  // Verify footer before setting footer text.
  EXPECT_EQ(nil, [controller_ tableView:controller_.tableView
                     viewForFooterInSection:1]);

  CrURL* legalURL =
      [[CrURL alloc] initWithGURL:GURL("https://www.example.com/legal")];
  [controller_ setFooterText:@"Saved locally.\n\nBEGIN_LINKTestEND_LINK legal"
                        urls:@[ legalURL ]];

  TableViewLinkHeaderFooterView* linkFooterView =
      GetLinkFooterView(/*section=*/1);
  UITextView* textView =
      chrome_test_util::FindViewByClass<UITextView>(linkFooterView.contentView);
  ASSERT_NE(nil, textView);
  EXPECT_NSEQ(@"Saved locally.\n\nTest legal", textView.attributedText.string);
  ASSERT_EQ(1U, linkFooterView.urls.count);
  EXPECT_EQ(GURL("https://www.example.com/legal"), linkFooterView.urls[0].gurl);
}

// Tests that setting identical footer text and URLs (with distinct CrURL
// instances having identical GURL values) is a no-op and does not replace the
// existing URLs array.
TEST_F(AutofillAISaveEntityTableViewControllerTest,
       SetFooterWithIdenticalContentDoesNotUpdate) {
  [controller_ setNewEntity:GetTestVehicle(kCarMaker2) oldEntity:std::nullopt];
  [controller_ loadViewIfNeeded];

  NSString* footerText = @"Saved locally.\n\nBEGIN_LINKTestEND_LINK legal";
  CrURL* legalURL =
      [[CrURL alloc] initWithGURL:GURL("https://www.example.com/legal")];
  NSArray<CrURL*>* originalURLs = @[ legalURL ];
  [controller_ setFooterText:footerText urls:originalURLs];

  TableViewLinkHeaderFooterView* linkFooterView =
      GetLinkFooterView(/*section=*/1);
  EXPECT_EQ(originalURLs[0], linkFooterView.urls[0]);

  CrURL* distinctLegalURL =
      [[CrURL alloc] initWithGURL:GURL("https://www.example.com/legal")];
  NSArray<CrURL*>* distinctURLs = @[ distinctLegalURL ];
  [controller_ setFooterText:footerText urls:distinctURLs];

  linkFooterView = GetLinkFooterView(/*section=*/1);
  EXPECT_EQ(originalURLs[0], linkFooterView.urls[0]);
}
