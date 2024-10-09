// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook.h"

#import "base/time/time.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

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
bool DisablePromoManagerFullScreenPromos() {
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
policy::ConfigurationPolicyProvider* GetOverriddenPlatformPolicyProvider() {
  return nullptr;
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

std::optional<std::string> FETDemoModeOverride() {
  return std::nullopt;
}

}  // namespace tests_hook
