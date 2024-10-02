// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"

#import "base/memory/raw_ptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/sync/test/sync_user_settings_mock.h"
#import "ios/chrome/browser/promos_manager/model/constants.h"
#import "ios/chrome/browser/promos_manager/model/promo_config.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/post_restore_signin/metrics.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest_mac.h"
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
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    pref_service_ = profile_.get()->GetPrefs();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());

    SetFakePreRestoreAccountInfo();
    provider_ =
        [[PostRestoreSignInProvider alloc] initForBrowser:browser_.get()];
  }

  void SetFakePreRestoreAccountInfo() {
    AccountInfo accountInfo;
    accountInfo.email = std::string(kFakePreRestoreAccountEmail);
    accountInfo.given_name = std::string(kFakePreRestoreAccountGivenName);
    accountInfo.full_name = std::string(kFakePreRestoreAccountFullName);
    StorePreRestoreIdentity(pref_service_, accountInfo,
                            /*history_sync_enabled=*/false);
  }

  void ClearUserName() {
    AccountInfo accountInfo;
    accountInfo.email = std::string(kFakePreRestoreAccountEmail);
    StorePreRestoreIdentity(pref_service_, accountInfo,
                            /*history_sync_enabled=*/false);
    // Reinstantiate a provider so that it picks up the changes.
    provider_ =
        [[PostRestoreSignInProvider alloc] initForBrowser:browser_.get()];
  }

  void SetupMockHandler() {
    mock_handler_ = OCMProtocolMock(@protocol(PromosManagerCommands));
    provider_.handler = mock_handler_;
  }

  // Signs in a fake identity.
  void SignIn() {
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity);
    auth_service_->SignIn(fake_identity,
                          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

 protected:
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<AuthenticationService> auth_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  id mock_handler_;
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
  EXPECT_NSEQ(
      [provider_ title],
      l10n_util::GetNSString(IDS_IOS_POST_RESTORE_SIGN_IN_ALERT_PROMO_TITLE));
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
  EXPECT_NSEQ([provider_ message], expected);
}

// Tests the text for the default action button.
TEST_F(PostRestoreSignInProviderTest, defaultActionButtonText) {
  EXPECT_NSEQ([provider_ defaultActionButtonText], @"Continue as Given");

  ClearUserName();
  EXPECT_NSEQ([provider_ defaultActionButtonText], @"Continue");
}

// Tests the text for the cancel button.
TEST_F(PostRestoreSignInProviderTest, cancelActionButtonText) {
  EXPECT_NSEQ([provider_ cancelActionButtonText], @"Ignore");
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
  EXPECT_TRUE(GetPreRestoreIdentity(pref_service_).has_value());
  [provider_ standardPromoAlertCancelAction];
  EXPECT_FALSE(GetPreRestoreIdentity(pref_service_).has_value());

  // Test that it is cleared when the user chooses to sign in.
  SetFakePreRestoreAccountInfo();
  EXPECT_TRUE(GetPreRestoreIdentity(pref_service_).has_value());
  SetupMockHandler();
  [provider_ standardPromoAlertDefaultAction];
  EXPECT_FALSE(GetPreRestoreIdentity(pref_service_).has_value());
}

// Tests that when tapping "continue" when signed-in does not attempt to
// re-signin.
TEST_F(PostRestoreSignInProviderTest, AlreadySignedIn) {
  __block bool didCallShowSignIn = false;
  SetupMockHandler();
  OCMStub([mock_handler_ showSignin:[OCMArg any]]).andDo(^(NSInvocation* inv) {
    didCallShowSignIn = true;
  });

  SignIn();
  [provider_ standardPromoAlertDefaultAction];

  EXPECT_FALSE(didCallShowSignIn);
}
