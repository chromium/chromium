// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_impl.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class GeminiServiceImplTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    scoped_feature_list_.InitWithFeatures({kGeminiCopresence}, {});

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
    pref_service_->registry()->RegisterIntegerPref(prefs::kGenAiEnabledByPolicy,
                                                   0);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kIOSBWGPromoImpressionCount, 0);
    pref_service_->registry()->RegisterBooleanPref(prefs::kIOSBwgConsent,
                                                   false);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kAIHubEligibilityTriggered, false);

    gemini_service_ = std::make_unique<GeminiServiceImpl>(
        profile_.get(), auth_service_, identity_test_env_.identity_manager(),
        pref_service_.get(), optimization_guide_service_);
  }

  void TearDown() override {
    // Shutdown the service to ensure it unregisters itself as an observer
    // from IdentityManager before IdentityManager is destroyed.
    if (gemini_service_) {
      gemini_service_->Shutdown();
    }
    PlatformTest::TearDown();
  }

  // Signs in an unmanaged account and sets their model execution capability.
  void SignInUnmanagedAccountWithCapability(bool capability) {
    std::string email = "unmanaged@gmail.com";
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    SetCapabilityForAccount(account_info, capability);
  }

  // Signs in a managed account and sets their model execution capability.
  void SignInManagedAccountWithCapability(bool capability) {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeManagedIdentity];
    fake_system_identity_manager()->AddIdentity(identity);
    ChromeAccountManagerService* account_manager =
        ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
    auth_service_->SignIn(account_manager->GetDefaultIdentity(),
                          signin_metrics::AccessPoint::kStartPage);

    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        base::SysNSStringToUTF8(identity.userEmail),
        signin::ConsentLevel::kSignin);
    SetCapabilityForAccount(account_info, capability);
  }

  // Sets the workspace eligibility.
  void SetWorkspaceEligibility(std::optional<bool> is_disabled) {
    gemini_service_->is_disabled_by_gemini_policy_ = is_disabled;
  }

  void SetCapabilityForAccount(AccountInfo info, bool capability) {
    AccountCapabilitiesTestMutator mutator(&info.capabilities);
    mutator.set_can_use_model_execution_features(capability);
    mutator.set_can_use_gemini_in_chrome(capability);
    identity_test_env_.UpdateAccountInfoForAccount(info);
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  // Environment objects are declared first, so they are destroyed last.
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  web::WebTaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // Profile and services that depend on the environment are declared next.
  // Note: `pref_service_` must be declared before `gemini_service_` so that
  // it is destroyed after `gemini_service_`, preventing a dangling pointer.
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<GeminiServiceImpl> gemini_service_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_;

  base::HistogramTester histogram_tester_;
};

// Tests that a user is considered eligible if they are signed in and their
// account has the `can_use_model_execution_features` capability.
TEST_F(GeminiServiceImplTest, IsProfileEligibleForGemini_WhenUserIsEligible) {
  SignInUnmanagedAccountWithCapability(true);
  SetWorkspaceEligibility(/*is_disabled=*/false);

  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());
  histogram_tester_.ExpectTotalCount(kGeminiIneligibilityReasonHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/true,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if they are signed in but their account
// capability is explicitly false.
TEST_F(GeminiServiceImplTest,
       IsProfileEligibleForGemini_IneligibleByCapability) {
  SignInUnmanagedAccountWithCapability(false);
  SetWorkspaceEligibility(/*is_disabled=*/false);
  pref_service_->SetInteger(prefs::kGeminiEnabledByPolicy,
                            static_cast<int>(gemini::SettingsPolicy::kAllowed));

  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());
  histogram_tester_.ExpectUniqueSample(
      kGeminiIneligibilityReasonHistogram,
      IOSGeminiIneligibilityReason::kInsufficientAccountCapability, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if both of the Gemini policies are disabled.
TEST_F(GeminiServiceImplTest,
       IsProfileEligibleForGemini_IneligibleByBothPolicies) {
  SignInUnmanagedAccountWithCapability(true);
  SetWorkspaceEligibility(/*is_disabled=*/false);
  pref_service_->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));
  pref_service_->SetInteger(
      prefs::kGenAiEnabledByPolicy,
      static_cast<int>(gemini::GenAiDefaultSettingsPolicy::kNotAllowed));

  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());
  histogram_tester_.ExpectUniqueSample(
      kGeminiIneligibilityReasonHistogram,
      IOSGeminiIneligibilityReason::kChromeEnterpriseDisabled, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if the Gemini policy is disabled.
TEST_F(GeminiServiceImplTest,
       IsProfileEligibleForGemini_IneligibleByGeminiPolicy) {
  SignInUnmanagedAccountWithCapability(true);
  SetWorkspaceEligibility(/*is_disabled=*/false);
  pref_service_->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());
  histogram_tester_.ExpectUniqueSample(
      kGeminiIneligibilityReasonHistogram,
      IOSGeminiIneligibilityReason::kChromeEnterpriseDisabled, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if the GenAI policy is disabled.
TEST_F(GeminiServiceImplTest,
       IsProfileEligibleForGemini_IneligibleByGenAIPolicy) {
  SignInUnmanagedAccountWithCapability(true);
  SetWorkspaceEligibility(/*is_disabled=*/false);
  pref_service_->SetInteger(
      prefs::kGenAiEnabledByPolicy,
      static_cast<int>(gemini::GenAiDefaultSettingsPolicy::kNotAllowed));

  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());
  histogram_tester_.ExpectUniqueSample(
      kGeminiIneligibilityReasonHistogram,
      IOSGeminiIneligibilityReason::kChromeEnterpriseDisabled, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that a user is eligible if both of the Gemini policies are enabled.
TEST_F(GeminiServiceImplTest, IsProfileEligibleForGemini_EligibleByPolicy) {
  SignInUnmanagedAccountWithCapability(true);
  SetWorkspaceEligibility(/*is_disabled=*/false);
  pref_service_->SetInteger(prefs::kGeminiEnabledByPolicy,
                            static_cast<int>(gemini::SettingsPolicy::kAllowed));
  pref_service_->SetInteger(
      prefs::kGenAiEnabledByPolicy,
      static_cast<int>(
          gemini::GenAiDefaultSettingsPolicy::kAllowedWithoutImprovingModels));

  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());
  histogram_tester_.ExpectTotalCount(kGeminiIneligibilityReasonHistogram, 0);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/true,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if they are not signed in to a primary
// account.
TEST_F(GeminiServiceImplTest,
       IsProfileEligibleForGemini_IneligibleWhenSignedOut) {
  // The default state is signed out.
  EXPECT_FALSE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));

  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());
  histogram_tester_.ExpectBucketCount(
      kGeminiIneligibilityReasonHistogram,
      IOSGeminiIneligibilityReason::kAccountUnauthenticated, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if they are signed in to a primary
// account but their account capabilities are unknown.
TEST_F(GeminiServiceImplTest,
       IsProfileEligibleForGemini_IneligibleWhenCapabilityIsUnknown) {
  // Sign in without setting any capabilities.
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());
  histogram_tester_.ExpectUniqueSample(
      kGeminiIneligibilityReasonHistogram,
      IOSGeminiIneligibilityReason::kInsufficientAccountCapability, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that a user is ineligible if the Gemini workspace is restricted.
TEST_F(GeminiServiceImplTest,
       IsProfileEligibleForGemini_IneligibleWithRestrictedWorkspace) {
  // Sign in with workspace set to non eligible
  SignInUnmanagedAccountWithCapability(true);
  SetWorkspaceEligibility(/*is_disabled=*/true);

  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());
  histogram_tester_.ExpectUniqueSample(
      kGeminiIneligibilityReasonHistogram,
      IOSGeminiIneligibilityReason::kWorkspaceRestricted, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram,
                                       /*sample=*/false,
                                       /*expected_count=*/1);
}

// Tests that for a managed account, the user is ineligible until the
// workspace response arrives, but we don't log the workspace restriction.
TEST_F(GeminiServiceImplTest,
       IsProfileEligibleForGemini_ManagedAccountPending) {
  SignInManagedAccountWithCapability(true);
  SetWorkspaceEligibility(/*is_disabled=*/std::nullopt);

  // Should be ineligible because it's managed and response is pending.
  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());

  // Verify that the primary identity is now flagged as managed.
  EXPECT_TRUE(
      auth_service_->HasPrimaryIdentityManaged(signin::ConsentLevel::kSignin));

  // Workspace restriction should NOT be logged.
  histogram_tester_.ExpectBucketCount(
      kGeminiIneligibilityReasonHistogram,
      IOSGeminiIneligibilityReason::kWorkspaceRestricted, 0);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram, false, 1);
}

// Tests that for a managed account, once the workspace response arrives and
// confirms it is restricted, we do log the workspace restriction.
TEST_F(GeminiServiceImplTest,
       IsProfileEligibleForGemini_ManagedAccountRestricted) {
  SignInManagedAccountWithCapability(true);
  SetWorkspaceEligibility(/*is_disabled=*/true);

  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());

  // Workspace restriction SHOULD be logged.
  histogram_tester_.ExpectBucketCount(
      kGeminiIneligibilityReasonHistogram,
      IOSGeminiIneligibilityReason::kWorkspaceRestricted, 1);
  histogram_tester_.ExpectUniqueSample(kEligibilityHistogram, false, 1);
}

// Tests that LogUserConsentState correctly logs the consent flow state when
// the profile is eligible.
TEST_F(GeminiServiceImplTest, LogUserConsentState_LogsWhenEligible) {
  SignInUnmanagedAccountWithCapability(true);
  SetWorkspaceEligibility(/*is_disabled=*/false);

  // Explicitly set to no consent.
  pref_service_.get()->SetBoolean(prefs::kIOSBwgConsent, false);
  // Triggering IsProfileEligibleForGemini should log the state.
  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());

  histogram_tester_.ExpectUniqueSample(kGeminiFREStateHistogram,
                                       gemini::FREState::kPending, 1);

  // Changing state and checking eligibility again should log the new state.
  pref_service_.get()->SetInteger(prefs::kIOSBWGPromoImpressionCount, 1);
  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());

  histogram_tester_.ExpectBucketCount(kGeminiFREStateHistogram,
                                      gemini::FREState::kStarted, 1);

  pref_service_.get()->SetBoolean(prefs::kIOSBwgConsent, true);
  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());

  histogram_tester_.ExpectBucketCount(kGeminiFREStateHistogram,
                                      gemini::FREState::kCompleted, 1);
  histogram_tester_.ExpectTotalCount(kGeminiFREStateHistogram, 3);
}
