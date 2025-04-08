// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/time/time.h"
#import "components/feature_engagement/public/feature_activation.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#import "ios/chrome/app/tests_hook.h"

namespace tests_hook {

bool DisableAppGroupAccess() {
  return false;
}
bool DisableClientSideFieldTrials() {
  return false;
}
bool DisableContentSuggestions() {
  return false;
}
bool DisableDiscoverFeed() {
  return false;
}
bool DisableDefaultFirstRun() {
  return false;
}
bool DisableDefaultSearchEngineChoice() {
  return false;
}
bool DisableGeolocation() {
  return false;
}
bool DisablePromoManagerDisplayingPromo() {
  return false;
}
std::unique_ptr<ProfileOAuth2TokenService> GetOverriddenTokenService(
    PrefService* user_prefs,
    std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate) {
  return nullptr;
}
bool DisableUpgradeSigninPromo() {
  return false;
}
bool DisableUpdateService() {
  return false;
}

bool DelayAppLaunchPromos() {
  return false;
}
bool NeverPurgeDiscardedSessionsData() {
  return false;
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
void SetUpTestsIfPresent() {}
void RunTestsIfPresent() {}
void SignalAppLaunched() {}

base::TimeDelta PasswordCheckMinimumDuration() {
  return base::Seconds(3);
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
