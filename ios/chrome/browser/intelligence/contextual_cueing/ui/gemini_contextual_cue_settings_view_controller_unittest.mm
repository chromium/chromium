// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_constants.h"
#import "ios/chrome/browser/intelligence/contextual_cueing/ui/gemini_contextual_cue_settings_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util_mac.h"

// Test fixture for `GeminiContextualCueSettingsViewController`.
class GeminiContextualCueSettingsViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[GeminiContextualCueSettingsViewController alloc] init];
    mock_mutator_ =
        OCMStrictProtocolMock(@protocol(GeminiContextualCueSettingsMutator));
    mock_presentation_delegate_ = OCMStrictProtocolMock(@protocol(
        GeminiContextualCueSettingsViewControllerPresentationDelegate));
    view_controller_.mutator = mock_mutator_;
    view_controller_.presentationDelegate = mock_presentation_delegate_;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mock_mutator_);
    EXPECT_OCMOCK_VERIFY(mock_presentation_delegate_);
    PlatformTest::TearDown();
  }

  GeminiContextualCueSettingsViewController* view_controller_;
  id mock_mutator_;
  id mock_presentation_delegate_;
};

// Tests that the table view loads two sections with one row each and sets the
// Done button accessibility identifier.
TEST_F(GeminiContextualCueSettingsViewControllerTest, LoadsTableStructure) {
  [view_controller_ loadViewIfNeeded];

  EXPECT_NSEQ(kGeminiContextualCueSettingsTableViewAccessibilityIdentifier,
              view_controller_.tableView.accessibilityIdentifier);
  EXPECT_NSEQ(kGeminiContextualCueSettingsDoneButtonAccessibilityIdentifier,
              view_controller_.navigationItem.rightBarButtonItem
                  .accessibilityIdentifier);
  EXPECT_EQ(2, [view_controller_.tableView numberOfSections]);
  EXPECT_EQ(1, [view_controller_.tableView numberOfRowsInSection:0]);
  EXPECT_EQ(1, [view_controller_.tableView numberOfRowsInSection:1]);
}

// Tests that selecting the "Manage Gemini suggestions" and "Edit prompt" rows
// forwards the actions to the mutator.
TEST_F(GeminiContextualCueSettingsViewControllerTest,
       SelectingRowsForwardsToMutator) {
  [view_controller_ loadViewIfNeeded];

  OCMExpect([mock_mutator_ didTapManageSuggestions]);
  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];

  OCMExpect([mock_mutator_ didTapEditPrompt]);
  [view_controller_ tableView:view_controller_.tableView
      didSelectRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:1]];
}

// Tests that setting a non-empty prompt displays a localized quoted footer in
// the "Edit prompt" section.
TEST_F(GeminiContextualCueSettingsViewControllerTest,
       DisplaysLocalizedQuotedPromptFooter) {
  [view_controller_ setPrompt:@"Summarize this page"];
  [view_controller_ loadViewIfNeeded];

  UIView* section0_footer =
      [view_controller_ tableView:view_controller_.tableView
           viewForFooterInSection:0];
  EXPECT_EQ(nil, section0_footer);

  UIView* section1_footer =
      [view_controller_ tableView:view_controller_.tableView
           viewForFooterInSection:1];
  ASSERT_NE(nil, section1_footer);
  EXPECT_EQ(UITableViewAutomaticDimension,
            [view_controller_ tableView:view_controller_.tableView
                heightForFooterInSection:1]);
}
