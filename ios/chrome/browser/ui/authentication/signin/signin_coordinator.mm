// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

#import "base/feature_list.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/forced_signin/forced_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/instant_signin/instant_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_history_sync/signin_and_history_sync_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_screen_provider.h"
#import "ios/chrome/browser/ui/authentication/signin/trusted_vault_reauthentication/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/two_screens_signin/two_screens_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/upgrade_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/user_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@implementation SigninCoordinator

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // ConsistencyPromoSigninCoordinator.
  registry->RegisterIntegerPref(prefs::kSigninWebSignDismissalCount, 0);
}

+ (instancetype)
    userSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                       identity:(id<SystemIdentity>)identity
                                    accessPoint:(AccessPoint)accessPoint
                                    promoAction:(PromoAction)promoAction {
  UserSigninLogger* logger = [[UserSigninLogger alloc]
        initWithAccessPoint:accessPoint
                promoAction:promoAction
      accountManagerService:ChromeAccountManagerServiceFactory::
                                GetForBrowserState(browser->GetBrowserState())];
  return [[UserSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                        identity:identity
                    signinIntent:UserSigninIntentSignin
                          logger:logger];
}

+ (instancetype)
    instantSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                           browser:(Browser*)browser
                                          identity:(id<SystemIdentity>)identity
                                       accessPoint:(signin_metrics::AccessPoint)
                                                       accessPoint
                                       promoAction:(signin_metrics::PromoAction)
                                                       promoAction {
  return [[InstantSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                        identity:identity
                     accessPoint:accessPoint
                     promoAction:promoAction];
}

+ (instancetype)forcedSigninCoordinatorWithBaseViewController:
                    (UIViewController*)viewController
                                                      browser:
                                                          (Browser*)browser {
  return [[ForcedSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                  screenProvider:[[SigninScreenProvider alloc] init]];
}

+ (instancetype)
    twoScreensSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                              browser:(Browser*)browser
                                          accessPoint:(AccessPoint)accessPoint
                                          promoAction:(PromoAction)promoAction {
  return [[TwoScreensSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint
                     promoAction:promoAction];
}

+ (instancetype)
    upgradeSigninPromoCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                browser:(Browser*)browser {
  AccessPoint accessPoint = AccessPoint::ACCESS_POINT_SIGNIN_PROMO;
  PromoAction promoAction = PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return [[TwoScreensSigninCoordinator alloc]
        initWithBaseViewController:viewController
                           browser:browser
                       accessPoint:accessPoint
                       promoAction:promoAction];
  }
  UserSigninLogger* logger = [[UpgradeSigninLogger alloc]
        initWithAccessPoint:accessPoint
                promoAction:promoAction
      accountManagerService:ChromeAccountManagerServiceFactory::
                                GetForBrowserState(browser->GetBrowserState())];
  return [[UserSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                        identity:nil
                    signinIntent:UserSigninIntentUpgrade
                          logger:logger];
}

+ (instancetype)
    advancedSettingsSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                    browser:(Browser*)browser
                                                signinState:
                                                    (IdentitySigninState)
                                                        signinState {
  return [[AdvancedSettingsSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     signinState:signinState];
}

+ (instancetype)addAccountCoordinatorWithBaseViewController:
                    (UIViewController*)viewController
                                                    browser:(Browser*)browser
                                                accessPoint:
                                                    (AccessPoint)accessPoint {
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint
                     promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
                    signinIntent:AddAccountSigninIntent::kAddAccount];
}

+ (instancetype)
    primaryAccountReauthCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                  browser:(Browser*)browser
                                              accessPoint:
                                                  (AccessPoint)accessPoint
                                              promoAction:
                                                  (PromoAction)promoAction {
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint
                     promoAction:promoAction
                    signinIntent:AddAccountSigninIntent::kPrimaryAccountReauth];
}

+ (instancetype)
    signinAndSyncReauthCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                 browser:(Browser*)browser
                                             accessPoint:
                                                 (AccessPoint)accessPoint
                                             promoAction:
                                                 (PromoAction)promoAction {
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint
                     promoAction:promoAction
                    signinIntent:AddAccountSigninIntent::kSigninAndSyncReauth];
}

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
                                                                  trigger {
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  return [[TrustedVaultReauthenticationCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                          intent:intent
                         trigger:trigger];
}

+ (instancetype)
    consistencyPromoSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                    browser:(Browser*)browser
                                                accessPoint:(signin_metrics::
                                                                 AccessPoint)
                                                                accessPoint {
  return [ConsistencyPromoSigninCoordinator
      coordinatorWithBaseViewController:viewController
                                browser:browser
                            accessPoint:accessPoint];
}

+ (instancetype)
    sheetSigninAndHistorySyncCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                       browser:(Browser*)browser
                                                   accessPoint:(signin_metrics::
                                                                    AccessPoint)
                                                                   accessPoint
                                                   promoAction:(PromoAction)
                                                                   promoAction {
  return [[SignInAndHistorySyncCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint
                     promoAction:promoAction];
}

- (void)dealloc {
  // -[SigninCoordinator runCompletionCallbackWithSigninResult:completionInfo:]
  // has to be called by the subclass before the coordinator is deallocated.
  DCHECK(!self.signinCompletion) << base::SysNSStringToUTF8([self description]);
}

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
}

#pragma mark - SigninCoordinator

- (void)start {
  // `signinCompletion` needs to be set by the owner to know when the sign-in
  // is finished.
  DCHECK(self.signinCompletion);
}

- (void)stop {
  // If you are an user of a SigninCoordinator subclass:
  // The sign-in view is still presented. You should not call `stop`, if you
  // need to close the view. You need to call -[SigninCoordinator
  // interruptWithAction:completion:].
  // If you work on a SigninCoordinator subclass:
  // -[SigninCoordinator runCompletionCallbackWithSigninResult:completionInfo:]
  // has to be called by the subclass before
  // -[SigninCoordinator stop] is called.
  DCHECK(!self.signinCompletion);
}

#pragma mark - Private

- (void)runCompletionCallbackWithSigninResult:
            (SigninCoordinatorResult)signinResult
                               completionInfo:
                                   (SigninCompletionInfo*)completionInfo {
  // `identity` is set, if and only if the sign-in is successful.
  DCHECK(((signinResult == SigninCoordinatorResultSuccess) &&
          completionInfo.identity) ||
         ((signinResult != SigninCoordinatorResultSuccess) &&
          !completionInfo.identity))
      << "signinResult: " << signinResult
      << ", identity: " << (completionInfo.identity ? "YES" : "NO");
  // If `self.signinCompletion` is nil, this method has been probably called
  // twice.
  DCHECK(self.signinCompletion);
  SigninCoordinatorCompletionCallback signinCompletion = self.signinCompletion;
  // The owner should call the stop method, during the callback.
  // `self.signinCompletion` needs to be set to nil before calling it.
  self.signinCompletion = nil;
  signinCompletion(signinResult, completionInfo);
}

@end
