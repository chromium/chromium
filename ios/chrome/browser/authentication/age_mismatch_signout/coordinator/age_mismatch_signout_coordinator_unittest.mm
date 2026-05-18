// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/age_mismatch_signout_coordinator.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/metrics/user_action_tester.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/age_mismatch_signout_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

class AgeMismatchSignoutCoordinatorTest : public PlatformTest {
 public:
  AgeMismatchSignoutCoordinatorTest()
      : PlatformTest(),
        profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile_.get())) {
    mock_identity_ = OCMProtocolMock(@protocol(SystemIdentity));
    OCMStub([mock_identity_ userFullName]).andReturn(@"Test Name");
    OCMStub([mock_identity_ userEmail]).andReturn(@"test@example.com");
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  id<SystemIdentity> mock_identity_;
};

TEST_F(AgeMismatchSignoutCoordinatorTest, TapPrimaryAction) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  AgeMismatchSignoutCoordinator* coordinator =
      [[AgeMismatchSignoutCoordinator alloc]
          initWithBaseViewController:nil
                             browser:browser_.get()
                            identity:mock_identity_
                                mode:AgeMismatchPromptMode::kStandard];

  [coordinator start];

  histogram_tester.ExpectBucketCount(kAgeMismatchSignoutPromptModeHistogram,
                                     AgeMismatchPromptMode::kStandard, 1);

  id<PromoStyleViewControllerDelegate> delegate =
      (id<PromoStyleViewControllerDelegate>)coordinator;

  // Tap Learn more.
  [delegate
      didTapURLInDisclaimer:[NSURL
                                URLWithString:kAgeMismatchSignoutLearnMoreURL]];

  // Tap the primary action button.
  [delegate didTapPrimaryActionButton];

  EXPECT_EQ(
      1, user_action_tester.GetActionCount(kAgeMismatchSignoutLearnMoreAction));
  histogram_tester.ExpectBucketCount(
      kAgeMismatchSignoutActionHistogram,
      AgeMismatchSignoutAction::kUseAnotherAccount, 1);
  histogram_tester.ExpectBucketCount(
      kAgeMismatchSignoutActionHistogram,
      AgeMismatchSignoutAction::kUseWithoutAccount, 0);

  [coordinator stop];
}

TEST_F(AgeMismatchSignoutCoordinatorTest, TapSecondaryAction) {
  base::HistogramTester histogram_tester;
  AgeMismatchSignoutCoordinator* coordinator =
      [[AgeMismatchSignoutCoordinator alloc]
          initWithBaseViewController:nil
                             browser:browser_.get()
                            identity:mock_identity_
                                mode:AgeMismatchPromptMode::kStandard];

  [coordinator start];

  id<PromoStyleViewControllerDelegate> delegate =
      (id<PromoStyleViewControllerDelegate>)coordinator;

  // Tap the secondary button.
  [delegate didTapSecondaryActionButton];

  histogram_tester.ExpectBucketCount(
      kAgeMismatchSignoutActionHistogram,
      AgeMismatchSignoutAction::kUseWithoutAccount, 1);
  histogram_tester.ExpectBucketCount(
      kAgeMismatchSignoutActionHistogram,
      AgeMismatchSignoutAction::kUseAnotherAccount, 0);

  [coordinator stop];
}
