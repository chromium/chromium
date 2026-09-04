// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_impl.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_test_utils.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/test/test_sync_service.h"
#import "google_apis/gaia/core_account_id.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/account_profile_mapper.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/identity_test_environment_browser_state_adaptor.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class GeminiServiceImplTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(
        OptimizationGuideServiceFactory::GetInstance(),
        OptimizationGuideServiceFactory::GetDefaultFactory());
    profile_ = profile_manager_.AddProfileWithBuilder(std::move(builder));

    auth_service_ = AuthenticationServiceFactory::GetForProfile(profile_);
    identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
    optimization_guide_service_ =
        OptimizationGuideServiceFactory::GetForProfile(profile_);

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
        profile_, auth_service_, identity_manager_, pref_service_.get(),
        optimization_guide_service_);
  }

  void TearDown() override {
    // Shutdown the service to ensure it unregisters itself as an observer
    // from IdentityManager before IdentityManager is destroyed.
    if (gemini_service_) {
      gemini_service_->Shutdown();
    }
    gemini_service_.reset();
    pref_service_.reset();
    auth_service_ = nullptr;
    identity_manager_ = nullptr;
    optimization_guide_service_ = nullptr;
    profile_ = nullptr;
    PlatformTest::TearDown();
  }

  // Signs in an unmanaged account and sets their model execution capability.
  void SignInUnmanagedAccountWithCapability(signin::Tribool capability) {
    std::string email = "unmanaged@gmail.com";
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeIdentity1];
    fake_system_identity_manager()->AddIdentity(identity);
    auth_service_->SignIn(identity, signin_metrics::AccessPoint::kStartPage);
    if (capability != signin::Tribool::kUnknown) {
      SetCapabilityForAccount(
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          signin::TriboolToBoolOrDie(capability));
    }
  }

  // Signs in a managed account and sets their model execution capability.
  void SignInManagedAccountWithCapability(signin::Tribool capability) {
    FakeSystemIdentity* identity = [FakeSystemIdentity fakeManagedIdentity];
    fake_system_identity_manager()->AddIdentity(identity);
    GetApplicationContext()
        ->GetAccountProfileMapper()
        ->MakePersonalProfileManagedWithGaiaID(identity.gaiaId);
    auth_service_->SignIn(identity, signin_metrics::AccessPoint::kStartPage);
    if (capability != signin::Tribool::kUnknown) {
      SetCapabilityForAccount(
          identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          signin::TriboolToBoolOrDie(capability));
    }
  }

  // Sets the workspace eligibility.
  void SetWorkspaceEligibility(std::optional<bool> is_disabled) {
    gemini_service_->is_disabled_by_gemini_policy_ = is_disabled;
  }

  void SetCapabilityForAccount(const CoreAccountId& account_id,
                               bool capability) {
    AccountInfo account_info =
        identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(capability);
    mutator.set_can_use_gemini_in_chrome(capability);
    signin::UpdateAccountInfoForAccount(identity_manager_, account_info);
  }

  FakeSystemIdentityManager* fake_system_identity_manager() {
    return FakeSystemIdentityManager::FromSystemIdentityManager(
        GetApplicationContext()->GetSystemIdentityManager());
  }

  // Environment objects are declared first, so they are destroyed last.
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_ = nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;

  // Profile and services that depend on the environment are declared next.
  // Note: `pref_service_` must be declared before `gemini_service_` so that
  // it is destroyed after `gemini_service_`, preventing a dangling pointer.
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<GeminiServiceImpl> gemini_service_;
  raw_ptr<AuthenticationService> auth_service_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<OptimizationGuideService> optimization_guide_service_;

  base::HistogramTester histogram_tester_;
};

// Tests that a user is considered eligible if they are signed in and their
// account has the `can_use_model_execution_features` capability.
TEST_F(GeminiServiceImplTest, IsProfileEligibleForGemini_WhenUserIsEligible) {
  SignInUnmanagedAccountWithCapability(signin::Tribool::kTrue);
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
  SignInUnmanagedAccountWithCapability(signin::Tribool::kFalse);
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
  SignInUnmanagedAccountWithCapability(signin::Tribool::kTrue);
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
  SignInUnmanagedAccountWithCapability(signin::Tribool::kTrue);
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
  SignInUnmanagedAccountWithCapability(signin::Tribool::kTrue);
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
  SignInUnmanagedAccountWithCapability(signin::Tribool::kTrue);
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
  EXPECT_FALSE(
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin));

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
  SignInUnmanagedAccountWithCapability(signin::Tribool::kUnknown);

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
  SignInUnmanagedAccountWithCapability(signin::Tribool::kTrue);
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
  SignInManagedAccountWithCapability(signin::Tribool::kTrue);
  SetWorkspaceEligibility(/*is_disabled=*/std::nullopt);

  // Should be ineligible because it's managed and response is pending.
  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());

  // Verify that the primary identity is now flagged as managed.
  EXPECT_TRUE(auth_service_->HasPrimaryIdentityManaged());

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
  SignInManagedAccountWithCapability(signin::Tribool::kTrue);
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
  SignInUnmanagedAccountWithCapability(signin::Tribool::kTrue);
  SetWorkspaceEligibility(/*is_disabled=*/false);

  // Explicitly set to no consent.
  pref_service_.get()->SetBoolean(prefs::kIOSBwgConsent, false);
  // Triggering IsProfileEligibleForGemini should log the state.
  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());

  histogram_tester_.ExpectUniqueSample(kGeminiFirstRunStateHistogram,
                                       gemini::FirstRunState::kPending, 1);

  // Changing state and checking eligibility again should log the new state.
  pref_service_.get()->SetInteger(prefs::kIOSBWGPromoImpressionCount, 1);
  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());

  histogram_tester_.ExpectBucketCount(kGeminiFirstRunStateHistogram,
                                      gemini::FirstRunState::kStarted, 1);

  pref_service_.get()->SetBoolean(prefs::kIOSBwgConsent, true);
  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());

  histogram_tester_.ExpectBucketCount(kGeminiFirstRunStateHistogram,
                                      gemini::FirstRunState::kCompleted, 1);
  histogram_tester_.ExpectTotalCount(kGeminiFirstRunStateHistogram, 3);
}

// Tests that when the user is signed out, the workspace policy check is not
// pending, and the user is ineligible.
TEST_F(GeminiServiceImplTest, CheckGeminiEnterpriseEligibility_SignedOut) {
  // Ensure we are signed out.
  ASSERT_FALSE(
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Call the check (or trigger it).
  gemini_service_->CheckGeminiEnterpriseEligibilityIfNeeded();

  // The check should not be pending.
  EXPECT_FALSE(gemini_service_->IsWorkspacePolicyCheckPending());
  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());
}

// Tests that when the user signs in, the workspace policy check becomes pending
// (std::nullopt) synchronously.
TEST_F(GeminiServiceImplTest, CheckGeminiEnterpriseEligibility_SignInPending) {
  // Start signed out.
  gemini_service_->CheckGeminiEnterpriseEligibilityIfNeeded();
  ASSERT_FALSE(gemini_service_->IsWorkspacePolicyCheckPending());

  // Sign in. This triggers OnPrimaryAccountChanged, which calls
  // CheckGeminiEnterpriseEligibility.
  SignInUnmanagedAccountWithCapability(signin::Tribool::kTrue);

  // It should now be pending (std::nullopt) synchronously.
  EXPECT_TRUE(gemini_service_->IsWorkspacePolicyCheckPending());
}

// Tests that updating the primary account's extended info (e.g. capabilities)
// correctly updates the service's eligibility.
TEST_F(GeminiServiceImplTest, OnExtendedAccountInfoUpdated_UpdatesEligibility) {
  // Sign in with unknown capability first.
  SignInUnmanagedAccountWithCapability(signin::Tribool::kUnknown);
  SetWorkspaceEligibility(/*is_disabled=*/false);

  // User should be ineligible because capabilities are unknown.
  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());

  // Trigger extended account info update for the primary account, setting
  // capabilities to true.
  AccountInfo account_info =
      identity_manager_->FindExtendedAccountInfoByAccountId(
          identity_manager_->GetPrimaryAccountId(
              signin::ConsentLevel::kSignin));
  AccountCapabilitiesTestMutator mutator(&account_info);
  mutator.set_can_use_model_execution_features(true);
  mutator.set_can_use_gemini_in_chrome(true);
  signin::UpdateAccountInfoForAccount(identity_manager_, account_info);

  // User should now be eligible.
  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());
}

// Tests that a refresh token error state update for the primary account
// correctly updates the service's eligibility.
TEST_F(GeminiServiceImplTest,
       OnErrorStateOfRefreshTokenUpdatedForAccount_UpdatesEligibility) {
  // Sign in as eligible.
  SignInUnmanagedAccountWithCapability(signin::Tribool::kTrue);
  SetWorkspaceEligibility(/*is_disabled=*/false);
  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());

  // Trigger refresh token error state update to make it invalid.
  CoreAccountId account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager_, account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::UNKNOWN));

  // User should now be ineligible.
  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());
}

// Tests that changing the Gemini policy prefs correctly updates the service's
// eligibility.
TEST_F(GeminiServiceImplTest, OnPolicyPrefChanged_UpdatesEligibility) {
  // Sign in as eligible.
  SignInUnmanagedAccountWithCapability(signin::Tribool::kTrue);
  SetWorkspaceEligibility(/*is_disabled=*/false);
  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());

  // Trigger GenAI policy preference change (disallow).
  pref_service_->SetInteger(
      prefs::kGenAiEnabledByPolicy,
      static_cast<int>(gemini::GenAiDefaultSettingsPolicy::kNotAllowed));

  // User should now be ineligible.
  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());

  // Reset GenAI policy and check that user is eligible again.
  pref_service_->SetInteger(
      prefs::kGenAiEnabledByPolicy,
      static_cast<int>(
          gemini::GenAiDefaultSettingsPolicy::kAllowedWithoutImprovingModels));
  EXPECT_TRUE(gemini_service_->IsProfileEligibleForGemini());

  // Trigger Gemini policy preference change (disallow).
  pref_service_->SetInteger(
      prefs::kGeminiEnabledByPolicy,
      static_cast<int>(gemini::SettingsPolicy::kNotAllowed));

  // User should now be ineligible again.
  EXPECT_FALSE(gemini_service_->IsProfileEligibleForGemini());
}
