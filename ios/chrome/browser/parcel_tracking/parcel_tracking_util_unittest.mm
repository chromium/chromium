// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"

#import "base/test/scoped_feature_list.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class ParcelTrackingUtilTest : public PlatformTest {
 protected:
  void SetUp() override {
    browser_state_ = BuildChromeBrowserState();
    AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
        browser_state_.get(),
        std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForBrowserState(
            browser_state_.get()));
    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    shopping_service_->SetIsParcelTrackingEligible(true);
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
  }

  std::unique_ptr<TestChromeBrowserState> BuildChromeBrowserState() {
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    return builder.Build();
  }

  void SignIn() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(fake_identity_);
    auth_service_->SignIn(fake_identity_,
                          signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
  }

  void SignOut() {
    auth_service_->SignOut(signin_metrics::ProfileSignout::kTest,
                           /*force_clear_browsing_data=*/false, nil);
  }

  void SetPromptDisplayedStatus(bool displayed) {
    browser_state_->GetPrefs()->SetBoolean(
        prefs::kIosParcelTrackingOptInPromptDisplayed, displayed);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  AuthenticationService* auth_service_ = nullptr;
  FakeSystemIdentity* fake_identity_ = nullptr;
};

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns true when the user
// is eligible.
TEST_F(ParcelTrackingUtilTest, UserIsEligibleForPrompt) {
  scoped_feature_list_.InitAndEnableFeature(kIOSParcelTracking);
  SignIn();
  SetPromptDisplayedStatus(false);
  EXPECT_TRUE(IsUserEligibleParcelTrackingOptInPrompt(
      browser_state_->GetPrefs(), shopping_service_.get()));
}

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns false when the
// user is not signed in.
TEST_F(ParcelTrackingUtilTest, NotSignedIn) {
  scoped_feature_list_.InitAndEnableFeature(kIOSParcelTracking);
  SignOut();
  SetPromptDisplayedStatus(false);
  shopping_service_->SetIsParcelTrackingEligible(false);
  EXPECT_FALSE(IsUserEligibleParcelTrackingOptInPrompt(
      browser_state_->GetPrefs(), shopping_service_.get()));
}

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns false when the
// feature is disabled.
TEST_F(ParcelTrackingUtilTest, FeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kIOSParcelTracking);
  SignIn();
  SetPromptDisplayedStatus(false);
  EXPECT_FALSE(IsUserEligibleParcelTrackingOptInPrompt(
      browser_state_->GetPrefs(), shopping_service_.get()));
}

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns false when the
// user has seen the prompt.
TEST_F(ParcelTrackingUtilTest, UserHasSeenPrompt) {
  scoped_feature_list_.InitAndEnableFeature(kIOSParcelTracking);
  SignIn();
  SetPromptDisplayedStatus(true);
  EXPECT_FALSE(IsUserEligibleParcelTrackingOptInPrompt(
      browser_state_->GetPrefs(), shopping_service_.get()));
}
