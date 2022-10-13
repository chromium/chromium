// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/commands/promos_manager_commands.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"
#import "ios/chrome/browser/ui/post_restore_signin/metrics.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kFakePreRestoreAccountEmail[] = "person@example.org";
const char kFakePreRestoreAccountGivenName[] = "Given";
const char kFakePreRestoreAccountFullName[] = "Full Name";
}  // namespace

// Tests the PostRestoreSignInProvider.
class PostRestoreSignInProviderTest : public PlatformTest {
 public:
  explicit PostRestoreSignInProviderTest() {
    SetFakePreRestoreAccountInfo();
    provider_ = [[PostRestoreSignInProvider alloc] init];
  }

  void SetFakePreRestoreAccountInfo() {
    AccountInfo accountInfo;
    accountInfo.email = std::string(kFakePreRestoreAccountEmail);
    accountInfo.given_name = std::string(kFakePreRestoreAccountGivenName);
    accountInfo.full_name = std::string(kFakePreRestoreAccountFullName);
    StorePreRestoreIdentity(local_state_.Get(), accountInfo);
  }

  void ClearUserName() {
    AccountInfo accountInfo;
    accountInfo.email = std::string(kFakePreRestoreAccountEmail);
    StorePreRestoreIdentity(local_state_.Get(), accountInfo);
    // Reinstantiate a provider so that it picks up the changes.
    provider_ = [[PostRestoreSignInProvider alloc] init];
  }

  void SetupMockHandler() {
    mock_handler_ = OCMProtocolMock(@protocol(PromosManagerCommands));
    provider_.handler = mock_handler_;
  }

  void EnableFeatureVariationFullscreen() {
    scoped_feature_list_.InitAndEnableFeature(
        post_restore_signin::features::kIOSNewPostRestoreExperience);
  }

  void EnableFeatureVariationAlert() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::FeatureRefAndParams(
            post_restore_signin::features::kIOSNewPostRestoreExperience,
            {{post_restore_signin::features::kIOSNewPostRestoreExperienceParam,
              "true"}})},
        {});
  }

  IOSChromeScopedTestingLocalState local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mock_handler_;
  PostRestoreSignInProvider* provider_;
};

TEST_F(PostRestoreSignInProviderTest, hasIdentifierFullscreen) {
  EnableFeatureVariationFullscreen();
  EXPECT_EQ(provider_.identifier,
            promos_manager::Promo::PostRestoreSignInFullscreen);
}

TEST_F(PostRestoreSignInProviderTest, hasIdentifierAlert) {
  EnableFeatureVariationAlert();
  EXPECT_EQ(provider_.identifier,
            promos_manager::Promo::PostRestoreSignInAlert);
}

TEST_F(PostRestoreSignInProviderTest, standardPromoAlertDefaultAction) {
  EnableFeatureVariationFullscreen();
  SetupMockHandler();
  OCMExpect([mock_handler_ showSignin:[OCMArg any]]);
  [provider_ standardPromoAlertDefaultAction];
  [mock_handler_ verify];
}

TEST_F(PostRestoreSignInProviderTest, title) {
  EnableFeatureVariationFullscreen();
  EXPECT_TRUE([[provider_ title] isEqualToString:@"Chrome is Signed Out"]);
}

TEST_F(PostRestoreSignInProviderTest, message) {
  EnableFeatureVariationFullscreen();
  NSString* expected;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    expected = @"You were signed out of your account person@example.org as "
               @"part of your iPad reset. Tap continue below to sign in.";
  } else {
    expected = @"You were signed out of your account person@example.org as "
               @"part of your iPhone reset. Tap continue below to sign in.";
  }
  EXPECT_TRUE([[provider_ message] isEqualToString:expected]);
}

TEST_F(PostRestoreSignInProviderTest, defaultActionButtonText) {
  EnableFeatureVariationAlert();
  EXPECT_TRUE([[provider_ defaultActionButtonText]
      isEqualToString:@"Continue as Given"]);

  ClearUserName();
  EXPECT_TRUE(
      [[provider_ defaultActionButtonText] isEqualToString:@"Continue"]);
}

TEST_F(PostRestoreSignInProviderTest, cancelActionButtonText) {
  EnableFeatureVariationAlert();
  EXPECT_TRUE([[provider_ cancelActionButtonText] isEqualToString:@"Ignore"]);
}

TEST_F(PostRestoreSignInProviderTest, viewController) {
  EnableFeatureVariationFullscreen();
  EXPECT_TRUE(provider_.viewController != nil);
}

TEST_F(PostRestoreSignInProviderTest, recordsChoiceDismissed) {
  base::HistogramTester histogram_tester;

  // Test the Alert version.
  [provider_ standardPromoAlertCancelAction];
  histogram_tester.ExpectBucketCount(kIOSPostRestoreSigninChoiceHistogram,
                                     IOSPostRestoreSigninChoice::Dismiss, 1);

  // Test the Fullscreen version.
  [provider_ standardPromoDismissAction];
  histogram_tester.ExpectBucketCount(kIOSPostRestoreSigninChoiceHistogram,
                                     IOSPostRestoreSigninChoice::Dismiss, 2);
}

TEST_F(PostRestoreSignInProviderTest, recordsChoiceContinue) {
  base::HistogramTester histogram_tester;
  SetupMockHandler();
  OCMStub([mock_handler_ showSignin:[OCMArg any]]);
  [provider_ standardPromoAlertDefaultAction];
  histogram_tester.ExpectBucketCount(kIOSPostRestoreSigninChoiceHistogram,
                                     IOSPostRestoreSigninChoice::Continue, 1);
}

TEST_F(PostRestoreSignInProviderTest, recordsDisplayed) {
  base::HistogramTester histogram_tester;
  [provider_ promoWasDisplayed];
  histogram_tester.ExpectBucketCount(kIOSPostRestoreSigninDisplayedHistogram,
                                     true, 1);
}

TEST_F(PostRestoreSignInProviderTest, clearsPreRestoreIdentity) {
  // Test the Alert cancel.
  SetFakePreRestoreAccountInfo();
  EXPECT_TRUE(GetPreRestoreIdentity(local_state_.Get()).has_value());
  [provider_ standardPromoAlertCancelAction];
  EXPECT_FALSE(GetPreRestoreIdentity(local_state_.Get()).has_value());

  // Test the Fullscreen cancel.
  SetFakePreRestoreAccountInfo();
  EXPECT_TRUE(GetPreRestoreIdentity(local_state_.Get()).has_value());
  [provider_ standardPromoDismissAction];
  EXPECT_FALSE(GetPreRestoreIdentity(local_state_.Get()).has_value());

  // Test that it is cleared when the user chooses to sign in.
  SetFakePreRestoreAccountInfo();
  EXPECT_TRUE(GetPreRestoreIdentity(local_state_.Get()).has_value());
  SetupMockHandler();
  [provider_ standardPromoAlertDefaultAction];
  EXPECT_FALSE(GetPreRestoreIdentity(local_state_.Get()).has_value());
}
