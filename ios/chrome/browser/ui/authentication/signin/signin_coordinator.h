// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

class Browser;
namespace syncer {
enum class TrustedVaultUserActionTriggerForUMA;
}  // namespace syncer
namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs
@protocol SystemIdentity;

// Main class for sign-in coordinator. This class should not be instantiated
// directly, this should be done using the class methods.
@interface SigninCoordinator : ChromeCoordinator

// Called when the sign-in dialog is interrupted, canceled or successful.
// This completion needs to be set before calling -[SigninCoordinator start].
@property(nonatomic, copy) SigninCoordinatorCompletionCallback signinCompletion;

// Registers preferences related to sign-in coordinator.
+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Returns a coordinator for user sign-in workflow.
// `viewController` presents the sign-in.
// `identity` is the identity preselected with the sign-in opens.
// `accessPoint` is the view where the sign-in button was displayed.
// `promoAction` is promo button used to trigger the sign-in.
+ (instancetype)
    userSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                       identity:(id<SystemIdentity>)identity
                                    accessPoint:
                                        (signin_metrics::AccessPoint)accessPoint
                                    promoAction:(signin_metrics::PromoAction)
                                                    promoAction;

// Returns a coordinator for forced sign-in workflow.
// `viewController` presents the sign-in.
+ (instancetype)forcedSigninCoordinatorWithBaseViewController:
                    (UIViewController*)viewController
                                                      browser:(Browser*)browser;

// Returns a coordinator for upgrade sign-in workflow.
// `viewController` presents the sign-in.
+ (instancetype)upgradeSigninPromoCoordinatorWithBaseViewController:
                    (UIViewController*)viewController
                                                            browser:(Browser*)
                                                                        browser;

// Returns a coordinator for advanced sign-in settings workflow.
// `viewController` presents the sign-in.
// `signinState` defines the user's sign-in state prior to all SigninCoordinator
//               manipulations.
+ (instancetype)
    advancedSettingsSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                    browser:(Browser*)browser
                                                signinState:
                                                    (IdentitySigninState)
                                                        signinState;

// Returns a coordinator to add an account.
// `viewController` presents the sign-in.
// `accessPoint` access point from the sign-in where is started.
+ (instancetype)
    addAccountCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                    accessPoint:(signin_metrics::AccessPoint)
                                                    accessPoint;

// Returns a coordinator for re-authentication workflow.
// `viewController` presents the sign-in.
// `accessPoint` access point from the sign-in where is started.
// `promoAction` is promo button used to trigger the sign-in.
+ (instancetype)
    reAuthenticationCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                              browser:(Browser*)browser
                                          accessPoint:
                                              (signin_metrics::AccessPoint)
                                                  accessPoint
                                          promoAction:
                                              (signin_metrics::PromoAction)
                                                  promoAction;

// Returns a coordinator for re-authentication workflow for Trusted
// Vault for the primary identity. This is done with TrustedVaultService.
// Related to IOSTrustedVaultClient.
// `viewController` presents the sign-in.
// `intent` Dialog to present.
// `trigger` UI elements where the trusted vault reauth has been triggered.
+ (instancetype)
    trustedVaultReAuthenticationCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                          browser:
                                                              (Browser*)browser
                                                           intent:
                                                               (SigninTrustedVaultDialogIntent)
                                                                   intent
                                                          trigger:
                                                              (syncer::
                                                                   TrustedVaultUserActionTriggerForUMA)
                                                                  trigger;

// Returns a coordinator to display the account consistency promo with a list
// of accounts available on the device for sign-in.
// `viewController` presents the promo.
// This method can return nil if sign-in is not authorized or if there is no
// account on the device.
+ (instancetype)
    consistencyPromoSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                    browser:(Browser*)browser
                                                accessPoint:(signin_metrics::
                                                                 AccessPoint)
                                                                accessPoint;

// Interrupts the sign-in flow.
// `signinCompletion(SigninCoordinatorResultInterrupted, nil)` is guaranteed to
// be called before `completion()`.
// `action` action describing how to interrupt the sign-in.
// `completion` called once the sign-in is fully interrupted.
- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion;

// ChromeCoordinator.
- (void)start NS_REQUIRES_SUPER;
- (void)stop NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_
