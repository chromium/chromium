// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/first_run/ui_bundled/interruptible_chrome_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_completion_info.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

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

// Main class for sign-in coordinator. This class should not be instantiated
// directly, this should be done using the class methods.
@interface SigninCoordinator : InterruptibleChromeCoordinator

// Called when the sign-in dialog is interrupted, canceled or successful.
// This completion needs to be set before calling -[SigninCoordinator start].
@property(nonatomic, copy) SigninCoordinatorCompletionCallback signinCompletion;

// The access point which caused this coordinator to open.
// Used for histogram only.
@property(nonatomic, readonly) signin_metrics::AccessPoint accessPoint;

// TODO(crbug.com/40071586): Need to remove this property when the bug is
// closed.
// This property returns the time ticks when the instance was created.
@property(nonatomic, readonly, assign) base::TimeTicks creationTimeTicks;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Registers preferences related to sign-in coordinator.
+ (void)registerProfilePrefs:(user_prefs::PrefRegistrySyncable*)registry;

// Returns a coordinator to sign-in the user without taps if the identity has
// been selected with `identity`. Otherwise, it will ask the user to select
// an identity, and starts the sign-in flow. If there is no identity on the
// device, the add account dialog will be displayed, and then the sign-in flow
// is started with the newly added identity.
+ (instancetype)
    instantSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                           browser:(Browser*)browser
                                          identity:(id<SystemIdentity>)identity
                                       accessPoint:(signin_metrics::AccessPoint)
                                                       accessPoint
                                       promoAction:(signin_metrics::PromoAction)
                                                       promoAction;

// Returns a coordinator for forced sign-in workflow.
// `viewController` presents the sign-in.
+ (instancetype)
    forcedSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                          browser:(Browser*)browser
                                      accessPoint:(signin_metrics::AccessPoint)
                                                      accessPoint;

// Returns a coordinator for upgrade sign-in workflow.
// `viewController` presents the sign-in.
+ (instancetype)upgradeSigninPromoCoordinatorWithBaseViewController:
                    (UIViewController*)viewController
                                                            browser:(Browser*)
                                                                        browser;

// Returns a coordinator to add an account.
// `viewController` presents the sign-in.
// `accessPoint` access point from the sign-in where is started.
+ (instancetype)
    addAccountCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                    accessPoint:(signin_metrics::AccessPoint)
                                                    accessPoint;

// Returns a coordinator for re-authentication workflow. This should only be
// called when the primary account is available.
// `viewController` presents the sign-in.
// `accessPoint` access point from the sign-in where is started.
// `promoAction` is promo button used to trigger the sign-in.
+ (instancetype)
    primaryAccountReauthCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                  browser:(Browser*)browser
                                              accessPoint:
                                                  (signin_metrics::AccessPoint)
                                                      accessPoint
                                              promoAction:
                                                  (signin_metrics::PromoAction)
                                                      promoAction;

// Returns a coordinator for re-authentication workflow. This should only be
// called when there is no primary account.
// `viewController` presents the sign-in.
// `accessPoint` access point from the sign-in where is started.
// `promoAction` is promo button used to trigger the sign-in.
+ (instancetype)
    signinAndSyncReauthCoordinatorWithBaseViewController:
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
// `securityDomainID` Identifies a particular security domain.
// `trigger` UI elements where the trusted vault reauth has been triggered.
// `accessPoint` Identifies where the dialog is initiated from.
+ (instancetype)
    trustedVaultReAuthenticationCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                          browser:
                                                              (Browser*)browser
                                                           intent:
                                                               (SigninTrustedVaultDialogIntent)
                                                                   intent
                                                 securityDomainID:
                                                     (trusted_vault::
                                                          SecurityDomainId)
                                                         securityDomainID
                                                          trigger:
                                                              (syncer::
                                                                   TrustedVaultUserActionTriggerForUMA)
                                                                  trigger
                                                      accessPoint:
                                                          (signin_metrics::
                                                               AccessPoint)
                                                              accessPoint;

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

// Returns a coordinator to display the sign-in view then the history opt-in.
+ (instancetype)
    sheetSigninAndHistorySyncCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                       browser:(Browser*)browser
                                                   accessPoint:(signin_metrics::
                                                                    AccessPoint)
                                                                   accessPoint
                                                   promoAction:(signin_metrics::
                                                                    PromoAction)
                                                                   promoAction;

// Interrupts the sign-in flow.
// `signinCompletion(SigninCoordinatorResultInterrupted, nil)` is guaranteed to
// be called before `completion()`.
// When the coordinator is interrupted with `UIShutdownNoDismiss` action, both
// `signinCompletion()` and `completion()` are called synchronously in this
// order.
// When the coordinator is interrupted with `DismissWithoutAnimation` or
// `DismissWithAnimation`, the view is dismissed first. After being dismissed,
// `signinCompletion()` is called, and then `completion()` is called.
//
// It is still mandatory to call `-[SigninCoordinator stop]` once
// `signinCompletion()` is called.
- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion;

// ChromeCoordinator.
- (void)start NS_REQUIRES_SUPER;
- (void)stop NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_SIGNIN_COORDINATOR_H_
