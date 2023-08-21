// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook.h"

#import <Foundation/Foundation.h>

#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

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
bool DisableFirstRun() {
  // Always disable FRE for perf tests.
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
bool DisableMainThreadFreezeDetection() {
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
std::unique_ptr<password_manager::BulkLeakCheckServiceInterface>
GetOverriddenBulkLeakCheckService() {
  return nullptr;
}
std::unique_ptr<password_manager::RecipientsFetcher>
GetOverriddenRecipientsFetcher() {
  return nullptr;
}
void SetUpTestsIfPresent() {}
void RunTestsIfPresent() {}

}  // namespace tests_hook
