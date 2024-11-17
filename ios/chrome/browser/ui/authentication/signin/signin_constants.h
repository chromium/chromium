// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

// Sign-in result returned Sign-in result.
typedef NS_ENUM(NSUInteger, SigninCoordinatorResult) {
  // Sign-in has been canceled by the user or by another reason.
  SigninCoordinatorResultCanceledByUser,
  // The flow was interrupted, e.g. because the user opened a new URL or the app
  // was terminated.
  // Sign-in might have finished or not.
  SigninCoordinatorResultInterrupted,
  // Sign-in has been done, the user has explicitly accepted sign-in.
  SigninCoordinatorResultSuccess,
  // Sign-in did not complete because it is disabled. This can happen if
  // enterprise policies are updated after sign-in is started.
  SigninCoordinatorResultDisabled,
};

// User's signed-in state as defined by AuthenticationService.
// TODO(crbug.com/40066949): Revisit after phase 3 migration of syncing users.
typedef NS_ENUM(NSUInteger, IdentitySigninState) {
  IdentitySigninStateSignedOut,
  IdentitySigninStateSignedInWithSyncDisabled,
  IdentitySigninStateSignedInWithSyncEnabled,
};

// Action to do when the sign-in dialog needs to be interrupted.
enum class SigninCoordinatorInterrupt {
  // Stops the sign-in coordinator without dismissing the view. The sign-in
  // completion block and the interrupt completion block will be called
  // synchronously.
  // This should be only used when UI shutdown.
  // See crbug.com/1455216.
  UIShutdownNoDismiss,
  // Stops the sign-in coordinator and dismisses the view without animation.
  DismissWithoutAnimation,
  // Stops the sign-in coordinator and dismisses the view with animation.
  DismissWithAnimation,
};

// Name of accessibility identifier for the skip sign-in button.
extern NSString* const kSkipSigninAccessibilityIdentifier;
// Name of accessibility identifier for the add account button in the sign-in
// flow.
extern NSString* const kAddAccountAccessibilityIdentifier;
// Name of accessibility identifier for the confirmation "Yes I'm In" sign-in
// button.
extern NSString* const kConfirmationAccessibilityIdentifier;
// Name of the accessibility identifier for the History Sync view.
extern NSString* const kHistorySyncViewAccessibilityIdentifier;
// Name of accessibility identifier for the more button in the sign-in flow.
extern NSString* const kMoreAccessibilityIdentifier;
// Name of accessibility identifier for the web sign-in consistency sheet.
extern NSString* const kWebSigninAccessibilityIdentifier;
// Name of accessibility identifier for the primary button that signs in
// the user for the web sign-in consistency sheet.
extern NSString* const kWebSigninPrimaryButtonAccessibilityIdentifier;
// Name of accessibility identifier for "Skip" button in the web sign-in
// consistency sheet.
extern NSString* const kWebSigninSkipButtonAccessibilityIdentifier;
// Name of the accessibility identifier for the Tangible Sync view.
extern NSString* const kTangibleSyncViewAccessibilityIdentifier;
// Name of the accessibility identifier for the "add account" button in the
// consistency account chooser.
extern NSString* const kConsistencyAccountChooserAddAccountIdentifier;

// Action that is required to do to complete the sign-in, or instead of sign-in.
// This action is in charge of the SigninCoordinator's owner.
typedef NS_ENUM(NSUInteger, SigninCompletionAction) {
  // No action needed.
  SigninCompletionActionNone,
  // The user tapped the manager, learn more, link and sign-in was cancelled.
  SigninCompletionActionShowManagedLearnMore,
};

// Intent for TrustedVaultReauthenticationCoordinator to display either
// the reauthentication or degraded recoverability dialog.
typedef NS_ENUM(NSUInteger, SigninTrustedVaultDialogIntent) {
  // Show reauthentication dialog for fetch keys.
  SigninTrustedVaultDialogIntentFetchKeys,
  // Show reauthentication degraded recoverability dialog (to enroll additional
  // recovery factors).
  SigninTrustedVaultDialogIntentDegradedRecoverability,
};

// Max dismissal count for web sign-in consistency dialog (the dismissal value
// is reset as soon as the user shows sign-in intent).
extern const int kDefaultWebSignInDismissalCount;

// Values of the UMA SSORecallPromo.PromoAction histogram.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. When you add a new entry or when you
// deprecate an existing one, also update SSOPromoUserAction in enums.xml and
// SyncDataType suffix in histograms.xml.
typedef NS_ENUM(NSUInteger, UserSigninPromoAction) {
  PromoActionDismissed = 0,
  PromoActionEnabledSSOAccount = 1,
  PromoActionAddedAnotherAccount = 2,
  PromoActionCount = 3,
};

// Key in the UserDefaults to record the version of the application when the
// sign-in promo has been displayed. The value is set on the first cold start to
// make sure the sign-in promo is not triggered right after the FRE.
// Exposed for testing.
extern NSString* const kDisplayedSSORecallForMajorVersionKey;
// Key in the UserDefaults to record the GAIA id list when the sign-in promo
// was shown.
// Exposed for testing.
extern NSString* const kLastShownAccountGaiaIdVersionKey;
// Key in the UserDefaults to record the number of times the sign-in promo has
// been shown.
// TODO(crbug.com/40831586): Need to merge with
// kDisplayedSSORecallPromoCountKey. Exposed for testing.
extern NSString* const kSigninPromoViewDisplayCountKey;
// Key in the UserDefaults to track how many times the SSO Recall promo has been
// displayed.
// TODO(crbug.com/40831586): Need to merge with kSigninPromoViewDisplayCountKey.
// Exposed for testing.
extern NSString* const kDisplayedSSORecallPromoCountKey;
// Name of the UMA SSO Recall histogram.
extern const char* const kUMASSORecallPromoAction;
// Name of the histogram recording how many accounts were available on the
// device when the promo was shown.
extern const char* const kUMASSORecallAccountsAvailable;
// Name of the histogram recording how many times the promo has been shown.
extern const char* const kUMASSORecallPromoSeenCount;

// Default timeout to wait for fetching account capabilities, which determine
// minor mode restrictions status.
inline constexpr base::TimeDelta kMinorModeRestrictionsFetchDeadline =
    base::Milliseconds(500);

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_CONSTANTS_H_
