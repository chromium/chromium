// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "base/time/time.h"

@class SigninCoordinator;
@protocol SystemIdentity;

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
  // Sign-in cannot start as the UI is not available. In this case, no
  // SigninCoordinator object is created.
  // Only triggered by `SceneController` when processing a ShowSigninCommand
  // and when the UI is not ready to present any signin coordinator.
  SigninCoordinatorUINotAvailable,
  // Sign-in coordinator is stopped because of a change in profile.
  SigninCoordinatorProfileSwitch,
};

namespace signin_ui {

// The result of a authentiaction.
enum class CancelationReason {
  // Not canceled.
  kNotCanceled,
  // Canceled by the user.
  kUserCanceled,
  // Canceled, but not by the user.
  kFailed,
};

}  // namespace signin_ui

// Enum for tracking key events in the Signin Fullscreen Promo flow on iOS.
// Used in UMA histograms.
// LINT.IfChange(SigninFullscreenPromoEvents)
enum class SigninFullscreenPromoEvents {
  // Recorded when the Promo Manager selects and triggers the display
  // of the fullscreen sign-in promo.
  kPromoManagerTriggered = 0,

  // Recorded when the promo is prevented from showing due to
  // SigninDisabledByPolicy.
  kPromoCanceledByPolicy = 1,

  // Recorded when the promo is prevented from showing because the UI
  // is blocked.
  kPromoCanceledByUIBlocked = 2,

  // Recorded when the UI for the fullscreen sign-in promo is started.
  kPromoUIStarted = 3,

  // Recorded when the the sign-in UI is started.
  kSigninUIStarted = 4,

  // Recorded when the history-sync UI is started.
  kHistorySyncUIStarted = 5,

  // This value should always be the last entry.
  kMaxValue = kHistorySyncUIStarted,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ios/enums.xml:IOSSigninFullscreenPromoEvents)

// Called when the sign-in dialog is closed.
// `coordinator` the SigninCoordinator to which this block was assigned. nil if
// this block was not assigned to a signin coordinator. `result` is the sign-in
// result state. `signinCompletionIdentity` the identity that was used if any.
using SigninCoordinatorCompletionCallback =
    void (^)(SigninCoordinator* coordinator,
             SigninCoordinatorResult result,
             id<SystemIdentity> identity);

// Name of the accessibility identifier for the History Sync view.
extern NSString* const kHistorySyncViewAccessibilityIdentifier;
// Name of accessibility identifier for the consistency sign-in consistency
// sheet.
extern NSString* const kConsistencySigninAccessibilityIdentifier;
// Name of accessibility identifier for the primary button that signs in
// the user for the consistency sign-in consistency sheet.
extern NSString* const kConsistencySigninPrimaryButtonAccessibilityIdentifier;
// Name of accessibility identifier for "Skip" button in the consistency sign-in
// consistency sheet.
extern NSString* const kConsistencySigninSkipButtonAccessibilityIdentifier;
// Name of the accessibility identifier for the "add account" button in the
// consistency account chooser.
extern NSString* const kConsistencyAccountChooserAddAccountIdentifier;

// Name of the accessibility identifier for the managed profile creation screen.
extern NSString* const kManagedProfileCreationScreenAccessibilityIdentifier;

// Name of the accessibility identifier for the browsing data management screen.
extern NSString* const kBrowsingDataManagementScreenAccessibilityIdentifier;

// Name of the accessibility identifier for the navigation bar of the managed
// profile creation screen.
extern NSString* const
    kManagedProfileCreationNavigationBarAccessibilityIdentifier;

// Name of the accessibility identifier for the browsing data button on the
// managed profile creation screen.
extern NSString* const kBrowsingDataButtonAccessibilityIdentifier;

// Name of the accessibility identifier for the keep browsing data separate
// cell.
extern NSString* const kKeepBrowsingDataSeparateCellId;

// Name of the accessibility identifier for the merge browsing data cell.
extern NSString* const kMergeBrowsingDataCellId;

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
// Key in NSUserDefaults containing a boolean indicating whether the sign-in
// fullscreen promo migration to the promo manager has been completed.
// TODO(crbug.com/396111171): Post migration clean up.
extern NSString* const kFullscreenSigninPromoManagerMigrationDone;
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

// URL to the learn more screen about managed profiles.
extern NSString* const kManagedProfileLearnMoreURL;

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_CONSTANTS_H_
