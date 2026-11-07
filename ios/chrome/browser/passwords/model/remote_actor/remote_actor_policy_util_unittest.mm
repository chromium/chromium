// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_policy_util.h"

#import <memory>
#import <utility>

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "base/values.h"
#import "components/metrics/metrics_state_manager.h"
#import "components/metrics/test/test_enabled_state_provider.h"
#import "components/policy/core/common/management/management_service.h"
#import "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#import "components/prefs/testing_pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "components/variations/service/test_variations_service.h"
#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/browser/policy/model/browser_management_service.h"
#import "ios/chrome/browser/policy/model/browser_management_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_manager_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

constexpr int kSparkAllowed =
    static_cast<int>(gemini::SparkSettingsPolicy::kAllowed);
constexpr int kSparkNotAllowed =
    static_cast<int>(gemini::SparkSettingsPolicy::kNotAllowed);

class RemoteActorPolicyUtilTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = profile_manager_.AddProfileWithBuilder(CreateProfileBuilder());
  }

  TestProfileIOS::Builder CreateProfileBuilder() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    return builder;
  }

  void SignIn(FakeSystemIdentity* identity) {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    system_identity_manager->AddIdentity(identity);
    AuthenticationServiceFactory::GetForProfile(profile_.get())
        ->SignIn(identity, signin_metrics::AccessPoint::kStartPage);
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestProfileManagerIOS profile_manager_;
  raw_ptr<TestProfileIOS> profile_;
};

// Tests that `IsRemoteActorCredentialSharingAllowedByPolicy()` returns `false`
// when given a null or off-the-record profile.
TEST_F(RemoteActorPolicyUtilTest, NullOrOffTheRecordProfileDisallowed) {
  EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedByPolicy(nullptr));
  EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedByPolicy(
      profile_->GetOffTheRecordProfile()));
}

// Tests that `IsRemoteActorCredentialSharingAllowedByPolicy()` returns `true`
// by default for an unmanaged profile even though `kGeminiSparkSettings`
// defaults to `kNotAllowed` (`1`).
TEST_F(RemoteActorPolicyUtilTest, UnmanagedProfileAllowedByDefault) {
  ASSERT_EQ(prefs()->GetInteger(prefs::kGeminiSparkSettings), kSparkNotAllowed);
  ASSERT_FALSE(prefs()->IsManagedPreference(prefs::kGeminiSparkSettings));

  SignIn([FakeSystemIdentity fakeIdentity1]);
  EXPECT_TRUE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));
}

// Tests that `IsRemoteActorCredentialSharingAllowedByPolicy()` blocks a
// browser-managed profile unless `kGeminiSparkSettings` is set to `kAllowed`
// (`0`).
TEST_F(RemoteActorPolicyUtilTest, BrowserManagedProfileRequiresPolicyEnabled) {
  policy::ScopedManagementServiceOverrideForTesting management_override(
      policy::BrowserManagementServiceFactory::GetForProfile(profile_.get()),
      policy::EnterpriseManagementAuthority::CLOUD);

  EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));

  prefs()->SetManagedPref(prefs::kGeminiSparkSettings,
                          base::Value(kSparkAllowed));
  EXPECT_TRUE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));
}

// Tests that `IsRemoteActorCredentialSharingAllowedByPolicy()` blocks a
// device-managed client unless `kGeminiSparkSettings` is set to `kAllowed`
// (`0`).
TEST_F(RemoteActorPolicyUtilTest, PlatformManagedDeviceRequiresPolicyEnabled) {
  policy::ScopedManagementServiceOverrideForTesting management_override(
      policy::BrowserManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::DOMAIN_LOCAL);

  EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));

  prefs()->SetManagedPref(prefs::kGeminiSparkSettings,
                          base::Value(kSparkAllowed));
  EXPECT_TRUE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));
}

// Tests that `IsRemoteActorCredentialSharingAllowedByPolicy()` blocks a profile
// signed in with a managed enterprise account unless `kGeminiSparkSettings` is
// set to `kAllowed` (`0`).
TEST_F(RemoteActorPolicyUtilTest, ManagedAccountRequiresPolicyEnabled) {
  FakeSystemIdentity* managed_identity =
      [FakeSystemIdentity fakeManagedIdentity];
  FakeSystemIdentityManager::FromSystemIdentityManager(
      GetApplicationContext()->GetSystemIdentityManager())
      ->AddIdentity(managed_identity);
  TestProfileIOS* managed_profile =
      profile_manager_.AddManagedProfileForGaiaIDWithBuilder(
          managed_identity.gaiaId, CreateProfileBuilder());
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(managed_profile);
  auth_service->SignIn(managed_identity,
                       signin_metrics::AccessPoint::kStartPage);
  ASSERT_TRUE(auth_service->HasPrimaryIdentityManaged());

  EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedByPolicy(managed_profile));

  managed_profile->GetTestingPrefService()->SetManagedPref(
      prefs::kGeminiSparkSettings, base::Value(kSparkAllowed));
  EXPECT_TRUE(IsRemoteActorCredentialSharingAllowedByPolicy(managed_profile));
}

// Tests that `IsRemoteActorCredentialSharingAllowedByPolicy()` respects a
// policy-managed `kGeminiSparkSettings` preference even without other
// management signals.
TEST_F(RemoteActorPolicyUtilTest, ManagedSparkPrefEnforced) {
  prefs()->SetManagedPref(prefs::kGeminiSparkSettings,
                          base::Value(kSparkNotAllowed));
  EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));

  prefs()->SetManagedPref(prefs::kGeminiSparkSettings,
                          base::Value(kSparkAllowed));
  EXPECT_TRUE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));
}

// Tests that `IsRemoteActorCredentialSharingAllowedByPolicy()` blocks sharing
// when `kGeminiSparkSettings` is `kAllowed` (`0`) if any prerequisite Gemini or
// Act-on-Web policy is disabled, matching `gemini::GeminiAllowedByPolicy()`.
TEST_F(RemoteActorPolicyUtilTest, PrerequisitePoliciesEnforced) {
  prefs()->SetManagedPref(prefs::kGeminiSparkSettings,
                          base::Value(kSparkAllowed));
  ASSERT_TRUE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));

  // Disabled via `GeminiActOnWebSettings` = 1.
  prefs()->SetManagedPref(prefs::kGeminiActOnWebSettings, base::Value(1));
  EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));
  prefs()->RemoveManagedPref(prefs::kGeminiActOnWebSettings);

  // Disabled via `GeminiSettings` = 1 (`kNotAllowed`).
  prefs()->SetManagedPref(
      prefs::kGeminiEnabledByPolicy,
      base::Value(static_cast<int>(gemini::SettingsPolicy::kNotAllowed)));
  EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));
  prefs()->RemoveManagedPref(prefs::kGeminiEnabledByPolicy);

  // Disabled via `GenAiDefaultSettings` = 2 (`kNotAllowed`) when
  // `GeminiSettings` is unset.
  prefs()->SetManagedPref(
      prefs::kGenAiEnabledByPolicy,
      base::Value(
          static_cast<int>(gemini::GenAiDefaultSettingsPolicy::kNotAllowed)));
  EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));

  // Remains disabled even if `GeminiSettings` is explicitly `0` (`kAllowed`),
  // matching `gemini::GeminiAllowedByPolicy()`.
  prefs()->SetManagedPref(
      prefs::kGeminiEnabledByPolicy,
      base::Value(static_cast<int>(gemini::SettingsPolicy::kAllowed)));
  EXPECT_FALSE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));

  // Allowed when `GenAiDefaultSettings` is `0` or `1`.
  for (auto allowed_gen_ai :
       {gemini::GenAiDefaultSettingsPolicy::kAllowedImprovingModels,
        gemini::GenAiDefaultSettingsPolicy::kAllowedWithoutImprovingModels}) {
    SCOPED_TRACE(static_cast<int>(allowed_gen_ai));
    prefs()->SetManagedPref(prefs::kGenAiEnabledByPolicy,
                            base::Value(static_cast<int>(allowed_gen_ai)));
    EXPECT_TRUE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));
  }
}

// Tests that `IsRemoteActorCredentialSharingAllowedByPolicy()` exempts dogfood
// clients from managed default blocking, matching Desktop Glic policy behavior.
TEST_F(RemoteActorPolicyUtilTest, DogfoodClientExempted) {
  TestingPrefServiceSimple variations_prefs;
  variations::TestVariationsService::RegisterPrefs(variations_prefs.registry());
  metrics::TestEnabledStateProvider enabled_state_provider(/*consent=*/false,
                                                           /*enabled=*/false);
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager =
      metrics::MetricsStateManager::Create(
          &variations_prefs, &enabled_state_provider, std::wstring(),
          base::FilePath(), metrics::StartupVisibility::kUnknown);
  variations::TestVariationsService variations_service(
      &variations_prefs, metrics_state_manager.get());
  variations_service.SetIsLikelyDogfoodClientForTesting(true);

  TestingApplicationContext::GetGlobal()->SetVariationsService(
      &variations_service);
  struct ScopedVariationsServiceReset {
    ~ScopedVariationsServiceReset() {
      TestingApplicationContext::GetGlobal()->SetVariationsService(nullptr);
    }
  } reset_variations_service;

  policy::ScopedManagementServiceOverrideForTesting management_override(
      policy::BrowserManagementServiceFactory::GetForProfile(profile_.get()),
      policy::EnterpriseManagementAuthority::CLOUD);

  EXPECT_TRUE(IsRemoteActorCredentialSharingAllowedByPolicy(profile_.get()));
}

}  // namespace
