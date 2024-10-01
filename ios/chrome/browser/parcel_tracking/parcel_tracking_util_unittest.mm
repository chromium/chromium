// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"

#import "base/memory/raw_ptr.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/service/variations_service_client.h"
#import "components/variations/synthetic_trial_registry.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_variations_service.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/test/test_network_connection_tracker.h"
#import "testing/platform_test.h"

class ParcelTrackingUtilTest : public PlatformTest {
 protected:
  void SetUp() override {
    profile_ = BuildProfile();
    AuthenticationServiceFactory::CreateAndInitializeForProfile(
        profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
    auth_service_ = static_cast<AuthenticationService*>(
        AuthenticationServiceFactory::GetInstance()->GetForProfile(
            profile_.get()));
    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    shopping_service_->SetIsParcelTrackingEligible(true);
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
  }

  std::unique_ptr<TestProfileIOS> BuildProfile() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    return std::move(builder).Build();
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
    profile_->GetPrefs()->SetBoolean(
        prefs::kIosParcelTrackingOptInPromptDisplayLimitMet, displayed);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<AuthenticationService> auth_service_ = nullptr;
  FakeSystemIdentity* fake_identity_ = nullptr;
};

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns true when the user
// is eligible.
TEST_F(ParcelTrackingUtilTest, UserIsEligibleForPrompt) {
  SignIn();
  SetPromptDisplayedStatus(false);
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");
  EXPECT_TRUE(IsUserEligibleParcelTrackingOptInPrompt(profile_->GetPrefs(),
                                                      shopping_service_.get()));
}

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns false when the
// user is not signed in.
TEST_F(ParcelTrackingUtilTest, NotSignedIn) {
  SignOut();
  SetPromptDisplayedStatus(false);
  shopping_service_->SetIsParcelTrackingEligible(false);
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(IsUserEligibleParcelTrackingOptInPrompt(
      profile_->GetPrefs(), shopping_service_.get()));
}

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns false when the
// user has seen the prompt.
TEST_F(ParcelTrackingUtilTest, UserHasSeenPrompt) {
  SignIn();
  SetPromptDisplayedStatus(true);
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(IsUserEligibleParcelTrackingOptInPrompt(
      profile_->GetPrefs(), shopping_service_.get()));
}

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns false when the
// permanent country is not set to US.
TEST_F(ParcelTrackingUtilTest, CountryNotUS) {
  SignIn();
  SetPromptDisplayedStatus(true);
  EXPECT_FALSE(IsUserEligibleParcelTrackingOptInPrompt(
      profile_->GetPrefs(), shopping_service_.get()));
}
