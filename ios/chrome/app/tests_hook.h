// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TESTS_HOOK_H_
#define IOS_CHROME_APP_TESTS_HOOK_H_

#include <memory>
#import <optional>

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class PrefService;
class ProfileOAuth2TokenServiceDelegate;
class ProfileOAuth2TokenService;
class SystemIdentityManager;
class TrustedVaultClientBackend;
namespace drive {
class DriveService;
}
namespace policy {
class ConfigurationPolicyProvider;
}
namespace password_manager {
class BulkLeakCheckServiceInterface;
class RecipientsFetcher;
}

namespace plus_addresses {
class PlusAddressService;
}

namespace tab_groups {
class TabGroupSyncService;
}

namespace base {
class TimeDelta;
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

// Returns true if the first run UI, which would interfere with many tests,
// should by default be skipped. Note that even in a target where this function
// returns `false`, that's just a default, and individual tests may still enable
// the first run UI.
bool DisableDefaultFirstRun();

// Returns true if the geolocation should be disabled to avoid the user location
// prompt displaying for the omnibox.
bool DisableGeolocation();

// Returns true if the Promo Manager should avoid displaying full-screen promos
// on app startup to allow tests to run unimpeded.
bool DisablePromoManagerFullScreenPromos();

// Returns true if the search engine choice view, which would interfere with
// many tests, should by default be skipped. Note that even in a target where
// this function returns `false`, that's just a default, and individual tests
// may still enable this view.
bool DisableDefaultSearchEngineChoice();

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

// Returns true if any app launch promos should delay themselves so EGTests
// can start before checking if the promo appears.
bool DelayAppLaunchPromos();

// Returns a policy provider that should be installed as the platform policy
// provider when testing. May return nullptr.
policy::ConfigurationPolicyProvider* GetOverriddenPlatformPolicyProvider();

// Allows overriding the SystemIdentityManager factory. The real factory will
// be used if this hook returns null.
std::unique_ptr<SystemIdentityManager> CreateSystemIdentityManager();

// Allows overriding the TrustedVaultClientBackend factory. The real factory
// will be used if this hook returns null.
std::unique_ptr<TrustedVaultClientBackend> CreateTrustedVaultClientBackend();

// Allows overriding the TabGroupSyncService factory. The real factory will be
// used if this hook returns null.
std::unique_ptr<tab_groups::TabGroupSyncService> CreateTabGroupSyncService(
    ProfileIOS* profile);

// Returns a bulk leak check service that should be used when testing. The real
// factory will be used if this hook returns a nullptr.
std::unique_ptr<password_manager::BulkLeakCheckServiceInterface>
GetOverriddenBulkLeakCheckService();

// Returns a plus address service that should be used when testing. The real
// factory will be used if this hook returns a nullptr.
std::unique_ptr<plus_addresses::PlusAddressService>
GetOverriddenPlusAddressService();

// Returns a recipients fetcher instance that should be used in EG tests. The
// real instance will be used if this hook returns a nullptr.
std::unique_ptr<password_manager::RecipientsFetcher>
GetOverriddenRecipientsFetcher();

// Global integration tests setup.
void SetUpTestsIfPresent();

// Runs the integration tests.  This is not used by EarlGrey-based integration
// tests.
void RunTestsIfPresent();

// Signal that the app has successfully launched. Only used by performance
// tests.
void SignalAppLaunched();

// Minimum duration of password checks. The password check UI displays checks as
// in progress for at least this duration in order to avoid updating the UI too
// fast and making it flicker. Test targets do not have an artificial minimum
// duration as it can make test flaky.
base::TimeDelta PasswordCheckMinimumDuration();

// Duration for snackbars. If the value is 0, the default value from
// -[MDCSnackbarMessage duration] should not be updated.
base::TimeDelta GetOverriddenSnackbarDuration();

// Returns a Drive service instance that should be used in EG tests. The real
// instance will be used if this hook returns a nullptr.
std::unique_ptr<drive::DriveService> GetOverriddenDriveService();

// Override the Feature Engagement Tracker used in tests with a demo version.
// Returning std::nullopt will not do any override. Returning any string will
// override with a demo tracker that only enables that feature (use empty string
// for a demo tracker that enables all features).
std::optional<std::string> FETDemoModeOverride();

}  // namespace tests_hook

#endif  // IOS_CHROME_APP_TESTS_HOOK_H_
