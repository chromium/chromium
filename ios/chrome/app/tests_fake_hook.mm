// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
#import "ios/chrome/app/tests_hook.h"
// clang-format on

#import "base/time/time.h"
#import "build/branding_buildflags.h"
#import "components/commerce/core/shopping_service.h"
#import "components/feature_engagement/public/feature_activation.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_constants.h"

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
#import "ios/chrome/app/tests_hook_helper.h"  // nogncheck
#endif

namespace tests_hook {

bool DisableGeminiEligibilityCheck() {
  return false;
}

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
bool DisableFullscreenSigninPromo() {
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

bool ShouldLoadMinimalAppUI() {
  return false;
}

void LoadMinimalAppUI(UIWindow* window) {}

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
void SignalAppLaunched() {}

base::TimeDelta PasswordCheckMinimumDuration() {
  return base::Seconds(3);
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

std::optional<base::TimeDelta> GetOverrideInfobarDuration() {
  return std::nullopt;
}

UIImage* GetPHPickerViewControllerImage() {
  return nil;
}

std::unique_ptr<AimEligibilityService> CreateAimEligibilityService(
    ProfileIOS* profile) {
  return nullptr;
}

std::unique_ptr<contextual_search::ContextualSearchService>
CreateContextualSearchService(ProfileIOS* profile) {
  return nullptr;
}

void InjectFakeTabsInBrowser(Browser* browser) {
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  NSString* const kAddLotsOfTabs = @"AddLotsOfTabs";

  int tabCountToAdd =
      [[NSUserDefaults standardUserDefaults] integerForKey:kAddLotsOfTabs];
  // Also check an environment variable for some other test environments which
  // expect a minimum number of tabs.
  if (tabCountToAdd == 0) {
    tabCountToAdd = [[NSProcessInfo.processInfo.environment
        objectForKey:@"MINIMUM_TAB_COUNT"] intValue];
  }
  if (tabCountToAdd > 0) {
    [[NSUserDefaults standardUserDefaults] setInteger:0 forKey:kAddLotsOfTabs];
    InjectUnrealizedWebStatesUntilListHasSizeItems(browser, tabCountToAdd);
  }
#endif
}

}  // namespace tests_hook
