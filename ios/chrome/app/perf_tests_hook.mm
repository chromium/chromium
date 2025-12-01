// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
#import "ios/chrome/app/tests_hook.h"
// clang-format on

#import <Foundation/Foundation.h>

#import "base/time/time.h"
#import "components/commerce/core/shopping_service.h"
#import "components/feature_engagement/public/feature_activation.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"

namespace tests_hook {

bool DisableGeminiEligibilityCheck() {
  return false;
}

bool DisableAppGroupAccess() {
  return false;
}
bool DisableClientSideFieldTrials() {
  // Always disable client side field trials for perf tests.
  return true;
}
bool DisableContentSuggestions() {
  return false;
}
bool DisableDiscoverFeed() {
  // Performance tests may disable the discover feed by setting the
  // DISABLE_DISCOVER_FEED environment variable. Possible values
  // the variable may be set to are described in the apple documentation for
  // boolValue:
  // https://developer.apple.com/documentation/foundation/nsstring/1409420-boolvalue
  return [[NSProcessInfo.processInfo.environment
      objectForKey:@"DISABLE_DISCOVER_FEED"] boolValue];
}
bool DisableDefaultFirstRun() {
  // Always disable FRE for perf tests.
  return true;
}
bool DisableDefaultSearchEngineChoice() {
  // Always disable search engine selection for perf tests.
  return true;
}
bool DisableGeolocation() {
  return false;
}
bool DisablePromoManagerDisplayingPromo() {
  // Always disable full-screen promos for perf tests.
  return true;
}
std::unique_ptr<ProfileOAuth2TokenService> GetOverriddenTokenService(
    PrefService* user_prefs,
    std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate) {
  return nullptr;
}
bool DisableFullscreenSigninPromo() {
  // Always disable fullscreen sign-in promo for perf tests.
  return true;
}
bool DisableUpdateService() {
  return false;
}

bool DelayAppLaunchPromos() {
  return true;
}
bool NeverPurgeDiscardedSessionsData() {
  return false;
}

bool LoadMinimalAppUI() {
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
std::unique_ptr<commerce::ShoppingService> CreateShoppingService(
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

// Note SignalAppLaunched() is not implemented here because in needs to
// include system libraries that defines a macro PLATFORM_IOS which is
// in  conflict with the multiple uses of PLATFORM_IOS as an enumerator
// in Chromium code.
//
// The function is implemented in perf_tests_hook_logging.mm to prevent
// compilation failures.

base::TimeDelta PasswordCheckMinimumDuration() {
  // No artificial delays for perf tests.
  return base::Seconds(0);
}

std::unique_ptr<drive::DriveService> GetOverriddenDriveService() {
  return nullptr;
}

feature_engagement::FeatureActivation FETDemoModeOverride() {
  return feature_engagement::FeatureActivation::AllEnabled();
}

void WipeProfileIfRequested(base::span<const char* const> args) {
  // Do nothing.
}

base::TimeDelta
GetOverriddenDelayForRequestingTurningOnCredentialProviderExtension() {
  return base::Seconds(0);
}

base::TimeDelta GetSnackbarMessageDuration() {
  return kSnackbarMessageDuration;
}

UIImage* GetPHPickerViewControllerImage() {
  return nil;
}

}  // namespace tests_hook
