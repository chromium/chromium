// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

class SuggestionsFromGeminiTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    return [[SuggestionsFromGeminiTableViewController alloc] init];
  }
};

// Tests that the SuggestionsFromGemini subpage displays the Manage Connected
// Apps row.
TEST_F(SuggestionsFromGeminiTableViewControllerTest, TestInitialization) {
  CreateController();
  CheckController();

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(1, NumberOfItemsInSection(0));

  TableViewDetailTextItem* item =
      base::apple::ObjCCastStrict<TableViewDetailTextItem>(
          GetTableViewItem(0, 0));
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_TITLE),
      controller().title);
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_MANAGE_CONNECTED_APPS_TITLE),
      item.text);
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_MANAGE_CONNECTED_APPS_SUMMARY),
      item.detailText);
  EXPECT_EQ(TableViewDetailTextCellAccessorySymbolExternalLink,
            item.accessorySymbol);
}

// Tests that view controller actions correctly invoke mutators.
TEST_F(SuggestionsFromGeminiTableViewControllerTest, TestMutatorInteraction) {
  CreateController();
  CheckController();

  id mockMutator = OCMProtocolMock(@protocol(SuggestionsFromGeminiMutator));
  SuggestionsFromGeminiTableViewController* geminiController =
      base::apple::ObjCCastStrict<SuggestionsFromGeminiTableViewController>(
          controller());
  geminiController.mutator = mockMutator;

  // Verify selecting row triggers didSelectManageConnectedApps on mutator.
  OCMExpect([mockMutator didSelectManageConnectedApps]);
  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:0 inSection:0];
  [geminiController tableView:geminiController.tableView
      didSelectRowAtIndexPath:indexPath];
  EXPECT_OCMOCK_VERIFY(mockMutator);
}

}  // namespace
