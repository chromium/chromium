// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/suggestions_from_gemini/ui/suggestions_from_gemini_help_improve_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/test/metrics/user_action_tester.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

class SuggestionsFromGeminiHelpImproveTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  LegacyChromeTableViewController* InstantiateController() override {
    SuggestionsFromGeminiHelpImproveTableViewController* viewController =
        [[SuggestionsFromGeminiHelpImproveTableViewController alloc] init];
    [viewController setSuggestionsFromGeminiPolicyState:policyState_];
    return viewController;
  }

  void CheckDetailIconItem(int section, int item, int expected_string_id) {
    TableViewDetailIconItem* detail_item =
        base::apple::ObjCCastStrict<TableViewDetailIconItem>(
            GetTableViewItem(section, item));
    EXPECT_NSEQ(l10n_util::GetNSString(expected_string_id), detail_item.text);
  }

  SuggestionsFromGeminiPolicyState policyState_ =
      SuggestionsFromGeminiPolicyState::kFullyAllowed;
};

// Tests that the SuggestionsFromGeminiHelpImproveTableViewController is
// correctly initialized and displays the details.
TEST_F(SuggestionsFromGeminiHelpImproveTableViewControllerTest,
       TestInitialization) {
  CreateController();
  CheckController();

  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE),
      controller().title);

  EXPECT_EQ(3, NumberOfSections());

  // Section 0: Helping Improve
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  TableViewModel* model = controller().tableViewModel;
  EXPECT_EQ(nil, [model headerForSectionIndex:0]);

  TableViewTextItem* textItem =
      base::apple::ObjCCastStrict<TableViewTextItem>(GetTableViewItem(0, 0));
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_TITLE),
      textItem.text);
  EXPECT_EQ(UITableViewCellSelectionStyleNone, textItem.selectionStyle);

  TableViewTextHeaderFooterItem* footer0 =
      base::apple::ObjCCastStrict<TableViewTextHeaderFooterItem>(
          [model footerForSectionIndex:0]);
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_QUALITY_LOGGING_SUBTITLE),
      footer0.subtitle);

  // Section 1: When Used
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  CheckDetailIconItem(1, 0, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_WHEN_USED_1);
  CheckDetailIconItem(1, 1, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_WHEN_USED_2);

  // Section 2: Things to Consider
  EXPECT_EQ(2, NumberOfItemsInSection(2));
  CheckDetailIconItem(2, 0, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_1);
  CheckDetailIconItem(2, 1, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_2);
}

// Tests that the SuggestionsFromGeminiHelpImproveTableViewController is
// correctly initialized with TableViewInfoButtonItem when autofill prediction
// improvements is allowed without logging by policy.
TEST_F(SuggestionsFromGeminiHelpImproveTableViewControllerTest,
       TestInitializationWhenPredictionImprovementsLoggingDisabledByPolicy) {
  policyState_ = SuggestionsFromGeminiPolicyState::kLoggingDisabled;
  CreateController();
  CheckController();

  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE),
      controller().title);

  EXPECT_EQ(3, NumberOfSections());

  // Section 0: Helping Improve (should have info button item)
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  TableViewModel* model = controller().tableViewModel;
  EXPECT_EQ(nil, [model headerForSectionIndex:0]);

  TableViewInfoButtonItem* infoButtonItem =
      base::apple::ObjCCastStrict<TableViewInfoButtonItem>(
          GetTableViewItem(0, 0));
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE),
      infoButtonItem.text);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              infoButtonItem.statusText);
  EXPECT_EQ(UITableViewCellSelectionStyleNone, infoButtonItem.selectionStyle);

  TableViewTextHeaderFooterItem* footer0 =
      base::apple::ObjCCastStrict<TableViewTextHeaderFooterItem>(
          [model footerForSectionIndex:0]);
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_QUALITY_LOGGING_SUBTITLE),
      footer0.subtitle);

  // Section 1: When Used
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  CheckDetailIconItem(1, 0, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_WHEN_USED_1);
  CheckDetailIconItem(1, 1, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_WHEN_USED_2);

  // Section 2: Things to Consider (should have the third enterprise disclaimer
  // item)
  EXPECT_EQ(3, NumberOfItemsInSection(2));
  CheckDetailIconItem(2, 0, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_1);
  CheckDetailIconItem(2, 1, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_2);
  CheckDetailIconItem(2, 2, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_3);
}

// Tests that the SuggestionsFromGeminiHelpImproveTableViewController is
// correctly initialized with TableViewInfoButtonItem when autofill prediction
// improvements is fully disabled by policy.
TEST_F(SuggestionsFromGeminiHelpImproveTableViewControllerTest,
       TestInitializationWhenPredictionImprovementsFullyDisabledByPolicy) {
  policyState_ = SuggestionsFromGeminiPolicyState::kFullyDisabled;
  CreateController();
  CheckController();

  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE),
      controller().title);

  EXPECT_EQ(3, NumberOfSections());

  // Section 0: Helping Improve (should have info button item)
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  TableViewModel* model = controller().tableViewModel;
  EXPECT_EQ(nil, [model headerForSectionIndex:0]);

  TableViewInfoButtonItem* infoButtonItem =
      base::apple::ObjCCastStrict<TableViewInfoButtonItem>(
          GetTableViewItem(0, 0));
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_IOS_PERSONAL_CONTEXT_AUTOFILL_SETTINGS_HELPING_IMPROVE_NOTICE_TITLE),
      infoButtonItem.text);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_SETTING_OFF),
              infoButtonItem.statusText);
  EXPECT_EQ(UITableViewCellSelectionStyleNone, infoButtonItem.selectionStyle);

  TableViewTextHeaderFooterItem* footer0 =
      base::apple::ObjCCastStrict<TableViewTextHeaderFooterItem>(
          [model footerForSectionIndex:0]);
  EXPECT_NSEQ(
      l10n_util::GetNSString(
          IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_QUALITY_LOGGING_SUBTITLE),
      footer0.subtitle);

  // Section 1: When Used
  EXPECT_EQ(2, NumberOfItemsInSection(1));
  CheckDetailIconItem(1, 0, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_WHEN_USED_1);
  CheckDetailIconItem(1, 1, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_WHEN_USED_2);

  // Section 2: Things to Consider (should have the third enterprise disclaimer
  // item)
  EXPECT_EQ(3, NumberOfItemsInSection(2));
  CheckDetailIconItem(2, 0, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_1);
  CheckDetailIconItem(2, 1, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_2);
  CheckDetailIconItem(2, 2, IDS_SETTINGS_SUGGESTIONS_FROM_GEMINI_CONSIDER_3);
}

// Tests that the view controller reports the expected user action when
// dismissed.
TEST_F(SuggestionsFromGeminiHelpImproveTableViewControllerTest,
       TestReportDismissalUserAction) {
  base::UserActionTester user_action_tester;
  SuggestionsFromGeminiHelpImproveTableViewController* helpImproveController =
      base::apple::ObjCCastStrict<
          SuggestionsFromGeminiHelpImproveTableViewController>(controller());
  [helpImproveController reportDismissalUserAction];
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SuggestionsFromGeminiHelpImproveClose"));
}

// Tests that the view controller reports the expected user action when going
// back.
TEST_F(SuggestionsFromGeminiHelpImproveTableViewControllerTest,
       TestReportBackUserAction) {
  base::UserActionTester user_action_tester;
  SuggestionsFromGeminiHelpImproveTableViewController* helpImproveController =
      base::apple::ObjCCastStrict<
          SuggestionsFromGeminiHelpImproveTableViewController>(controller());
  [helpImproveController reportBackUserAction];
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SuggestionsFromGeminiHelpImproveBack"));
}

}  // namespace
