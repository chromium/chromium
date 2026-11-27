// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/manual_fill/ui/expanded_manual_fill_header_view.h"

#import <UIKit/UIKit.h>

#import "base/test/metrics/user_action_tester.h"
#import "ios/chrome/browser/autofill/manual_fill/public/manual_fill_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

using manual_fill::ManualFillDataType;

class ExpandedManualFillHeaderViewTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    mock_delegate_ =
        OCMStrictProtocolMock(@protocol(ExpandedManualFillHeaderViewDelegate));
    header_view_ = [[ExpandedManualFillHeaderView alloc]
        initWithDataType:ManualFillDataType::kCredential];
    header_view_.delegate = mock_delegate_;
  }

  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(mock_delegate_);
    PlatformTest::TearDown();
  }

  ExpandedManualFillHeaderView* header_view_;
  id mock_delegate_;
};

// Tests that the header view configures its Chrome logo, top container, close
// button, and segmented control, and forwards close and segment events.
TEST_F(ExpandedManualFillHeaderViewTest,
       ConfiguresControlsAndForwardsInteractions) {
  header_view_.frame = CGRectMake(0, 0, 390, 96);
  header_view_.horizontalInset = 20.0;
  [header_view_ layoutIfNeeded];

  EXPECT_NSEQ(header_view_.accessibilityIdentifier,
              manual_fill::kExpandedManualFillHeaderViewID);
  UIView* top_view = chrome_test_util::FindViewById(
      header_view_, manual_fill::kExpandedManualFillHeaderTopViewID);
  UIView* chrome_logo = chrome_test_util::FindViewById(
      header_view_, manual_fill::kExpandedManualFillChromeLogoID);
  ASSERT_NE(top_view, nil);
  ASSERT_NE(chrome_logo, nil);
  EXPECT_EQ(header_view_.preferredAccessibilityFocusView, chrome_logo);
  EXPECT_EQ(header_view_.segmentedControl.selectedSegmentIndex, 0);

  OCMExpect([mock_delegate_ headerView:header_view_
                     didSelectDataType:ManualFillDataType::kAddress]);
  header_view_.segmentedControl.selectedSegmentIndex = 2;
  [header_view_.segmentedControl
      sendActionsForControlEvents:UIControlEventValueChanged];

  NSString* close_label = l10n_util::GetNSString(
      IDS_IOS_EXPANDED_MANUAL_FILL_CLOSE_BUTTON_ACCESSIBILITY_LABEL);
  UIButton* close_button = chrome_test_util::FindViewWithPredicate<UIButton>(
      header_view_, ^BOOL(UIView* candidate) {
        return [candidate isKindOfClass:[UIButton class]] &&
               [candidate.accessibilityLabel isEqualToString:close_label];
      });
  ASSERT_NE(close_button, nil);

  base::UserActionTester user_action_tester;
  OCMExpect([mock_delegate_ headerView:header_view_
                   didPressCloseButton:close_button]);
  [close_button sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(user_action_tester.GetActionCount("ManualFallback_Close"), 1);
}
