// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/add_account_signin/add_account_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_promo_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/fullscreen_signin/coordinator/fullscreen_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/history_sync/history_sync_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/instant_signin/instant_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/interruptible_chrome_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_history_sync/signin_and_history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_screen_provider.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/stop_animated_chrome_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/trusted_vault_reauthentication/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/two_screens_signin/two_screens_signin_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@implementation SigninCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _contextStyle = contextStyle;
    _accessPoint = accessPoint;
    _creationTimeTicks = base::TimeTicks::Now();
  }
  return self;
}

+ (void)registerProfilePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // ConsistencyPromoSigninCoordinator.
  registry->RegisterIntegerPref(prefs::kSigninWebSignDismissalCount, 0);
  registry->RegisterDictionaryPref(prefs::kSigninHasAcceptedManagementDialog);
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
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
                                      continuationProvider {
  CHECK(continuationProvider);
  return [[InstantSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                        identity:identity
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                     promoAction:promoAction
            continuationProvider:continuationProvider];
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
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
                            changeProfileContinuationProvider {
  CHECK(changeProfileContinuationProvider);
  return [[FullscreenSigninCoordinator alloc]
             initWithBaseViewController:viewController
                                browser:browser
                         screenProvider:[[SigninScreenProvider alloc] init]
                           contextStyle:contextStyle
                            accessPoint:accessPoint
      changeProfileContinuationProvider:changeProfileContinuationProvider];
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
    upgradeSigninPromoCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                browser:(Browser*)browser
                                           contextStyle:
                                               (SigninContextStyle)contextStyle
                      changeProfileContinuationProvider:
                          (const ChangeProfileContinuationProvider&)
                              changeProfileContinuationProvider {
  CHECK(changeProfileContinuationProvider);
  AccessPoint accessPoint = AccessPoint::kSigninPromo;
  PromoAction promoAction = PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  return [[TwoScreensSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                     promoAction:promoAction
            continuationProvider:changeProfileContinuationProvider];
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
    addAccountCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                   contextStyle:(SigninContextStyle)contextStyle
                                    accessPoint:(AccessPoint)accessPoint
                           continuationProvider:
                               (const ChangeProfileContinuationProvider&)
                                   continuationProvider {
  CHECK(continuationProvider);
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                     promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
                    signinIntent:AddAccountSigninIntent::kAddAccount
            continuationProvider:continuationProvider];
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
    primaryAccountReauthCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                  browser:(Browser*)browser
                                             contextStyle:(SigninContextStyle)
                                                              contextStyle
                                              accessPoint:
                                                  (AccessPoint)accessPoint
                                              promoAction:
                                                  (PromoAction)promoAction
                                     continuationProvider:
                                         (const ChangeProfileContinuationProvider&)
                                             continuationProvider {
  CHECK(continuationProvider);
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                     promoAction:promoAction
                    signinIntent:AddAccountSigninIntent::kPrimaryAccountReauth
            continuationProvider:continuationProvider];
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
    signinAndSyncReauthCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                 browser:(Browser*)browser
                                            contextStyle:
                                                (SigninContextStyle)contextStyle
                                             accessPoint:
                                                 (AccessPoint)accessPoint
                                             promoAction:
                                                 (PromoAction)promoAction
                                    continuationProvider:
                                        (const ChangeProfileContinuationProvider&)
                                            continuationProvider {
  CHECK(continuationProvider);
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                     promoAction:promoAction
                    signinIntent:AddAccountSigninIntent::kResignin
            continuationProvider:continuationProvider];
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
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
                                                              accessPoint {
  DCHECK(!browser->GetProfile()->IsOffTheRecord());
  return [[TrustedVaultReauthenticationCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                          intent:intent
                securityDomainID:securityDomainID
                         trigger:trigger
                     accessPoint:accessPoint];
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
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
                                               continuationProvider {
  return [ConsistencyPromoSigninCoordinator
      coordinatorWithBaseViewController:viewController
                                browser:browser
                           contextStyle:contextStyle
                            accessPoint:accessPoint
                   prepareChangeProfile:prepareChangeProfile
                   continuationProvider:continuationProvider];
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
    signinAndHistorySyncCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                  browser:(Browser*)browser
                                             contextStyle:(SigninContextStyle)
                                                              contextStyle
                                              accessPoint:
                                                  (signin_metrics::AccessPoint)
                                                      accessPoint
                                              promoAction:
                                                  (PromoAction)promoAction
                                      optionalHistorySync:
                                          (BOOL)optionalHistorySync
                                          fullscreenPromo:(BOOL)fullscreenPromo
                                     continuationProvider:
                                         (const ChangeProfileContinuationProvider&)
                                             continuationProvider {
  CHECK(continuationProvider);
  return [[SignInAndHistorySyncCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                     promoAction:promoAction
             optionalHistorySync:optionalHistorySync
                 fullscreenPromo:fullscreenPromo
            continuationProvider:continuationProvider];
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
    accountMenuCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                         browser:(Browser*)browser
                                    contextStyle:
                                        (SigninContextStyle)contextStyle
                                      anchorView:(UIView*)anchorView
                                         fromWeb:(BOOL)fromWeb {
  return
      [[AccountMenuCoordinator alloc] initWithBaseViewController:viewController
                                                         browser:browser
                                                    contextStyle:contextStyle
                                                      anchorView:anchorView
                                                         fromWeb:fromWeb];
}

+ (SigninCoordinator<InterruptibleChromeCoordinator>*)
    historySyncCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                         browser:(Browser*)browser
                                    contextStyle:
                                        (SigninContextStyle)contextStyle
                                     accessPoint:(signin_metrics::AccessPoint)
                                                     accessPoint
                                     promoAction:(signin_metrics::PromoAction)
                                                     promoAction {
  return [[HistorySyncSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint];
}

- (void)dealloc {
  // Subclasses implementing `InterruptibleChromeCoordinator` must call
  // -[SigninCoordinator runCompletionWithSigninResult:completionIdentity:]
  // before the coordinator is deallocated.
  DCHECK(![self conformsToProtocol:@protocol(InterruptibleChromeCoordinator)] ||
         !self.signinCompletion)
      << base::SysNSStringToUTF8([self description]);
}

#pragma mark - SigninCoordinator

- (void)start {
  // `signinCompletion` needs to be set by the owner to know when the sign-in
  // is finished.
  DCHECK(self.signinCompletion);
}

- (void)stop {
  // All classes inheriting SigninCoordinator are currently being migrated from
  // InterruptibleChromeCoordinator. See crbug.com/c/381444097 for details. If
  // you are an user of a SigninCoordinator<StopAnimatedChromeCoordinator>
  // subclass, you can stop it by calling -stop or -stopAnimated. If you are an
  // user of a SigninCoordinator<InterruptibleChromeCoordinator> subclass: The
  // sign-in view is still presented. You should not call `stop`, if you need to
  // close the view. You need to call -[SigninCoordinator
  // interruptWithAction:completion:].
  // If you work on a SigninCoordinator<InterruptibleChromeCoordinator>
  // subclass:
  // -[SigninCoordinator
  // runCompletionWithSigninResult:completionIdentity:] has to be called
  // by the subclass before
  // -[SigninCoordinator stop] is called.
  if ([self conformsToProtocol:@protocol(StopAnimatedChromeCoordinator)]) {
    SigninCoordinator<StopAnimatedChromeCoordinator>* stopAnimatedSelf =
        base::apple::ObjCCast<SigninCoordinator<StopAnimatedChromeCoordinator>>(
            self);
    [stopAnimatedSelf stopAnimated:NO];
  } else {
    CHECK([self conformsToProtocol:@protocol(InterruptibleChromeCoordinator)]);
    DCHECK(!self.signinCompletion);
    [super stop];
  }
}

#pragma mark - Protected

// TODO(crbug.com/381444097): implements this protocol in the header file once
// each class inheriting SigninCoordinator implements this protocol.
- (void)stopAnimated:(BOOL)animated {
  [super stop];
}

- (void)runCompletionWithSigninResult:(SigninCoordinatorResult)signinResult
                   completionIdentity:(id<SystemIdentity>)completionIdentity {
  // `identity` is set, if and only if the sign-in is successful.
  DCHECK(
      ((signinResult == SigninCoordinatorResultSuccess ||
        signinResult == SigninCoordinatorProfileSwitch) &&
       completionIdentity) ||
      ((signinResult != SigninCoordinatorResultSuccess) && !completionIdentity))
      << "signinResult: " << signinResult
      << ", identity: " << (completionIdentity ? "YES" : "NO");
  // If `self.signinCompletion` is nil, this method has been probably called
  // twice.
  DCHECK(self.signinCompletion);
  SigninCoordinatorCompletionCallback signinCompletion = self.signinCompletion;
  // The owner should call the stop method, during the callback.
  // `self.signinCompletion` needs to be set to nil before calling it.
  self.signinCompletion = nil;
  signinCompletion(signinResult, completionIdentity);
}

@end
