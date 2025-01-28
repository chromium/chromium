// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"

#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/commerce/core/feature_utils.h"
#import "components/commerce/core/mock_account_checker.h"
#import "components/commerce/core/mock_shopping_service.h"
#import "components/variations/service/variations_service.h"
#import "components/variations/service/variations_service_client.h"
#import "components/variations/synthetic_trial_registry.h"
#import "ios/chrome/browser/parcel_tracking/features.h"
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
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    profile_ = std::move(builder).Build();
    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());
    shopping_service_ = std::make_unique<commerce::MockShoppingService>();
    account_checker_ = std::make_unique<commerce::MockAccountChecker>();
    shopping_service_->SetAccountChecker(account_checker_.get());
    account_checker_->SetSignedIn(false);
    fake_identity_ = [FakeSystemIdentity fakeIdentity1];
  }

  void SetPromptDisplayedStatus(bool displayed) {
    profile_->GetPrefs()->SetBoolean(
        prefs::kIosParcelTrackingOptInPromptDisplayLimitMet, displayed);
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<commerce::MockShoppingService> shopping_service_;
  std::unique_ptr<commerce::MockAccountChecker> account_checker_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<AuthenticationService> auth_service_ = nullptr;
  FakeSystemIdentity* fake_identity_ = nullptr;
};

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns true when the user
// is eligible.
TEST_F(ParcelTrackingUtilTest, UserIsEligibleForPrompt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({commerce::kParcelTracking},
                                       {kIOSDisableParcelTracking});
  account_checker_->SetSignedIn(true);
  ASSERT_TRUE(commerce::IsParcelTrackingEligible(account_checker_.get()));
  SetPromptDisplayedStatus(false);
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");
  EXPECT_TRUE(IsUserEligibleParcelTrackingOptInPrompt(profile_->GetPrefs(),
                                                      shopping_service_.get()));
}

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns false when the
// user is not signed in.
TEST_F(ParcelTrackingUtilTest, NotSignedIn) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({commerce::kParcelTracking},
                                       {kIOSDisableParcelTracking});
  account_checker_->SetSignedIn(false);
  ASSERT_FALSE(commerce::IsParcelTrackingEligible(account_checker_.get()));
  SetPromptDisplayedStatus(false);
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(IsUserEligibleParcelTrackingOptInPrompt(
      profile_->GetPrefs(), shopping_service_.get()));
}

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns false when the
// feature is disabled (i.e. kIOSDisableParcelTracking is enabled). This is now
// the default behavior.
TEST_F(ParcelTrackingUtilTest, FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(commerce::kParcelTracking);
  account_checker_->SetSignedIn(true);
  ASSERT_TRUE(commerce::IsParcelTrackingEligible(account_checker_.get()));
  SetPromptDisplayedStatus(false);
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(IsUserEligibleParcelTrackingOptInPrompt(
      profile_->GetPrefs(), shopping_service_.get()));
}

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns false when the
// user has seen the prompt.
TEST_F(ParcelTrackingUtilTest, UserHasSeenPrompt) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({commerce::kParcelTracking},
                                       {kIOSDisableParcelTracking});
  account_checker_->SetSignedIn(true);
  ASSERT_TRUE(commerce::IsParcelTrackingEligible(account_checker_.get()));
  SetPromptDisplayedStatus(true);
  IOSChromeScopedTestingVariationsService scoped_variations_service;
  scoped_variations_service.Get()->OverrideStoredPermanentCountry("us");
  EXPECT_FALSE(IsUserEligibleParcelTrackingOptInPrompt(
      profile_->GetPrefs(), shopping_service_.get()));
}

// Tests that IsUserEligibleParcelTrackingOptInPrompt returns false when the
// permanent country is not set to US.
TEST_F(ParcelTrackingUtilTest, CountryNotUS) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({commerce::kParcelTracking},
                                       {kIOSDisableParcelTracking});
  account_checker_->SetSignedIn(true);
  ASSERT_TRUE(commerce::IsParcelTrackingEligible(account_checker_.get()));
  SetPromptDisplayedStatus(false);
  EXPECT_FALSE(IsUserEligibleParcelTrackingOptInPrompt(
      profile_->GetPrefs(), shopping_service_.get()));
}
