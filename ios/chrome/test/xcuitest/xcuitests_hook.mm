// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/tests_hook.h"

#import "components/signin/internal/identity_manager/profile_oauth2_token_service_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

bool DisableFirstRun() {
  return true;
}

bool DisableGeolocation() {
  return true;
}

bool DisablePromoManagerFullScreenPromos() {
  return true;
}

bool DisableUpgradeSigninPromo() {
  return true;
}

bool DisableUpdateService() {
  return true;
}

bool DisableMainThreadFreezeDetection() {
  return true;
}

bool DelayAppLaunchPromos() {
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

std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager() {
  return nullptr;
}

void SetUpTestsIfPresent() {
  // No-op for XCUITest.
}

void RunTestsIfPresent() {
  // No-op for XCUITest.
}

}  // namespace tests_hook
