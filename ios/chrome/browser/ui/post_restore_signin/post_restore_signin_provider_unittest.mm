// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/sync/test/sync_user_settings_mock.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/post_restore_signin/metrics.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"

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
    provider_ = [[PostRestoreSignInProvider alloc]
        initWithSyncUserSettings:&sync_user_settings_];
  }

  void SetFakePreRestoreAccountInfo() {
    AccountInfo accountInfo;
    accountInfo.email = std::string(kFakePreRestoreAccountEmail);
    accountInfo.given_name = std::string(kFakePreRestoreAccountGivenName);
    accountInfo.full_name = std::string(kFakePreRestoreAccountFullName);
    StorePreRestoreIdentity(local_state_.Get(), accountInfo,
                            /*history_sync_enabled=*/false);
  }

  void ClearUserName() {
    AccountInfo accountInfo;
    accountInfo.email = std::string(kFakePreRestoreAccountEmail);
    StorePreRestoreIdentity(local_state_.Get(), accountInfo,
                            /*history_sync_enabled=*/false);
    // Reinstantiate a provider so that it picks up the changes.
    provider_ = [[PostRestoreSignInProvider alloc]
        initWithSyncUserSettings:&sync_user_settings_];
  }

  void SetupMockHandler() {
    mock_handler_ = OCMProtocolMock(@protocol(PromosManagerCommands));
    provider_.handler = mock_handler_;
  }

  IOSChromeScopedTestingLocalState local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mock_handler_;
  PostRestoreSignInProvider* provider_;

 private:
  testing::NiceMock<syncer::SyncUserSettingsMock> sync_user_settings_;
};

// Tests `hasIdentifierAlert` method.
TEST_F(PostRestoreSignInProviderTest, hasIdentifierAlert) {
  EXPECT_EQ(provider_.config.identifier,
            promos_manager::Promo::PostRestoreSignInAlert);
}

// Tests the default action.
TEST_F(PostRestoreSignInProviderTest, standardPromoAlertDefaultAction) {
  SetupMockHandler();
  OCMExpect([mock_handler_ showSignin:[OCMArg any]]);
  [provider_ standardPromoAlertDefaultAction];
  [mock_handler_ verify];
}

// Test the title text.
TEST_F(PostRestoreSignInProviderTest, title) {
  EXPECT_TRUE([[provider_ title] isEqualToString:@"Chrome is Signed Out"]);
}

// Tests the alert message.
TEST_F(PostRestoreSignInProviderTest, message) {
  NSString* expected;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    expected = @"You were signed out of your account, person@example.org, as "
               @"part of your iPad reset. To sign back in, tap \"Continue\" "
               @"below.";
  } else {
    expected = @"You were signed out of your account, person@example.org, as "
               @"part of your iPhone reset. To sign back in, tap \"Continue\" "
               @"below.";
  }
  EXPECT_TRUE([[provider_ message] isEqualToString:expected]);
}

// Tests the text for the default action button.
TEST_F(PostRestoreSignInProviderTest, defaultActionButtonText) {
  EXPECT_TRUE([[provider_ defaultActionButtonText]
      isEqualToString:@"Continue as Given"]);

  ClearUserName();
  EXPECT_TRUE(
      [[provider_ defaultActionButtonText] isEqualToString:@"Continue"]);
}

// Tests the text for the cancel button.
TEST_F(PostRestoreSignInProviderTest, cancelActionButtonText) {
  EXPECT_TRUE([[provider_ cancelActionButtonText] isEqualToString:@"Ignore"]);
}

// Tests that a histogram is recorded when the user chooses to dismiss.
TEST_F(PostRestoreSignInProviderTest, recordsChoiceDismissed) {
  base::HistogramTester histogram_tester;

  [provider_ standardPromoAlertCancelAction];
  histogram_tester.ExpectBucketCount(kIOSPostRestoreSigninChoiceHistogram,
                                     IOSPostRestoreSigninChoice::Dismiss, 1);
}

// Tests that a histogram is recorded when the user chooses to continue.
TEST_F(PostRestoreSignInProviderTest, recordsChoiceContinue) {
  base::HistogramTester histogram_tester;
  SetupMockHandler();
  OCMStub([mock_handler_ showSignin:[OCMArg any]]);
  [provider_ standardPromoAlertDefaultAction];
  histogram_tester.ExpectBucketCount(kIOSPostRestoreSigninChoiceHistogram,
                                     IOSPostRestoreSigninChoice::Continue, 1);
}

// Tests that a histogram is recorded when the promo is displayed.
TEST_F(PostRestoreSignInProviderTest, recordsDisplayed) {
  base::HistogramTester histogram_tester;
  [provider_ promoWasDisplayed];
  histogram_tester.ExpectBucketCount(kIOSPostRestoreSigninDisplayedHistogram,
                                     true, 1);
}

// Tests that the provider clears the pre-restore identity.
TEST_F(PostRestoreSignInProviderTest, clearsPreRestoreIdentity) {
  // Test cancel.
  SetFakePreRestoreAccountInfo();
  EXPECT_TRUE(GetPreRestoreIdentity(local_state_.Get()).has_value());
  [provider_ standardPromoAlertCancelAction];
  EXPECT_FALSE(GetPreRestoreIdentity(local_state_.Get()).has_value());

  // Test that it is cleared when the user chooses to sign in.
  SetFakePreRestoreAccountInfo();
  EXPECT_TRUE(GetPreRestoreIdentity(local_state_.Get()).has_value());
  SetupMockHandler();
  [provider_ standardPromoAlertDefaultAction];
  EXPECT_FALSE(GetPreRestoreIdentity(local_state_.Get()).has_value());
}
