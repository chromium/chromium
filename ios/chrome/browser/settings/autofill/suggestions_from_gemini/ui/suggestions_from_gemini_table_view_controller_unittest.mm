// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

@interface SuggestionsFromGeminiTableViewController (Testing)
- (void)personalContextSwitchChanged:(UISwitch*)switchView;
@end

namespace {

class SuggestionsFromGeminiTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    return [[SuggestionsFromGeminiTableViewController alloc] init];
  }
};

// Tests that the SuggestionsFromGemini subpage displays the switch and the
// Manage Connected Apps row.
TEST_F(SuggestionsFromGeminiTableViewControllerTest, TestInitialization) {
  CreateController();
  CheckController();

  EXPECT_EQ(2, NumberOfSections());
  EXPECT_EQ(2, NumberOfItemsInSection(0));
  EXPECT_EQ(1, NumberOfItemsInSection(1));

  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(GetTableViewItem(0, 0));
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_SWITCH_TITLE),
              switchItem.text);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_SWITCH_SUMMARY),
              switchItem.detailText);
  EXPECT_FALSE(switchItem.on);

  TableViewDetailTextItem* item =
      base::apple::ObjCCastStrict<TableViewDetailTextItem>(
          GetTableViewItem(0, 1));
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

  TableViewDetailTextItem* helpImproveItem =
      base::apple::ObjCCastStrict<TableViewDetailTextItem>(
          GetTableViewItem(1, 0));
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE),
      helpImproveItem.text);
  EXPECT_EQ(nil, helpImproveItem.detailText);
  EXPECT_EQ(UITableViewCellAccessoryDisclosureIndicator,
            helpImproveItem.accessoryType);
}

// Tests that setting the switch state via the consumer interface updates the
// switch cell.
TEST_F(SuggestionsFromGeminiTableViewControllerTest, TestConsumerSetsSwitchOn) {
  CreateController();
  CheckController();

  SuggestionsFromGeminiTableViewController* geminiController =
      base::apple::ObjCCastStrict<SuggestionsFromGeminiTableViewController>(
          controller());
  [geminiController setSuggestionsFromGeminiSwitchOn:YES];

  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(GetTableViewItem(0, 0));
  EXPECT_TRUE(switchItem.on);
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

  // Verify switch change triggers mutator.
  OCMExpect([mockMutator didToggleSuggestionsFromGeminiSwitch:NO]);
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(GetTableViewItem(0, 0));
  EXPECT_EQ(switchItem.target, controller());
  EXPECT_NSEQ(NSStringFromSelector(switchItem.selector),
              NSStringFromSelector(@selector(personalContextSwitchChanged:)));

  UISwitch* switchView = [[UISwitch alloc] init];
  switchView.on = NO;
  [geminiController personalContextSwitchChanged:switchView];
  EXPECT_OCMOCK_VERIFY(mockMutator);

  // Verify selecting row triggers didSelectManageConnectedApps on mutator.
  OCMExpect([mockMutator didSelectManageConnectedApps]);
  NSIndexPath* indexPath = [NSIndexPath indexPathForRow:1 inSection:0];
  [geminiController tableView:geminiController.tableView
      didSelectRowAtIndexPath:indexPath];
  EXPECT_OCMOCK_VERIFY(mockMutator);

  // Verify selecting Help Improve row triggers mutator.
  OCMExpect([mockMutator didSelectHelpImprove]);
  NSIndexPath* helpImproveIndexPath = [NSIndexPath indexPathForRow:0
                                                         inSection:1];
  [geminiController tableView:geminiController.tableView
      didSelectRowAtIndexPath:helpImproveIndexPath];
  EXPECT_OCMOCK_VERIFY(mockMutator);
}

// Tests that the view controller reports the expected user action when
// dismissed.
TEST_F(SuggestionsFromGeminiTableViewControllerTest,
       TestReportDismissalUserAction) {
  base::UserActionTester user_action_tester;
  SuggestionsFromGeminiTableViewController* geminiController =
      base::apple::ObjCCastStrict<SuggestionsFromGeminiTableViewController>(
          controller());
  [geminiController reportDismissalUserAction];
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SuggestionsFromGeminiSettingsClose"));
}

// Tests that the view controller reports the expected user action when going
// back.
TEST_F(SuggestionsFromGeminiTableViewControllerTest, TestReportBackUserAction) {
  base::UserActionTester user_action_tester;
  SuggestionsFromGeminiTableViewController* geminiController =
      base::apple::ObjCCastStrict<SuggestionsFromGeminiTableViewController>(
          controller());
  [geminiController reportBackUserAction];
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SuggestionsFromGeminiSettingsBack"));
}

}  // namespace
