// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/change_profile_continuation_provider.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_context_style.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

enum class AccountMenuAccessPoint;
class Browser;
@protocol SystemIdentity;
namespace syncer {
enum class TrustedVaultUserActionTriggerForUMA;
}  // namespace syncer
namespace trusted_vault {
enum class SecurityDomainId;
}  // namespace trusted_vault
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs
@class ShowSigninCommand;

// Main class for sign-in coordinator. This class should not be instantiated
// directly, this should be done using the class methods.
@interface SigninCoordinator : AnimatedCoordinator

// Called when the sign-in dialog is interrupted, canceled or successful.
// This completion needs to be set before calling -[SigninCoordinator start].
@property(nonatomic, copy) SigninCoordinatorCompletionCallback signinCompletion;

// Customize content on sign-in and history sync screens.
@property(nonatomic, readonly) SigninContextStyle contextStyle;

// The access point which caused this coordinator to open.
// Used for histogram only.
@property(nonatomic, readonly) signin_metrics::AccessPoint accessPoint;

// TODO(crbug.com/40071586): Need to remove this property when the bug is
// closed.
// This property returns the time ticks when the instance was created.
@property(nonatomic, readonly, assign) base::TimeTicks creationTimeTicks;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Registers preferences related to sign-in coordinator.
+ (void)registerProfilePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Returns a coordinator according to the command
+ (SigninCoordinator*)signinCoordinatorWithCommand:(ShowSigninCommand*)command
                                           browser:(Browser*)browser
                                baseViewController:
                                    (UIViewController*)baseViewController;

// Returns a coordinator to sign-in the user without taps if the identity has
// been selected with `identity`. Otherwise, it will ask the user to select
// an identity, and starts the sign-in flow. If there is no identity on the
// device, the add account dialog will be displayed, and then the sign-in flow
// is started with the newly added identity.
+ (SigninCoordinator*)
    instantSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                           browser:(Browser*)browser
                                          identity:(id<SystemIdentity>)identity
                                      contextStyle:
                                          (SigninContextStyle)contextStyle
                                       accessPoint:(signin_metrics::AccessPoint)
                                                       accessPoint
                                       promoAction:(signin_metrics::PromoAction)
                                                       promoAction
                              continuationProvider:
                                  (const ChangeProfileContinuationProvider&)
                                      continuationProvider;

// Returns a coordinator for fullscreen sign-in workflow.
// `viewController` presents the sign-in.
+ (SigninCoordinator*)
    fullscreenSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                              browser:(Browser*)browser
                                         contextStyle:
                                             (SigninContextStyle)contextStyle
                                          accessPoint:
                                              (signin_metrics::AccessPoint)
                                                  accessPoint
                    changeProfileContinuationProvider:
                        (const ChangeProfileContinuationProvider&)
                            changeProfileContinuationProvider;

// Returns a coordinator for upgrade sign-in workflow.
// `viewController` presents the sign-in.
// `contextStyle` is used to customize content on screens.
+ (SigninCoordinator*)
    upgradeSigninPromoCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                browser:(Browser*)browser
                                           contextStyle:
                                               (SigninContextStyle)contextStyle
                      changeProfileContinuationProvider:
                          (const ChangeProfileContinuationProvider&)
                              changeProfileContinuationProvider;

// Returns a coordinator to add an account.
// `viewController` presents the sign-in.
// `contextStyle` is used to customize content on screens.
// `accessPoint` access point from the sign-in where is started.
+ (SigninCoordinator*)
    addAccountCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                   contextStyle:(SigninContextStyle)contextStyle
                                    accessPoint:
                                        (signin_metrics::AccessPoint)accessPoint
                           continuationProvider:
                               (const ChangeProfileContinuationProvider&)
                                   continuationProvider;

// Returns a coordinator for re-authentication workflow. This should only be
// called when the primary account is available.
// `viewController` presents the sign-in.
// `contextStyle` is used to customize content on screens.
// `accessPoint` access point from the sign-in where is started.
// `promoAction` is promo button used to trigger the sign-in.
+ (SigninCoordinator*)
    primaryAccountReauthCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                  browser:(Browser*)browser
                                             contextStyle:(SigninContextStyle)
                                                              contextStyle
                                              accessPoint:
                                                  (signin_metrics::AccessPoint)
                                                      accessPoint
                                              promoAction:
                                                  (signin_metrics::PromoAction)
                                                      promoAction
                                     continuationProvider:
                                         (const ChangeProfileContinuationProvider&)
                                             continuationProvider;

// Returns a coordinator for re-authentication workflow. This should only be
// called when there is no primary account.
// `viewController` presents the sign-in.
// `contextStyle` is used to customize content on screens.
// `accessPoint` access point from the sign-in where is started.
// `promoAction` is promo button used to trigger the sign-in.
+ (SigninCoordinator*)
    signinAndSyncReauthCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                 browser:(Browser*)browser
                                            contextStyle:
                                                (SigninContextStyle)contextStyle
                                             accessPoint:
                                                 (signin_metrics::AccessPoint)
                                                     accessPoint
                                             promoAction:
                                                 (signin_metrics::PromoAction)
                                                     promoAction
                                    continuationProvider:
                                        (const ChangeProfileContinuationProvider&)
                                            continuationProvider;

// Returns a coordinator to display the account consistency promo with a list
// of accounts available on the device for sign-in.
// `viewController` presents the promo.
// This method can return nil if sign-in is not authorized or if there is no
// account on the device.
+ (SigninCoordinator*)
    consistencyPromoSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                    browser:(Browser*)browser
                                               contextStyle:(SigninContextStyle)
                                                                contextStyle
                                                accessPoint:
                                                    (signin_metrics::
                                                         AccessPoint)accessPoint
                                       prepareChangeProfile:
                                           (ProceduralBlock)prepareChangeProfile
                                       continuationProvider:
                                           (const ChangeProfileContinuationProvider&)
                                               continuationProvider;

// Returns a coordinator to display the sign-in view then the history opt-in
// with  with its base `viewController`, the `browser`, from which `accessPoint`
// the sign in flow was initialized, using which `promoAction` (when relevant),
// when `optionalHistorySync` is YES, the history sync opt in will be presented
// if the user hasn't already approved it.
// `fullscreenPromo`: whether the promo should be displayed in a fullscreen
// modal.
+ (SigninCoordinator*)
    signinAndHistorySyncCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                  browser:(Browser*)browser
                                             contextStyle:(SigninContextStyle)
                                                              contextStyle
                                              accessPoint:
                                                  (signin_metrics::AccessPoint)
                                                      accessPoint
                                              promoAction:
                                                  (signin_metrics::PromoAction)
                                                      promoAction
                                      optionalHistorySync:
                                          (BOOL)optionalHistorySync
                                          fullscreenPromo:(BOOL)fullscreenPromo
                                     continuationProvider:
                                         (const ChangeProfileContinuationProvider&)
                                             continuationProvider;

// Returns a coordinator to show the history sync.
+ (SigninCoordinator*)
    historySyncCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                         browser:(Browser*)browser
                                    contextStyle:
                                        (SigninContextStyle)contextStyle
                                     accessPoint:(signin_metrics::AccessPoint)
                                                     accessPoint
                                     promoAction:(signin_metrics::PromoAction)
                                                     promoAction;

// ChromeCoordinator.
- (void)start NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_SIGNIN_SIGNIN_COORDINATOR_H_
