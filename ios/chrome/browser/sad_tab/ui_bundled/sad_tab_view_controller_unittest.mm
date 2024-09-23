// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sad_tab/ui_bundled/sad_tab_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

// Test fixture for testing SadTabViewController class.
class SadTabViewControllerTest : public PlatformTest {
 protected:
  SadTabViewControllerTest()
      : view_controller_([[SadTabViewController alloc] init]) {}
  SadTabViewController* view_controller_;
};

// Tests Sad Tab message and button title for first failure in non-incognito
// mode.
TEST_F(SadTabViewControllerTest, FirstFailureInNonIncognitoText) {
  view_controller_.repeatedFailure = NO;
  view_controller_.offTheRecord = NO;
  [view_controller_ loadViewIfNeeded];

  ASSERT_TRUE(view_controller_.messageTextView);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SAD_TAB_MESSAGE),
              view_controller_.messageTextView.text);

  ASSERT_TRUE(view_controller_.actionButton);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_LABEL).uppercaseString,
              view_controller_.actionButton.configuration.title);
}

// Tests Sad Tab message and button title for first failure in incognito
// mode.
TEST_F(SadTabViewControllerTest, FirstFailureInIncognitoText) {
  view_controller_.repeatedFailure = NO;
  view_controller_.offTheRecord = YES;
  [view_controller_ loadViewIfNeeded];

  ASSERT_TRUE(view_controller_.messageTextView);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SAD_TAB_MESSAGE),
              view_controller_.messageTextView.text);

  ASSERT_TRUE(view_controller_.actionButton);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_LABEL).uppercaseString,
              view_controller_.actionButton.configuration.title);
}

// Tests Sad Tab message and button title for repeated failure in non-incognito
// mode.
TEST_F(SadTabViewControllerTest, RepeatedFailureInNonIncognitoText) {
  view_controller_.repeatedFailure = YES;
  view_controller_.offTheRecord = NO;
  [view_controller_ loadViewIfNeeded];

  ASSERT_TRUE(view_controller_.messageTextView);
  EXPECT_TRUE([view_controller_.messageTextView.text
      containsString:l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_TRY)]);
  EXPECT_TRUE([view_controller_.messageTextView.text
      containsString:l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_INCOGNITO)]);
  EXPECT_TRUE([view_controller_.messageTextView.text
      containsString:l10n_util::GetNSString(
                         IDS_SAD_TAB_RELOAD_RESTART_BROWSER)]);
  EXPECT_TRUE([view_controller_.messageTextView.text
      containsString:l10n_util::GetNSString(
                         IDS_SAD_TAB_RELOAD_RESTART_DEVICE)]);

  ASSERT_TRUE(view_controller_.actionButton);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_SAD_TAB_SEND_FEEDBACK_LABEL).uppercaseString,
      view_controller_.actionButton.configuration.title);
}

// Tests Sad Tab message and button title for repeated failure in incognito
// mode.
TEST_F(SadTabViewControllerTest, RepeatedFailureInIncognitoText) {
  view_controller_.repeatedFailure = YES;
  view_controller_.offTheRecord = YES;
  [view_controller_ loadViewIfNeeded];

  ASSERT_TRUE(view_controller_.messageTextView);
  EXPECT_TRUE([view_controller_.messageTextView.text
      containsString:l10n_util::GetNSString(IDS_SAD_TAB_RELOAD_TRY)]);
  EXPECT_TRUE([view_controller_.messageTextView.text
      containsString:l10n_util::GetNSString(
                         IDS_SAD_TAB_RELOAD_RESTART_BROWSER)]);
  EXPECT_TRUE([view_controller_.messageTextView.text
      containsString:l10n_util::GetNSString(
                         IDS_SAD_TAB_RELOAD_RESTART_DEVICE)]);

  ASSERT_TRUE(view_controller_.actionButton);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_SAD_TAB_SEND_FEEDBACK_LABEL).uppercaseString,
      view_controller_.actionButton.configuration.title);
}

// Tests action button tap for first failure.
TEST_F(SadTabViewControllerTest, FirstFailureAction) {
  id delegate = OCMStrictProtocolMock(@protocol(SadTabViewControllerDelegate));
  OCMExpect([delegate sadTabViewControllerReload:view_controller_]);

  view_controller_.repeatedFailure = NO;
  view_controller_.delegate = delegate;
  [view_controller_ loadViewIfNeeded];

  ASSERT_TRUE(view_controller_.actionButton);
  [view_controller_.actionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests action button tap for repeated failure.
TEST_F(SadTabViewControllerTest, RepeatedFailureAction) {
  id delegate = OCMStrictProtocolMock(@protocol(SadTabViewControllerDelegate));
  OCMExpect([delegate sadTabViewControllerShowReportAnIssue:view_controller_]);

  view_controller_.repeatedFailure = YES;
  view_controller_.delegate = delegate;
  [view_controller_ loadViewIfNeeded];

  ASSERT_TRUE(view_controller_.actionButton);
  [view_controller_.actionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_OCMOCK_VERIFY(delegate);
}
