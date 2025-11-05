// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/two_screens_signin/two_screens_signin_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_coordinator.h"
#import "ios/chrome/browser/authentication/history_sync/coordinator/history_sync_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/logging/fullscreen_signin_promo_logger.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/uno_signin_screen_provider.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

using base::RecordAction;
using base::UserMetricsAction;

@interface TwoScreensSigninCoordinator () <
    HistorySyncCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation TwoScreensSigninCoordinator {
  signin_metrics::PromoAction _promoAction;

  // This can be either the FullscreenSigninScreenCoordinator or the
  // HistorySyncCoordinator depending on which step the user is on.
  ChromeCoordinator* _childCoordinator;

  // The navigation controller used to present the views.
  UINavigationController* _navigationController;

  // The screen provider that specifies which screens to present.
  ScreenProvider* _screenProvider;

  // The signin logger for the upgrade screen.
  FullscreenSigninPromoLogger* _fullscreenSigninPromoLogger;

  ChangeProfileContinuationProvider _continuationProvider;

  // The current screen type.
  ScreenType _currentScreenType;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                  contextStyle:(SigninContextStyle)contextStyle
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
                   promoAction:(signin_metrics::PromoAction)promoAction
          continuationProvider:
              (const ChangeProfileContinuationProvider&)continuationProvider {
  DCHECK_EQ(browser->type(), Browser::Type::kRegular);
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                              contextStyle:contextStyle
                               accessPoint:accessPoint];
  if (self) {
    CHECK(continuationProvider);
    _continuationProvider = continuationProvider;
    // This coordinator should not be used in the FRE.
    CHECK_NE(accessPoint, signin_metrics::AccessPoint::kStartPage);
    _promoAction = promoAction;
    signin::IdentityManager* identityManager =
        IdentityManagerFactory::GetForProfile(self.profile);
    ChromeAccountManagerService* accountManagerService =
        ChromeAccountManagerServiceFactory::GetForProfile(self.profile);
    _fullscreenSigninPromoLogger = [[FullscreenSigninPromoLogger alloc]
          initWithAccessPoint:accessPoint
                  promoAction:promoAction
              identityManager:identityManager
        accountManagerService:accountManagerService];
  }
  return self;
}

- (void)dealloc {
  CHECK(!_fullscreenSigninPromoLogger, base::NotFatalUntil::M146);
}

#pragma mark - BuggyAuthenticationViewOwner

- (BOOL)viewWillPersist {
  return YES;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  if (self.accessPoint == signin_metrics::AccessPoint::kFullscreenSigninPromo) {
    // TODO(crbug.com/41352590): Need to add `CHECK(accountManagerService)`.
    [_fullscreenSigninPromoLogger logSigninStarted];
  }
  _screenProvider = [[UnoSigninScreenProvider alloc] init];
  _navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;

  // Retain `self` in case `presentScreenIfNeeded` executes the
  // signinCompletion, which would cause self’s owner to unassign its variable.
  __typeof(self) strongSelf = self;
  [self presentScreenIfNeeded:[_screenProvider nextScreenType]];

  // Check if the flow is already completed (kStepsCompleted) to prevent
  // presenting a nil navigation controller.
  if (strongSelf->_currentScreenType == kStepsCompleted) {
    return;
  }

  // Set the presentation delegate after the child coordinator creation to
  // override the default implementation.
  _navigationController.presentationController.delegate = self;

  [_navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - AnimatedCoordinator

- (void)stopAnimated:(BOOL)animated {
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:nil];
  [self finishWithResult:SigninCoordinatorResultInterrupted identity:nil];
  [_fullscreenSigninPromoLogger disconnect];
  _fullscreenSigninPromoLogger = nil;
  DCHECK(!_navigationController);
  DCHECK(!_childCoordinator);
  DCHECK(!_screenProvider);
  [super stopAnimated:animated];
}

#pragma mark - Private

- (void)stopChildCoordinator {
  [_childCoordinator stop];
  _childCoordinator = nil;
}

// Dismiss the main navigation view controller with an animation and run the
// sign-in completion callback on completion of the animation to finish
// presenting the screens.
- (void)finishPresentingScreens {
  __weak __typeof(self) weakSelf = self;
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  [_navigationController dismissViewControllerAnimated:YES completion:nil];
  SigninCoordinatorResult result = identity
                                       ? SigninCoordinatorResultSuccess
                                       : SigninCoordinatorResultCanceledByUser;
  [weakSelf finishWithResult:result identity:identity];
  [weakSelf runCompletionWithSigninResult:result completionIdentity:identity];
}

// Presents the screen of certain `type`.
- (void)presentScreenIfNeeded:(ScreenType)type {
  _currentScreenType = type;
  // If there are no screens remaining, call delegate to stop presenting
  // screens.
  if (type == kStepsCompleted) {
    [self finishPresentingScreens];
    return;
  }
  _childCoordinator = [self createChildCoordinatorWithScreenType:type];
  [_childCoordinator start];
}

// Creates a screen coordinator according to `type`.
- (ChromeCoordinator*)createChildCoordinatorWithScreenType:(ScreenType)type {
  switch (type) {
    case kSignIn:
      return [[FullscreenSigninScreenCoordinator alloc]
           initWithBaseNavigationController:_navigationController
                                    browser:self.browser
                                   delegate:self
                               contextStyle:self.contextStyle
                                accessPoint:self.accessPoint
                                promoAction:_promoAction
          changeProfileContinuationProvider:_continuationProvider];
    case kHistorySync:
      return [[HistorySyncCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser
                                  delegate:self
                                  firstRun:NO
                             showUserEmail:NO
                                isOptional:YES
                              contextStyle:self.contextStyle
                               accessPoint:self.accessPoint];
    case kDefaultBrowserPromo:
    case kChoice:
    case kDockingPromo:
    case kBestFeatures:
    case kLensInteractivePromo:
    case kLensAnimatedPromo:
    case kStepsCompleted:
    case kSyncedSetUp:
    case kGuidedTour:
    case kSafariImport:
      break;
  }
  NOTREACHED() << static_cast<int>(type);
}

// Calls the completion callback with the given `result` and the given
// `identity`.
- (void)finishWithResult:(SigninCoordinatorResult)result
                identity:(id<SystemIdentity>)identity {
  if (self.accessPoint == signin_metrics::AccessPoint::kFullscreenSigninPromo) {
    // TODO(crbug.com/40074532): `addedAccount` is not always `NO`. Need to fix
    // that call to have the right value.
    [_fullscreenSigninPromoLogger logSigninCompletedWithResult:result
                                                  addedAccount:NO];
  }
  // When this coordinator is interrupted, `_childCoordinator` needs to be
  // stopped here.
  [self stopChildCoordinator];
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
  _screenProvider = nil;
}

#pragma mark - FirstRunScreenDelegate

// This is called before finishing the presentation of a screen.
// Stops the child coordinator and prepares the next screen to present.
- (void)screenWillFinishPresenting {
  CHECK(_childCoordinator) << base::SysNSStringToUTF8([self description]);
  [self stopChildCoordinator];
  [self presentScreenIfNeeded:[_screenProvider nextScreenType]];
}

#pragma mark - HistorySyncCoordinatorDelegate

// Dismisses the current screen.
- (void)historySyncCoordinator:(HistorySyncCoordinator*)historySyncCoordinator
                    withResult:(HistorySyncResult)result {
  [self screenWillFinishPresenting];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordAction(UserMetricsAction("Signin_TwoScreens_SwipeDismiss"));
  [self runCompletionWithSigninResult:SigninCoordinatorResultCanceledByUser
                   completionIdentity:nil];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@: %p, screenProvider: %p, childCoordinator: %@, "
                       @"navigationController %p>",
                       self.class.description, self, _screenProvider,
                       _childCoordinator.class.description,
                       _navigationController];
}

@end
