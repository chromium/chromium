// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

#include "base/notreached.h"
#import "components/pref_registry/pref_registry_syncable.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/pref_names.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_promo_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/trusted_vault_reauthentication/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/logging/upgrade_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@implementation SigninCoordinator

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // ConsistencyPromoSigninCoordinator.
  registry->RegisterIntegerPref(prefs::kSigninBottomSheetShownCount, 0);
}

+ (instancetype)
    userSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                       identity:(ChromeIdentity*)identity
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

+ (instancetype)firstRunCoordinatorWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                                        browser:
                                                            (Browser*)browser {
  DCHECK(!base::FeatureList::IsEnabled(kEnableFREUIModuleIOS));
  UserSigninLogger* logger = [[FirstRunSigninLogger alloc]
        initWithAccessPoint:AccessPoint::ACCESS_POINT_START_PAGE
                promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
      accountManagerService:ChromeAccountManagerServiceFactory::
                                GetForBrowserState(browser->GetBrowserState())];
  return [[UserSigninCoordinator alloc]
      initWithBaseNavigationController:navigationController
                               browser:browser
                          signinIntent:UserSigninIntentFirstRun
                                logger:logger];
}

+ (instancetype)
    upgradeSigninPromoCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                browser:(Browser*)browser {
  UserSigninLogger* logger = [[UpgradeSigninLogger alloc]
        initWithAccessPoint:AccessPoint::ACCESS_POINT_SIGNIN_PROMO
                promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
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
                                                    browser:(Browser*)browser {
  return [[AdvancedSettingsSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser];
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
                    signinIntent:AddAccountSigninIntentAddSecondaryAccount];
}

+ (instancetype)
    reAuthenticationCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                              browser:(Browser*)browser
                                          accessPoint:(AccessPoint)accessPoint
                                          promoAction:(PromoAction)promoAction {
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint
                     promoAction:promoAction
                    signinIntent:AddAccountSigninIntentReauthPrimaryAccount];
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
  return [[TrustedVaultReauthenticationCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                          intent:intent
                         trigger:trigger];
}

+ (instancetype)
    consistencyPromoSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                    browser:(Browser*)browser {
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          browser->GetBrowserState());
  if (!accountManagerService->HasIdentities()) {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::SUPPRESSED_NO_ACCOUNTS);
    return nil;
  }
  PrefService* userPrefService = browser->GetBrowserState()->GetPrefs();
  if (!signin::IsSigninAllowed(userPrefService)) {
    RecordConsistencyPromoUserAction(
        signin_metrics::AccountConsistencyPromoAction::
            SUPPRESSED_SIGNIN_NOT_ALLOWED);
    return nil;
  }
  return [[ConsistencyPromoSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser];
}

- (void)dealloc {
  // -[SigninCoordinator runCompletionCallbackWithSigninResult:completionInfo:]
  // has to be called by the subclass before the coordinator is deallocated.
  DCHECK(!self.signinCompletion);
}

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  // This method needs to be implemented in the subclass.
  NOTREACHED();
}

#pragma mark - SigninCoordinator

- (void)start {
  // |signinCompletion| needs to be set by the owner to know when the sign-in
  // is finished.
  DCHECK(self.signinCompletion);
}

- (void)stop {
  // If you are an user of a SigninCoordinator subclass:
  // The sign-in view is still presented. You should not call |stop|, if you
  // need to close the view. You need to call -[SigninCoordinator
  // interruptWithAction:completion:].
  // If you work on a SigninCoordinator subclass:
  // -[SigninCoordinator runCompletionCallbackWithSigninResult:completionInfo:]
  // has to be called by the subclass before
  // -[SigninCoordinator stop] is called.
  DCHECK(!self.signinCompletion);
}

#pragma mark - Properties

- (BOOL)isSettingsViewPresented {
  return NO;
}

#pragma mark - Private

- (void)runCompletionCallbackWithSigninResult:
            (SigninCoordinatorResult)signinResult
                               completionInfo:
                                   (SigninCompletionInfo*)completionInfo {
  // |identity| is set, only and only if the sign-in is successful.
  DCHECK(((signinResult == SigninCoordinatorResultSuccess) &&
          completionInfo.identity) ||
         ((signinResult != SigninCoordinatorResultSuccess) &&
          !completionInfo.identity));
  // If |self.signinCompletion| is nil, this method has been probably called
  // twice.
  DCHECK(self.signinCompletion);
  SigninCoordinatorCompletionCallback signinCompletion = self.signinCompletion;
  // The owner should call the stop method, during the callback.
  // |self.signinCompletion| needs to be set to nil before calling it.
  self.signinCompletion = nil;
  signinCompletion(signinResult, completionInfo);
}

@end
