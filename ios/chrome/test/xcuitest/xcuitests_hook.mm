// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/test/allow_check_is_test_for_testing.h"
#import "base/time/time.h"
#import "components/feature_engagement/public/feature_activation.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#import "ios/chrome/app/tests_hook.h"

namespace tests_hook {

bool DisableAppGroupAccess() {
  return true;
}

bool DisableClientSideFieldTrials() {
  return true;
}

bool DisableContentSuggestions() {
  return true;
}

bool DisableDiscoverFeed() {
  return true;
}

bool DisableDefaultFirstRun() {
  return true;
}

bool DisableDefaultSearchEngineChoice() {
  return true;
}

bool DisableGeolocation() {
  return true;
}

bool DisablePromoManagerDisplayingPromo() {
  return true;
}

bool DisableUpgradeSigninPromo() {
  return true;
}

bool DisableUpdateService() {
  return true;
}

bool DelayAppLaunchPromos() {
  return true;
}

bool NeverPurgeDiscardedSessionsData() {
  return true;
}

std::unique_ptr<ProfileOAuth2TokenService> GetOverriddenTokenService(
    PrefService* user_prefs,
    std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate) {
  return nullptr;
}

policy::ConfigurationPolicyProvider* GetOverriddenPlatformPolicyProvider() {
  return nullptr;
}

bool SimulatePostDeviceRestore() {
  return false;
}

std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager() {
  return nullptr;
}

std::unique_ptr<TrustedVaultClientBackend> CreateTrustedVaultClientBackend() {
  return nullptr;
}

std::unique_ptr<tab_groups::TabGroupSyncService> CreateTabGroupSyncService(
    ProfileIOS* profile) {
  return nullptr;
}

void DataSharingServiceHooks(
    data_sharing::DataSharingService* data_sharing_service) {}

std::unique_ptr<ShareKitService> CreateShareKitService(
    data_sharing::DataSharingService* data_sharing_service,
    collaboration::CollaborationService* collaboration_service,
    tab_groups::TabGroupSyncService* sync_service,
    TabGroupService* tab_group_service) {
  return nullptr;
}

std::unique_ptr<password_manager::BulkLeakCheckServiceInterface>
GetOverriddenBulkLeakCheckService() {
  return nullptr;
}

std::unique_ptr<plus_addresses::PlusAddressService>
GetOverriddenPlusAddressService() {
  return nullptr;
}

std::unique_ptr<password_manager::RecipientsFetcher>
GetOverriddenRecipientsFetcher() {
  return nullptr;
}

void SetUpTestsIfPresent() {
  base::test::AllowCheckIsTestForTesting();
}

void RunTestsIfPresent() {
  // No-op for XCUITest.
}

void SignalAppLaunched() {
  // No-op for XCUITest.
}

base::TimeDelta PasswordCheckMinimumDuration() {
  // No artificial delays for tests.
  return base::Seconds(0);
}

base::TimeDelta GetOverriddenSnackbarDuration() {
  return base::Seconds(0);
}

std::unique_ptr<drive::DriveService> GetOverriddenDriveService() {
  return nullptr;
}

feature_engagement::FeatureActivation FETDemoModeOverride() {
  return feature_engagement::FeatureActivation::AllEnabled();
}

void WipeProfileIfRequested(int argc, char* argv[]) {
  // Do nothing.
}

base::TimeDelta
GetOverriddenDelayForRequestingTurningOnCredentialProviderExtension() {
  return base::Seconds(0);
}

}  // namespace tests_hook
