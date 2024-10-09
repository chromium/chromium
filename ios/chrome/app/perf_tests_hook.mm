// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook.h"

#import <Foundation/Foundation.h>
#import <os/log.h>
#import <os/signpost.h>

#import "base/time/time.h"
#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"
#import "ios/public/provider/chrome/browser/primes/primes_api.h"

namespace tests_hook {

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
bool DisablePromoManagerFullScreenPromos() {
  // Always disable full-screen promos for perf tests.
  return true;
}
std::unique_ptr<ProfileOAuth2TokenService> GetOverriddenTokenService(
    PrefService* user_prefs,
    std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate) {
  return nullptr;
}
bool DisableUpgradeSigninPromo() {
  // Always disable upgrade sign-in promo for perf tests.
  return true;
}
bool DisableUpdateService() {
  return false;
}

bool DelayAppLaunchPromos() {
  return true;
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

void SignalAppLaunched() {
  // The app launched signal is only used by startup tests, which unlike EG
  // tests do not have a tear down method which stops logging, so stop logging
  // here to flush logs
  ios::provider::PrimesStopLogging();

  os_log_t hke_os_log = os_log_create("com.google.hawkeye.ios",
                                      OS_LOG_CATEGORY_POINTS_OF_INTEREST);
  os_signpost_id_t os_signpost = os_signpost_id_generate(hke_os_log);
  os_signpost_event_emit(hke_os_log, os_signpost, "APP_LAUNCHED");
  // For startup tests instrumented with xctrace we need to log the signal using
  // os_log
  os_log(hke_os_log, "APP_LAUNCHED");

  // For regular startup tests we rely on printf to signal that the app has
  // started and can be terminated
  printf("APP_LAUNCHED\n");
}

base::TimeDelta PasswordCheckMinimumDuration() {
  // No artificial delays for perf tests.
  return base::Seconds(0);
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
