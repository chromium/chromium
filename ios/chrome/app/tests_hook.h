// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TESTS_HOOK_H_
#define IOS_CHROME_APP_TESTS_HOOK_H_

#include <memory>

class PrefService;
class ProfileOAuth2TokenServiceDelegate;
class ProfileOAuth2TokenService;
class SystemIdentityManager;
namespace policy {
class ConfigurationPolicyProvider;
}
namespace password_manager {
class BulkLeakCheckServiceInterface;
}

namespace tests_hook {

// Returns true if app group access should be disabled as tests don't have the
// required entitlements.
// This is used by internal code.
bool DisableAppGroupAccess();

// Returns true if client-side field trials should be disabled, so
// that their associated base::Features always use the default
// behavior, avoiding unexpected randomness during testing.
bool DisableClientSideFieldTrials();

// Returns true if ContentSuggestions should be disabled to allow other tests to
// run unimpeded.
bool DisableContentSuggestions();

// Returns true if Discover feed should be disabled to allow tests to run
// without it.
bool DisableDiscoverFeed();

// Returns true if the first_run path should be disabled to allow other tests to
// run unimpeded.
bool DisableFirstRun();

// Returns true if the geolocation should be disabled to avoid the user location
// prompt displaying for the omnibox.
bool DisableGeolocation();

// Returns true if the Promo Manager should avoid displaying full-screen promos
// on app startup to allow tests to run unimpeded.
bool DisablePromoManagerFullScreenPromos();

// Returns a token service that can be installed as a fake identity management
// service that bridges iOS SSO library and Chrome account info when testing.
// May return nullptr.
std::unique_ptr<ProfileOAuth2TokenService> GetOverriddenTokenService(
    PrefService* user_prefs,
    std::unique_ptr<ProfileOAuth2TokenServiceDelegate> delegate);

// Returns true if the upgrade sign-in promo should be disabled to allow other
// tests to run unimpeded.
bool DisableUpgradeSigninPromo();

// Returns true if the update service should be disabled so that the update
// infobar won't be shown during testing.
bool DisableUpdateService();

// The main thread freeze detection is interfering with the EarlGrey
// synchronization.
// Return true if it should be disabled.
bool DisableMainThreadFreezeDetection();

// Returns true if any app launch promos should delay themselves so EGTests
// can start before checking if the promo appears.
bool DelayAppLaunchPromos();

// Returns a policy provider that should be installed as the platform policy
// provider when testing. May return nullptr.
policy::ConfigurationPolicyProvider* GetOverriddenPlatformPolicyProvider();

// Allow overriding the SystemIdentityManager factory. The real factory will
// be used if this hook returns null.
std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager();

// Returns a bulk leak check service that should be used when testing. The real
// factory will be used if this hook returns a nullptr.
std::unique_ptr<password_manager::BulkLeakCheckServiceInterface>
GetOverriddenBulkLeakCheckService();

// Global integration tests setup.
void SetUpTestsIfPresent();

// Runs the integration tests.  This is not used by EarlGrey-based integration
// tests.
void RunTestsIfPresent();

}  // namespace tests_hook

#endif  // IOS_CHROME_APP_TESTS_HOOK_H_
