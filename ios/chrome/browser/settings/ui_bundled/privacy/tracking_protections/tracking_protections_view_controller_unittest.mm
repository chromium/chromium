// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/privacy/tracking_protections/tracking_protections_view_controller.h"

#import "components/strings/grit/privacy_sandbox_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

class TrackingProtectionsViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    TrackingProtectionsViewController* view_controller =
        [[TrackingProtectionsViewController alloc]
            initWithStyle:ChromeTableViewStyle()];
    return view_controller;
  }

  web::WebTaskEnvironment task_environment_;
};

TEST_F(TrackingProtectionsViewControllerTest, VerifySections) {
  CreateController();
  CheckController();

  CheckTitle(
      l10n_util::GetNSString(IDS_INCOGNITO_TRACKING_PROTECTIONS_PAGE_TITLE));
  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));

  TableViewMultiDetailTextItem* scriptBlockingItem =
      static_cast<TableViewMultiDetailTextItem*>(GetTableViewItem(0, 0));
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_FINGERPRINTING_PROTECTION_LINK_ROW_LABEL),
      scriptBlockingItem.text);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_FINGERPRINTING_PROTECTION_LINK_ROW_SUBLABEL),
      scriptBlockingItem.leadingDetailText);
  CheckSectionHeaderWithId(IDS_INCOGNITO_TRACKING_PROTECTIONS_DESCRIPTION_IOS,
                           0);
}
