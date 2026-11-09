// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
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
    UIView* footer_view = [controller_ tableView:controller_.tableView
                          viewForFooterInSection:section];
    EXPECT_TRUE(
        [footer_view isKindOfClass:[TableViewLinkHeaderFooterView class]]);
    return base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(
        footer_view);
  }

  AutofillAISaveEntityTableViewController* controller_;
};

// Tests that the table view controller displays the new and old sections.
TEST_F(AutofillAISaveEntityTableViewControllerTest, DisplayNewAndOldSections) {
  autofill::EntityInstance new_entity = GetTestVehicle(kCarMaker2);
  autofill::EntityInstance old_entity = GetTestVehicle(kCarMaker1);

  [controller_ setNewEntity:std::move(new_entity)
                  oldEntity:std::move(old_entity)];

  [controller_ loadViewIfNeeded];

  UITableViewDiffableDataSource<NSNumber*, TableViewItem*>* data_source =
      base::apple::ObjCCastStrict<
          UITableViewDiffableDataSource<NSNumber*, TableViewItem*>>(
          controller_.tableView.dataSource);

  // Verify New Info Section.
  NSIndexPath* index_path = [NSIndexPath indexPathForRow:0 inSection:0];
  TableViewTextEditItem* new_item =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(
          [data_source itemIdentifierForIndexPath:index_path]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(kCarMaker2), new_item.textFieldValue);

  // Verify Old Info Section.
  index_path = [NSIndexPath indexPathForRow:0 inSection:1];
  TableViewTextEditItem* old_item =
      base::apple::ObjCCastStrict<TableViewTextEditItem>(
          [data_source itemIdentifierForIndexPath:index_path]);
  EXPECT_NSEQ(base::SysUTF16ToNSString(kCarMaker1), old_item.textFieldValue);

  // No footer view is returned when `footer_text` has not been set.
  EXPECT_EQ(nil, [controller_ tableView:controller_.tableView
                     viewForFooterInSection:2]);
}

// Tests that the table view controller displays the footer text and URLs pushed
// via `setFooterText:urls:`.
TEST_F(AutofillAISaveEntityTableViewControllerTest,
       DisplayFooterWithTextAndUrls) {
  [controller_ setNewEntity:GetTestVehicle(kCarMaker2) oldEntity:std::nullopt];

  NSString* footer_text = @"Test wallet footer with BEGIN_LINKlinkEND_LINK";
  NSArray<CrURL*>* urls =
      @[ [[CrURL alloc] initWithGURL:GURL("https://example.com")] ];
  [controller_ setFooterText:footer_text urls:urls];

  [controller_ loadViewIfNeeded];

  // The footer section index is 1 if there is only new entity.
  TableViewLinkHeaderFooterView* link_footer_view =
      GetLinkFooterView(/*section=*/1);

  UITextView* text_view = chrome_test_util::FindViewByClass<UITextView>(
      link_footer_view.contentView);
  ASSERT_NE(nil, text_view);
  EXPECT_NSEQ(@"Test wallet footer with link", text_view.attributedText.string);
  ASSERT_EQ(1U, link_footer_view.urls.count);
  EXPECT_EQ(GURL("https://example.com"), link_footer_view.urls[0].gurl);
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

  CrURL* legal_url =
      [[CrURL alloc] initWithGURL:GURL("https://www.example.com/legal")];
  [controller_ setFooterText:@"Saved locally.\n\nBEGIN_LINKTestEND_LINK legal"
                        urls:@[ legal_url ]];

  TableViewLinkHeaderFooterView* link_footer_view =
      GetLinkFooterView(/*section=*/1);
  UITextView* text_view = chrome_test_util::FindViewByClass<UITextView>(
      link_footer_view.contentView);
  ASSERT_NE(nil, text_view);
  EXPECT_NSEQ(@"Saved locally.\n\nTest legal", text_view.attributedText.string);
  ASSERT_EQ(1U, link_footer_view.urls.count);
  EXPECT_EQ(GURL("https://www.example.com/legal"),
            link_footer_view.urls[0].gurl);
}

// Tests that setting identical footer text and URLs (with distinct CrURL
// instances having identical GURL values) is a no-op and does not replace the
// existing URLs array.
TEST_F(AutofillAISaveEntityTableViewControllerTest,
       SetFooterWithIdenticalContentDoesNotUpdate) {
  [controller_ setNewEntity:GetTestVehicle(kCarMaker2) oldEntity:std::nullopt];
  [controller_ loadViewIfNeeded];

  NSString* footer_text = @"Saved locally.\n\nBEGIN_LINKTestEND_LINK legal";
  CrURL* legal_url =
      [[CrURL alloc] initWithGURL:GURL("https://www.example.com/legal")];
  NSArray<CrURL*>* original_urls = @[ legal_url ];
  [controller_ setFooterText:footer_text urls:original_urls];

  TableViewLinkHeaderFooterView* link_footer_view =
      GetLinkFooterView(/*section=*/1);
  EXPECT_EQ(original_urls[0], link_footer_view.urls[0]);

  CrURL* distinct_legal_url =
      [[CrURL alloc] initWithGURL:GURL("https://www.example.com/legal")];
  NSArray<CrURL*>* distinct_urls = @[ distinct_legal_url ];
  [controller_ setFooterText:footer_text urls:distinct_urls];

  link_footer_view = GetLinkFooterView(/*section=*/1);
  EXPECT_EQ(original_urls[0], link_footer_view.urls[0]);
}
