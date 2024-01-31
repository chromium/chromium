// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync/test/sync_user_settings_mock.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/post_restore_signin/metrics.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

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
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                                       SyncServiceFactory::GetDefaultFactory());
    browser_state_ = test_cbs_builder.Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    provider_ =
        [[PostRestoreSignInProvider alloc] initForBrowser:browser_.get()];
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
    provider_ =
        [[PostRestoreSignInProvider alloc] initForBrowser:browser_.get()];
  }

  void SetupMockHandler() {
    mock_handler_ = OCMProtocolMock(@protocol(PromosManagerCommands));
    provider_.handler = mock_handler_;
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mock_handler_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  PostRestoreSignInProvider* provider_;
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
  EXPECT_TRUE([[provider_ title]
      isEqualToString:l10n_util::GetNSString(
                          IDS_IOS_POST_RESTORE_SIGN_IN_ALERT_PROMO_TITLE)]);
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
