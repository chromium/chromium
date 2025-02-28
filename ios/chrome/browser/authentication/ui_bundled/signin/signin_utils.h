// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_UTILS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_UTILS_H_

#import <UIKit/UIKit.h>

#import "base/functional/callback_forward.h"
#import "base/ios/block_types.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/data_type.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

class Browser;
class ChromeAccountManagerService;
@class MDCSnackbarMessage;
class ProfileIOS;

namespace signin_metrics {
enum class ProfileSignout;
}  // namespace signin_metrics

namespace base {
class TimeDelta;
class Version;
}  // namespace base

namespace syncer {
class SyncService;
}

namespace signin {

class IdentityManager;

using UnsyncedDataForSignoutOrProfileSwitchingCallback =
    base::OnceCallback<void(syncer::DataTypeSet data_type_set)>;

// Returns the maximum allowed waiting time for the Account Capabilities API.
base::TimeDelta GetWaitThresholdForCapabilities();

// Returns true if this user sign-in upgrade should be shown for `profile`.
bool ShouldPresentUserSigninUpgrade(ProfileIOS* profile,
                                    const base::Version& current_version);

// Returns true if the web sign-in dialog can be presented. If false, user
// actions is recorded to track why the sign-in dialog was not presented.
bool ShouldPresentWebSignin(ProfileIOS* profile);

// This method should be called when sign-in starts from the upgrade promo.
// It records in user defaults:
//   + the Chromium current version.
//   + increases the sign-in promo display count.
//   + Gaia ids list.
// Separated out into a discrete function to allow overriding when testing.
void RecordUpgradePromoSigninStarted(
    signin::IdentityManager* identity_manager,
    ChromeAccountManagerService* account_manager_service,
    const base::Version& current_version);

// Returns the current sign-in state of primary identity.
IdentitySigninState GetPrimaryIdentitySigninState(ProfileIOS* profile);

// Converts a SystemIdentityCapabilityResult to a Tribool.
Tribool TriboolFromCapabilityResult(SystemIdentityCapabilityResult result);

// Returns the list of all accounts on the device, including the ones that are
// assigned to other profiles, in the order provided by the system, from
// (depending on feature flags) IdentityManager and/or
// ChromeAccountManagerService.
NSArray<id<SystemIdentity>>* GetIdentitiesOnDevice(
    signin::IdentityManager* identityManager,
    ChromeAccountManagerService* accountManagerService);
// Convenience version that grabs the required services from the `profile`.
NSArray<id<SystemIdentity>>* GetIdentitiesOnDevice(ProfileIOS* profile);

// Returns the default identity on the device, i.e. the first one returned by
// GetIdentitiesOnDevice(), or nil if there are none.
id<SystemIdentity> GetDefaultIdentityOnDevice(
    signin::IdentityManager* identityManager,
    ChromeAccountManagerService* accountManagerService);
// Convenience version that grabs the required services from the `profile`.
id<SystemIdentity> GetDefaultIdentityOnDevice(ProfileIOS* profile);

// Switch profile if needed then sign out from the current profile.
void MultiProfileSignOut(Browser* browser,
                         signin_metrics::ProfileSignout signout_source,
                         bool force_snackbar_over_toolbar,
                         MDCSnackbarMessage* snackbar_message,
                         ProceduralBlock signout_completion,
                         bool should_record_metrics = true);

// Similar to `MultiProfileSignOut`, but switches to personal profile in all
// windows and not just one. This also skips recording metrics for single
// profile signout as policies have their own metrics for signout.
void MultiProfileSignOutForProfile(
    ProfileIOS* profile,
    signin_metrics::ProfileSignout signout_source,
    base::OnceClosure signout_completion_closure);

// Returns whether the sign-in fullscreen promo migration is done.
bool IsFullscreenSigninPromoManagerMigrationDone();

// Log to UserDefaults when the sign-in fullscreen promo impressions migration
// is done.
void LogFullscreenSigninPromoManagerMigrationDone();

// Fetches asynchronously the unsynced data types for a sign-out or a profile
// switching. And calls `callback`.
void FetchUnsyncedDataForSignOutOrProfileSwitching(
    syncer::SyncService* sync_service,
    UnsyncedDataForSignoutOrProfileSwitchingCallback callback);

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_UTILS_H_
