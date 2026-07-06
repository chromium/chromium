// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_sharing/sharing_status_view_controller.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/run_until.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_sharing/password_sharing_metrics.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Helper to recursively find a view with a given accessibility identifier.
UIView* FindViewWithID(UIView* view, NSString* identifier) {
  if ([view.accessibilityIdentifier isEqualToString:identifier]) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* foundView = FindViewWithID(subview, identifier);
    if (foundView) {
      return foundView;
    }
  }
  return nil;
}

class SharingStatusViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    [UIView setAnimationsEnabled:NO];
  }

  void TearDown() override {
    [UIView setAnimationsEnabled:YES];
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
};

TEST_F(SharingStatusViewControllerTest, TestCancellation) {
  base::HistogramTester histogram_tester;
  SharingStatusViewController* view_controller =
      [[SharingStatusViewController alloc] initWithNibName:nil bundle:nil];
  [view_controller loadViewIfNeeded];

  // Find the subviews.
  UIButton* cancelButton = static_cast<UIButton*>(
      FindViewWithID(view_controller.view, kSharingStatusCancelButtonID));
  UILabel* titleLabel = static_cast<UILabel*>(
      FindViewWithID(view_controller.view, kSharingStatusTitleLabelID));
  ASSERT_TRUE(cancelButton);
  ASSERT_TRUE(titleLabel);

  // Verify initial state.
  EXPECT_FALSE(cancelButton.isHidden);
  EXPECT_NSEQ(
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_STATUS_PROGRESS_TITLE),
      titleLabel.text);

  // Trigger the cancel button.
  [cancelButton sendActionsForControlEvents:UIControlEventTouchUpInside];

  // Wait for the cancellation to complete.
  EXPECT_TRUE(base::test::RunUntil([&]() { return cancelButton.isHidden; }));
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSWORD_SHARING_CANCELLED_TITLE),
              titleLabel.text);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSharingIOS.UserAction",
      PasswordSharingInteraction::kSharingConfirmationCancelClicked, 1);
}

}  // namespace
