// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_UTILS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_UTILS_H_

#import <UIKit/UIKit.h>

#import <utility>

#import "base/functional/callback.h"
#import "base/ios/block_types.h"
#import "base/time/time.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/base/data_type.h"
#import "ios/chrome/app/change_profile_continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

class Browser;
enum class ChangeProfileReason;
class ChromeAccountManagerService;
class ProfileIOS;
@class SceneState;
@class SnackbarMessage;

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

@protocol BuggyAuthenticationViewOwner;

namespace signin {

class IdentityManager;

using UnsyncedDataForSignoutOrProfileSwitchingCallback =
    base::OnceCallback<void(syncer::DataTypeSet data_type_set)>;

inline constexpr std::pair<base::TimeDelta, base::TimeDelta> kPromoTriggerRange(
    base::Days(53),
    base::Days(68));

// Represents a request to sign-out.
class ProfileSignoutRequest {
 public:
  // Callback invoked before starting the sign-out request with a boolean
  // indicating whether the operation will require changing the profile.
  using PrepareCallback = base::OnceCallback<void(bool will_change_profile)>;

  // Callback invoked when the profile switching operation has completed.
  using CompletionCallback = base::OnceCallback<void(SceneState*)>;

  explicit ProfileSignoutRequest(signin_metrics::ProfileSignout source);

  ProfileSignoutRequest(const ProfileSignoutRequest&) = delete;
  ProfileSignoutRequest& operator=(const ProfileSignoutRequest&) = delete;

  ~ProfileSignoutRequest();

  // Configures the snackbar message to display and whether it should be
  // forced over the toolbar or not.
  ProfileSignoutRequest&& SetSnackbarMessage(
      SnackbarMessage* snackbar_message,
      bool force_snackbar_over_toolbar) &&;

  // Configures the callback invoked before starting the request.
  ProfileSignoutRequest&& SetPrepareCallback(
      PrepareCallback prepare_callback) &&;

  // Configures the completion callback.
  ProfileSignoutRequest&& SetCompletionCallback(
      CompletionCallback completion_callback) &&;

  // Configures whether the metrics should be recorded.
  ProfileSignoutRequest&& SetShouldRecordMetrics(bool value) &&;

  // Starts the signout request, invoking `prepare_callback` synchronously
  // before doing any change with a boolean indicating whether the operation
  // will require changing the profile.
  void Run(Browser* browser) &&;

 private:
  const signin_metrics::ProfileSignout source_;
  PrepareCallback prepare_callback_;
  CompletionCallback completion_callback_;
  SnackbarMessage* snackbar_message_;
  bool force_snackbar_over_toolbar_ = false;
  bool should_record_metrics_ = true;
  bool run_has_been_called_ = false;
};

// Returns true if this user sign-in upgrade should be shown for `profile`.
bool ShouldPresentUserSigninUpgrade(ProfileIOS* profile,
                                    const base::Version& current_version);

// Returns true if the web sign-in dialog can be presented. If false, user
// actions is recorded to track why the sign-in dialog was not presented.
bool ShouldPresentWebSignin(ProfileIOS* profile);

// This method should be called when sign-in starts from the fullscreen signin
// promo. It records in user defaults:
//   + the Chromium current version.
//   + increases the sign-in promo display count.
//   + Gaia ids list.
// Separated out into a discrete function to allow overriding when testing.
void RecordFullscreenSigninPromoStarted(
    signin::IdentityManager* identity_manager,
    ChromeAccountManagerService* account_manager_service,
    const base::Version& current_version);

// Converts a SystemIdentityCapabilityResult to a Tribool.
Tribool TriboolFromCapabilityResult(SystemIdentityCapabilityResult result);

// Returns the list of all accounts on the device, in the order provided by the
// system, including the ones that are assigned to other profiles.
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

// Switch profile if needed in all windows then sign out from the current
// profile, but switches to personal profile in all. This also skips
// recording metrics for single profile signout as policies have their
// own metrics for signout.
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
// switching. And calls `callback` with the set of data type from
// `TypesRequiringUnsyncedDataCheckOnSignout` containing unsynced data.
void FetchUnsyncedDataForSignOutOrProfileSwitching(
    syncer::SyncService* sync_service,
    UnsyncedDataForSignoutOrProfileSwitchingCallback callback);

// Post an asynchronous request to switch from a managed profile to the
// personal profile, running `continuation` when the change completes.
void SwitchToPersonalProfile(SceneState* scene_state,
                             ChangeProfileReason reason,
                             ChangeProfileContinuation continuation);

// Whether there exists a scene with a profile different from the one of this
// scene where the user is signed-in.
bool DifferentUserIsSignedInInAnotherScene(SceneState* scene_state);

// Returns the regular browser.
Browser* GetRegularBrowser(Browser* browser);

}  // namespace signin

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_UTILS_H_
