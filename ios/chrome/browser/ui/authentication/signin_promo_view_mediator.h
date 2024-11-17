// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "components/sync/base/data_type.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_delegate.h"

class AuthenticationService;
class ChromeAccountManagerService;
class PrefService;
@protocol SigninPresenter;
@protocol AccountSettingsPresenter;
@class SigninPromoViewConfigurator;
@protocol SigninPromoViewConsumer;
@protocol SystemIdentity;

namespace signin_metrics {
enum class AccessPoint;
}

namespace syncer {
class SyncService;
}

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

// Enums for the sign-in promo view state. Those states are sequential, with no
// way to go backwards. All states can be skipped except `NeverVisible` and
// `Invalid`.
enum class SigninPromoViewState {
  // Initial state. When -[SigninPromoViewMediator disconnect] is called with
  // that state, no metrics is recorded.
  kNeverVisible,
  // None of the buttons has been used yet.
  kUnused,
  // Sign-in buttons have been used at least once.
  kUsedAtLeastOnce,
  // Sign-in promo has been closed.
  kClosed,
  // Sign-in promo view has been removed.
  kInvalid,
};

// The action performed when accepting the promo.
enum class SigninPromoAction {
  // Primary button signs the user in instantly.
  // Secondary button opens a floating dialog with the available accounts. When
  // an account is tapped, it is signed in instantly.
  kInstantSignin,
  // Single button. If there is an account, ask which account to use. Otherwise,
  // add the add account dialog, and then sign-in directly.
  kSigninWithNoDefaultIdentity,
  // Performs AuthenticationOperationSigninOnly.
  kSigninSheet,
  // Shows account settings.
  kReviewAccountSettings,
};

// Class that monitors the available identities and creates
// SigninPromoViewConfigurator. This class makes the link between the model and
// the view. The consumer will receive notification if default identity is
// changed or updated.
// TODO(crbug.com/40898970): This class needs to be split with a coordinator.
@interface SigninPromoViewMediator : NSObject<SigninPromoViewDelegate>

// Consumer to handle identity update notifications.
@property(nonatomic, weak) id<SigninPromoViewConsumer> consumer;

// The identity whose avatar is shown by the promo and which will be signed-in
// if the user taps the primary button.
// When the user is signed-out with no accounts on the device, this is nil
// (in that case the button opens a dialog to add an account instead).
// When the user is signed-out and has accounts on the device, this is the
// default identity.
// when the user is signed-in and not syncing, this is the signed-in identity
// (not necessarily the default one).
@property(nonatomic, strong, readonly) id<SystemIdentity> displayedIdentity;

// Sign-in promo view state. kNeverVisible by default.
@property(nonatomic, assign) SigninPromoViewState signinPromoViewState;

// YES if the promo spinner should be displayed. Either the sign-in or the
// initial sync is in progress.
@property(nonatomic, assign, readonly) BOOL showSpinner;

// Returns YES if the sign-in promo view is `Invalid`, `Closed` or invisible.
@property(nonatomic, assign, readonly, getter=isInvalidClosedOrNeverVisible)
    BOOL invalidClosedOrNeverVisible;

// The action performed when accepting the promo. kInstantSignin by default.
@property(nonatomic, assign) SigninPromoAction signinPromoAction;

// Set the data type that should be synced before the sign-in completes.
// The default value is `syncer::DataType::UNSPECIFIED`, therefore the sign-in
// promo will not wait for the initial sync.
// This value has to be set while the mediator is being set (right after the
// init method).
@property(nonatomic, assign) syncer::DataType dataTypeToWaitForInitialSync;

// Registers the feature preferences.
+ (void)registerProfilePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Tests if the sign-in promo view should be displayed according to the number
// of times it has been displayed and if the user closed the sign-in promo view.
+ (BOOL)shouldDisplaySigninPromoViewWithAccessPoint:
            (signin_metrics::AccessPoint)accessPoint
                                  signinPromoAction:
                                      (SigninPromoAction)signinPromoAction
                              authenticationService:
                                  (AuthenticationService*)authenticationService
                                        prefService:(PrefService*)prefService;

// See `-[SigninPromoViewMediator initWithBrowser:accountManagerService:
// authService:prefService:accessPointpresenter:baseViewController]`.
- (instancetype)init NS_UNAVAILABLE;

// Designated initializer.
// `baseViewController` is the view to present UI for sign-in.
- (instancetype)
    initWithAccountManagerService:
        (ChromeAccountManagerService*)accountManagerService
                      authService:(AuthenticationService*)authService
                      prefService:(PrefService*)prefService
                      syncService:(syncer::SyncService*)syncService
                      accessPoint:(signin_metrics::AccessPoint)accessPoint
                  signinPresenter:(id<SigninPresenter>)signinPresenter
         accountSettingsPresenter:
             (id<AccountSettingsPresenter>)accountSettingsPresenter
    NS_DESIGNATED_INITIALIZER;

- (SigninPromoViewConfigurator*)createConfigurator;

// Increments the "shown" counter used for histograms. Called when the signin
// promo view is visible. If the sign-in promo is already visible, this method
// does nothing.
- (void)signinPromoViewIsVisible;

// Called when the sign-in promo view is hidden. If the sign-in promo view has
// never been shown, or it is already hidden, this method does nothing.
- (void)signinPromoViewIsHidden;

// Disconnects the mediator, this method needs to be called when the sign-in
// promo view is removed from the view hierarchy (it or one of its superviews is
// removed). The mediator should not be used after this called.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_PROMO_VIEW_MEDIATOR_H_
