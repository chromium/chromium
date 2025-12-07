// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_UI_UTIL_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_UI_UTIL_H_

#import <UIKit/UIKit.h>

#include <string>
#include <string_view>

#include "base/ios/block_types.h"

@class ActionSheetCoordinator;
@class AlertCoordinator;
class AuthenticationService;
class Browser;
class GaiaId;
class PrefService;
class ProfileIOS;

namespace signin {
class IdentityManager;
}  // namespace signin

namespace signin_metrics {
enum class AccessPoint;
}  // namespace signin_metrics

// Sign-out result, related to SignoutActionSheetCoordinator().
typedef NS_ENUM(NSUInteger, SignoutActionSheetCoordinatorResult) {
  // The user canceled the sign-out confirmation dialog.
  SignoutActionSheetCoordinatorResultCanceled,
  // The user chose to sign-out and clear their data from the device.
  SignoutActionSheetCoordinatorResultClearFromDevice,
  // The user chose to sign-out and keep their data on the device.
  SignoutActionSheetCoordinatorResultKeepOnDevice,
};

// Enum to describe all 3 cases for a user being signed-in.
enum class SignedInUserState {
  // Sign-in with UNO. The sign-out needs to ask confirmation to sign out only
  // if there are unsaved data. When signed out, a snackbar needs to be
  // displayed.
  kNotSyncingAndReplaceSyncWithSignin,
  // Sign-in with UNO, where the user is managed, and was migrated from the
  // syncing state. In this state, data needs to be cleared on signout only when
  // kSeparateProfilesForManagedAccounts is disabled.
  kManagedAccountAndMigratedFromSyncing,
  // Signed in with managed account with the ClearDeviceDataOnSignoutForManaged
  // user feature enabled. In this state, data needs to be cleared on signout
  // only when kSeparateProfilesForManagedAccounts is disabled.
  kManagedAccountClearsDataOnSignout
};

// Sign-out completion block.
using SignoutActionSheetCoordinatorCompletion =
    void (^)(SignoutActionSheetCoordinatorResult result);
// Completion block for `GetLeavingPrimaryAccountConfirmationDialog()`.
// `continue_flow` is true if the user wants to continue the sign-out or the
// account switching.
using LeavingPrimaryAccountConfirmationDialogCompletion =
    void (^)(bool continue_flow);

// Returns the hosted domain for the primary account.
std::u16string HostedDomainForPrimaryAccount(
    signin::IdentityManager* identity_manager);

// Returns the sign in alert coordinator for `error`. `dismissAction` is called
// when the dialog is dismissed (the user taps on the Ok button) or cancelled
// (the alert coordinator is cancelled programatically).
AlertCoordinator* ErrorCoordinator(NSError* error,
                                   ProceduralBlock dismissAction,
                                   UIViewController* viewController,
                                   Browser* browser);

// Returns a message to display, as an error, to the user. This message
// contains:
//  * localized description (if any)
//  * domain name
//  * error code
//  * underlying errors recursively (only the domain name and the error code)
NSString* DialogMessageFromError(NSError* error);

// Returns the sign in alert coordinator for `error`, with no associated
// action. An action should be added before starting it.
AlertCoordinator* ErrorCoordinatorNoItem(NSError* error,
                                         UIViewController* viewController,
                                         Browser* browser);

// Returns a string for the view controller presentation status. This string
// can only be used for class description for debug purposes.
// `view_controller` can be nil.
NSString* ViewControllerPresentationStatusDescription(
    UIViewController* view_controller);

// Returns an alert coordinator asking the user whether they accept to switch to
// a managed account.
AlertCoordinator* ManagedConfirmationDialogContentForHostedDomain(
    NSString* hosted_domain,
    Browser* browser,
    UIViewController* view_controller,
    ProceduralBlock accept_block,
    ProceduralBlock cancel_block);

// Returns YES if the managed confirmation dialog should be shown for the
// hosted domain.
BOOL ShouldShowManagedConfirmationForHostedDomain(
    NSString* hosted_domain,
    signin_metrics::AccessPoint access_point,
    const GaiaId& gaia_ID,
    PrefService* prefs);

// Returns the current sign-in&sync state.
SignedInUserState GetSignedInUserState(
    AuthenticationService* authentication_service,
    signin::IdentityManager* identity_manager,
    PrefService* profile_pref_service);

// Returns `true` if the dialog from
// `GetLeavingPrimaryAccountConfirmationDialog()` needs to be shown, even if
// there is no unsynced data.
bool ForceLeavingPrimaryAccountConfirmationDialog(
    SignedInUserState signed_in_user_state,
    ProfileIOS* profile);

// Returns a dialog for the user to confirm to sign out, switch account.
// `anchorView` and `anchorRect` is the position that triggered sign-in.
// `account_profile_switch` is true if the flow was triggered for an account or
// profile switching.
// `signed_in_user_state` sign-in&sync state for the current primary account.
// `completion` called once the user closes the dialog.
ActionSheetCoordinator* GetLeavingPrimaryAccountConfirmationDialog(
    UIViewController* base_view_controller,
    Browser* browser,
    UIView* anchor_view,
    CGRect anchor_rect,
    SignedInUserState signed_in_user_state,
    bool account_profile_switch,
    LeavingPrimaryAccountConfirmationDialogCompletion completion);

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_AUTHENTICATION_UI_UTIL_H_
