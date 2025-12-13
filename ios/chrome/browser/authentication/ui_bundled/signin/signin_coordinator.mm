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
#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_coordinator.h"
#import "ios/chrome/browser/authentication/consistency_promo_signin/coordinator/consistency_promo_signin_coordinator.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/history_sync/history_sync_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/instant_signin/instant_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/logging/first_run_signin_logger.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_history_sync/signin_and_history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_in_progress.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_screen_provider.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/two_screens_signin/two_screens_signin_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@implementation SigninCoordinator {
  std::unique_ptr<SigninInProgress> _signinInProgress;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              contextStyle:(SigninContextStyle)contextStyle
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK(browser);
    CHECK_EQ(browser->type(), Browser::Type::kRegular,
             base::NotFatalUntil::M145);
    auto* authService =
        AuthenticationServiceFactory::GetForProfile(self.profile);
    CHECK(authService->SigninEnabled(), base::NotFatalUntil::M146);
    _contextStyle = contextStyle;
    _accessPoint = accessPoint;
    _creationTimeTicks = base::TimeTicks::Now();
    _signinInProgress = [self.sceneState createSigninInProgress];
  }
  return self;
}

+ (void)registerProfilePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  // ConsistencyPromoSigninCoordinator.
  registry->RegisterIntegerPref(prefs::kSigninWebSignDismissalCount, 0);
  registry->RegisterDictionaryPref(prefs::kSigninHasAcceptedManagementDialog);
}

+ (SigninCoordinator*)signinCoordinatorWithCommand:(ShowSigninCommand*)command
                                           browser:(Browser*)browser
                                baseViewController:
                                    (UIViewController*)baseViewController {
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(browser->GetProfile());
  CHECK(authenticationService->SigninEnabled(), base::NotFatalUntil::M146);
  SigninCoordinator* signinCoordinator;
  switch (command.operation) {
    case AuthenticationOperation::kResignin: {
      signinCoordinator = [SigninCoordinator
          signinAndSyncReauthCoordinatorWithBaseViewController:
              baseViewController
                                                       browser:browser
                                                  contextStyle:command
                                                                   .contextStyle
                                                   accessPoint:command
                                                                   .accessPoint
                                                   promoAction:command
                                                                   .promoAction
                                          continuationProvider:
                                              command
                                                  .changeProfileContinuationProvider];
      break;
    }
    case AuthenticationOperation::kSigninOnly: {
      auto& provider = command.changeProfileContinuationProvider;
      signinCoordinator = [SigninCoordinator
          consistencyPromoSigninCoordinatorWithBaseViewController:
              baseViewController
                                                          browser:browser
                                                     contextStyle:
                                                         command.contextStyle
                                                      accessPoint:
                                                          command.accessPoint
                                             prepareChangeProfile:
                                                 command.prepareChangeProfile
                                             continuationProvider:provider];
      break;
    }
    case AuthenticationOperation::kInstantSignin: {
      signinCoordinator = [SigninCoordinator
          instantSigninCoordinatorWithBaseViewController:baseViewController
                                                 browser:browser
                                                identity:command.identity
                                            contextStyle:command.contextStyle
                                             accessPoint:command.accessPoint
                                             promoAction:command.promoAction
                                    continuationProvider:
                                        command
                                            .changeProfileContinuationProvider];
      break;
    }
    case AuthenticationOperation::kSheetSigninAndHistorySync: {
      auto& provider = command.changeProfileContinuationProvider;

      signinCoordinator = [SigninCoordinator
          signinAndHistorySyncCoordinatorWithBaseViewController:
              baseViewController
                                                        browser:browser
                                                   contextStyle:
                                                       command.contextStyle
                                                    accessPoint:command
                                                                    .accessPoint
                                                    promoAction:command
                                                                    .promoAction
                                            optionalHistorySync:
                                                command.optionalHistorySync
                                                fullscreenPromo:
                                                    command.fullScreenPromo
                                           continuationProvider:provider];
      break;
    }
    case AuthenticationOperation::kHistorySync: {
      signinCoordinator = [SigninCoordinator
          historySyncCoordinatorWithBaseViewController:baseViewController
                                               browser:browser
                                          contextStyle:command.contextStyle
                                           accessPoint:command.accessPoint
                                           promoAction:command.promoAction
                                          showSnackbar:command.showSnackbar];
      break;
    }
  }
  signinCoordinator.signinCompletion = command.completion;
  return signinCoordinator;
}

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

+ (SigninCoordinator*)
    fullscreenSigninPromoCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                   browser:(Browser*)browser
                                              contextStyle:(SigninContextStyle)
                                                               contextStyle
                         changeProfileContinuationProvider:
                             (const ChangeProfileContinuationProvider&)
                                 changeProfileContinuationProvider {
  CHECK(changeProfileContinuationProvider);
  AccessPoint accessPoint = AccessPoint::kFullscreenSigninPromo;
  PromoAction promoAction = PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  return [[TwoScreensSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                     promoAction:promoAction
            continuationProvider:changeProfileContinuationProvider];
}

+ (SigninCoordinator*)
    addAccountCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                   contextStyle:(SigninContextStyle)contextStyle
                                    accessPoint:(AccessPoint)accessPoint
                                 prefilledEmail:(NSString*)email
                           continuationProvider:
                               (const ChangeProfileContinuationProvider&)
                                   continuationProvider {
  CHECK(viewController, base::NotFatalUntil::M140);
  CHECK(browser, base::NotFatalUntil::M140);
  CHECK(continuationProvider);
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                     promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
                    signinIntent:AddAccountSigninIntent::kAddAccount
                  prefilledEmail:email
            continuationProvider:continuationProvider];
}

+ (SigninCoordinator*)
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
  CHECK(viewController, base::NotFatalUntil::M140);
  CHECK(browser, base::NotFatalUntil::M140);
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                     promoAction:promoAction
                    signinIntent:AddAccountSigninIntent::kPrimaryAccountReauth
                  prefilledEmail:nil
            continuationProvider:continuationProvider];
}

+ (SigninCoordinator*)
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
  CHECK(viewController, base::NotFatalUntil::M140);
  CHECK(browser, base::NotFatalUntil::M140);
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                     promoAction:promoAction
                    signinIntent:AddAccountSigninIntent::kResignin
                  prefilledEmail:nil
            continuationProvider:continuationProvider];
}

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
                                               continuationProvider {
  return [ConsistencyPromoSigninCoordinator
      coordinatorWithBaseViewController:viewController
                                browser:browser
                           contextStyle:contextStyle
                            accessPoint:accessPoint
                   prepareChangeProfile:prepareChangeProfile
                   continuationProvider:continuationProvider];
}

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

+ (SigninCoordinator*)
    historySyncCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                         browser:(Browser*)browser
                                    contextStyle:
                                        (SigninContextStyle)contextStyle
                                     accessPoint:(signin_metrics::AccessPoint)
                                                     accessPoint
                                     promoAction:(signin_metrics::PromoAction)
                                                     promoAction
                                    showSnackbar:(BOOL)showSnackbar {
  return [[HistorySyncSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                    contextStyle:contextStyle
                     accessPoint:accessPoint
                    showSnackbar:showSnackbar];
}

#pragma mark - SigninCoordinator

- (void)start {
  // `signinCompletion` needs to be set by the owner to know when the sign-in
  // is finished.
  CHECK(self.signinCompletion, base::NotFatalUntil::M151);
}

#pragma mark - AnimatedCoordinator

- (void)stopAnimated:(BOOL)animated {
  _signinInProgress.reset();
  [super stopAnimated:animated];
}

#pragma mark - Protected

- (void)runCompletionWithSigninResult:(SigninCoordinatorResult)signinResult
                   completionIdentity:(id<SystemIdentity>)completionIdentity {
  // `identity` is set, if and only if the sign-in is successful.
  CHECK(((signinResult == SigninCoordinatorResultSuccess ||
          signinResult == SigninCoordinatorProfileSwitch) &&
         completionIdentity) ||
            ((signinResult != SigninCoordinatorResultSuccess) &&
             !completionIdentity),
        base::NotFatalUntil::M151)
      << "signinResult: " << signinResult
      << ", identity: " << (completionIdentity ? "YES" : "NO");
  // If `self.signinCompletion` is nil, this method has been probably called
  // twice.
  CHECK(self.signinCompletion, base::NotFatalUntil::M151);
  SigninCoordinatorCompletionCallback signinCompletion = self.signinCompletion;
  // The owner should call the stop method, during the callback.
  // `self.signinCompletion` needs to be set to nil before calling it.
  self.signinCompletion = nil;
  signinCompletion(self, signinResult, completionIdentity);
}

#pragma mark - BuggyAuthenticationViewOwner

- (BOOL)viewWillPersist {
  // Subclasses must implement this property. See the description in the header
  // file for its implementation.
  NOTREACHED();
}

@end
