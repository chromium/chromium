// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_service.h"

#import <memory>

#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class BwgServiceTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegate(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_.get());
    optimization_guide_service_ =
        OptimizationGuideServiceFactory::GetForProfile(profile_.get());

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kGeminiEnabledByPolicy, 0);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kAIHubEligibilityTriggered, false);

    bwg_service_ = std::make_unique<BwgService>(
        profile_.get(), auth_service_, identity_test_env_.identity_manager(),
        pref_service_.get(), optimization_guide_service_);
  }

  void TearDown() override {
    // Shutdown the service to ensure it unregisters itself as an observer
    // from IdentityManager before IdentityManager is destroyed.
    if (bwg_service_) {
      bwg_service_->Shutdown();
    }
    PlatformTest::TearDown();
  }

  // Signs in a user and sets their model execution capability.
  void SignInAndSetCapability(bool capability) {
    const std::string email = "test@example.com";

    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(capability);

    identity_test_env_.UpdateAccountInfoForAccount(account_info);
  }

  // Environment objects are declared first, so they are destroyed last.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;

  // Profile and services that depend on the environment are declared next.
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<BwgService> bwg_service_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_;

  base::HistogramTester histogram_tester_;
};

// Tests that a user is considered eligible if they are signed in and their
// account has the `can_use_model_execution_features` capability.
TEST_F(BwgServiceTest, IsProfileEligibleForBWG_WhenUserIsEligible) {
  SignInAndSetCapability(true);

  EXPECT_TRUE(bwg_service_->IsProfileEligibleForBwg());
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/true,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if they are signed in but their account
// capability is explicitly false.
TEST_F(BwgServiceTest, IsProfileEligibleForBWG_IneligibleByCapability) {
  SignInAndSetCapability(false);
  pref_service_->SetInteger(prefs::kGeminiEnabledByPolicy, 0);

  EXPECT_FALSE(bwg_service_->IsProfileEligibleForBwg());
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if the Gemini policy is disabled.
TEST_F(BwgServiceTest, IsProfileEligibleForBWG_IneligibleByPolicy) {
  SignInAndSetCapability(true);
  pref_service_->SetInteger(prefs::kGeminiEnabledByPolicy, 1);

  EXPECT_FALSE(bwg_service_->IsProfileEligibleForBwg());
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that a user is eligible if the Gemini policy is enabled.
TEST_F(BwgServiceTest, IsProfileEligibleForBWG_EligibleByPolicy) {
  SignInAndSetCapability(true);
  pref_service_->SetInteger(prefs::kGeminiEnabledByPolicy, 0);

  EXPECT_TRUE(bwg_service_->IsProfileEligibleForBwg());
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/true,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if they are not signed in to a primary
// account.
TEST_F(BwgServiceTest, IsProfileEligibleForBWG_IneligibleWhenSignedOut) {
  // The default state is signed out.
  EXPECT_FALSE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  EXPECT_FALSE(bwg_service_->IsProfileEligibleForBwg());
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if they are signed in to a primary
// account but their account capabilities are unknown.
TEST_F(BwgServiceTest,
       IsProfileEligibleForBWG_IneligibleWhenCapabilityIsUnknown) {
  // Sign in without setting any capabilities.
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  EXPECT_FALSE(bwg_service_->IsProfileEligibleForBwg());
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that BWG is available for a web state when the user is eligible and
// the web state is not off the record.
TEST_F(BwgServiceTest, IsBwgAvailableForWebState_WhenUserIsEligible) {
  SignInAndSetCapability(true);
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());
  web_state->SetCurrentURL(GURL("https://www.google.com"));
  web_state->SetContentsMimeType("text/html");

  EXPECT_TRUE(bwg_service_->IsBwgAvailableForWebState(web_state.get()));
}

// Tests that BWG is not available for a web state when the user is not
// eligible.
TEST_F(BwgServiceTest, IsBwgAvailableForWebState_WhenUserIsNotEligible) {
  SignInAndSetCapability(false);
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_.get());

  EXPECT_FALSE(bwg_service_->IsBwgAvailableForWebState(web_state.get()));
}

// Tests that BWG is not available for a web state when the web state is off
// the record.
TEST_F(BwgServiceTest, IsBwgAvailableForWebState_WhenWebStateIsOffTheRecord) {
  SignInAndSetCapability(true);
  auto web_state = std::make_unique<web::FakeWebState>();
  web_state->SetBrowserState(profile_->GetOffTheRecordProfile());

  EXPECT_FALSE(bwg_service_->IsBwgAvailableForWebState(web_state.get()));
}

// Tests that BWG is not available for a web state when the web state is null.
TEST_F(BwgServiceTest, IsBwgAvailableForWebState_WhenWebStateIsNull) {
  SignInAndSetCapability(true);

  EXPECT_FALSE(bwg_service_->IsBwgAvailableForWebState(nullptr));
}
