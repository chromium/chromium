// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/two_screens_signin/two_screens_signin_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/first_run/ui_bundled/signin/signin_screen_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/logging/upgrade_signin_logger.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/authentication/signin/uno_signin_screen_provider.h"
#import "ios/chrome/browser/ui/screen/screen_provider.h"
#import "ios/chrome/browser/ui/screen/screen_type.h"

using base::RecordAction;
using base::UserMetricsAction;

@interface TwoScreensSigninCoordinator () <
    HistorySyncCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate>
@end

@implementation TwoScreensSigninCoordinator {
  signin_metrics::PromoAction _promoAction;

  // This can be either the SigninScreenCoordinator or the
  // HistorySyncCoordinator depending on which step the user is on.
  InterruptibleChromeCoordinator* _childCoordinator;

  // The navigation controller used to present the views.
  UINavigationController* _navigationController;

  // The screen provider that specifies which screens to present.
  ScreenProvider* _screenProvider;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
                   promoAction:(signin_metrics::PromoAction)promoAction {
  DCHECK(!browser->GetProfile()->IsOffTheRecord());
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                               accessPoint:accessPoint];
  if (self) {
    // This coordinator should not be used in the FRE.
    CHECK_NE(accessPoint, signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE);
    _promoAction = promoAction;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  if (self.accessPoint ==
      signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO) {
    ChromeAccountManagerService* accountManagerService =
        ChromeAccountManagerServiceFactory::GetForProfile(
            self.browser->GetProfile());
    // TODO(crbug.com/41352590): Need to add `CHECK(accountManagerService)`.
    [UpgradeSigninLogger logSigninStartedWithAccessPoint:self.accessPoint
                                   accountManagerService:accountManagerService];
  }
  _screenProvider = [[UnoSigninScreenProvider alloc] init];
  _navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];
  _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  _navigationController.presentationController.delegate = self;

  [self presentScreen:[_screenProvider nextScreenType]];

  [_navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  if (_navigationController) {
    __block BOOL completionBlockCalled = NO;
    [self interruptWithAction:SigninCoordinatorInterrupt::UIShutdownNoDismiss
                   completion:^{
                     completionBlockCalled = YES;
                   }];
    CHECK(completionBlockCalled);
  }
  DCHECK(!_navigationController);
  DCHECK(!_childCoordinator);
  DCHECK(!_screenProvider);
  [super stop];
}

#pragma mark - Private

// Dismiss the main navigation view controller with an animation and run the
// sign-in completion callback on completion of the animation to finish
// presenting the screens.
- (void)finishPresentingScreens {
  __weak __typeof(self) weakSelf = self;
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForProfile(self.browser->GetProfile());
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  ProceduralBlock completion = ^{
    SigninCoordinatorResult result =
        identity ? SigninCoordinatorResultSuccess
                 : SigninCoordinatorResultCanceledByUser;
    [weakSelf finishWithResult:result identity:identity];
  };
  [_navigationController dismissViewControllerAnimated:YES
                                            completion:completion];
}

// Presents the screen of certain `type`.
- (void)presentScreen:(ScreenType)type {
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
- (InterruptibleChromeCoordinator*)createChildCoordinatorWithScreenType:
    (ScreenType)type {
  switch (type) {
    case kSignIn:
      return [[SigninScreenCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser
                                  delegate:self
                               accessPoint:self.accessPoint
                               promoAction:_promoAction];
    case kHistorySync:
      return [[HistorySyncCoordinator alloc]
          initWithBaseNavigationController:_navigationController
                                   browser:self.browser
                                  delegate:self
                                  firstRun:NO
                             showUserEmail:NO
                                isOptional:YES
                               accessPoint:self.accessPoint];
    case kDefaultBrowserPromo:
    case kChoice:
    case kDockingPromo:
    case kStepsCompleted:
      break;
  }
  NOTREACHED() << static_cast<int>(type);
}

// Calls the completion callback with the given `result` and a
// SigninCompletionInfo object that includes the given `identity`.
- (void)finishWithResult:(SigninCoordinatorResult)result
                identity:(id<SystemIdentity>)identity {
  if (self.accessPoint ==
      signin_metrics::AccessPoint::ACCESS_POINT_SIGNIN_PROMO) {
    // TODO(crbug.com/40074532): `addedAccount` is not always `NO`. Need to fix
    // that call to have the right value.
    [UpgradeSigninLogger logSigninCompletedWithResult:result addedAccount:NO];
  }
  // When this coordinator is interrupted, `_childCoordinator` needs to be
  // stopped here.
  if (_childCoordinator) {
    [_childCoordinator stop];
    _childCoordinator = nil;
  }
  _navigationController.presentationController.delegate = nil;
  _navigationController = nil;
  _screenProvider = nil;
  SigninCompletionInfo* completionInfo =
      [SigninCompletionInfo signinCompletionInfoWithIdentity:identity];
  [self runCompletionCallbackWithSigninResult:result
                               completionInfo:completionInfo];
}

#pragma mark - FirstRunScreenDelegate

// This is called before finishing the presentation of a screen.
// Stops the child coordinator and prepares the next screen to present.
- (void)screenWillFinishPresenting {
  CHECK(_childCoordinator) << base::SysNSStringToUTF8([self description]);
  [_childCoordinator stop];
  _childCoordinator = nil;
  [self presentScreen:[_screenProvider nextScreenType]];
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  __weak __typeof(_navigationController) weakNavigationController =
      _navigationController;
  ProceduralBlock finishCompletion = ^() {
    [weakSelf finishWithResult:SigninCoordinatorResultInterrupted identity:nil];
    if (completion) {
      completion();
    }
  };
  BOOL animated = NO;
  switch (action) {
    case SigninCoordinatorInterrupt::UIShutdownNoDismiss: {
      [_childCoordinator
          interruptWithAction:SigninCoordinatorInterrupt::UIShutdownNoDismiss
                   completion:^{
                     [weakNavigationController.presentingViewController
                         dismissViewControllerAnimated:NO
                                            completion:nil];
                     finishCompletion();
                   }];
      return;
    }
    case SigninCoordinatorInterrupt::DismissWithoutAnimation: {
      animated = NO;
      break;
    }
    case SigninCoordinatorInterrupt::DismissWithAnimation: {
      animated = YES;
      break;
    }
  }

  // Interrupt the child coordinator UI first before dismissing the new
  // sign-in navigation controller.
  [_childCoordinator
      interruptWithAction:SigninCoordinatorInterrupt::DismissWithoutAnimation
               completion:^{
                 UIViewController* presentingViewController =
                     weakNavigationController.presentingViewController;
                 if (presentingViewController) {
                   [presentingViewController
                       dismissViewControllerAnimated:animated
                                          completion:finishCompletion];
                 } else {
                   finishCompletion();
                 }
               }];
}

#pragma mark - HistorySyncCoordinatorDelegate

// Dismisses the current screen.
- (void)closeHistorySyncCoordinator:
            (HistorySyncCoordinator*)historySyncCoordinator
                     declinedByUser:(BOOL)declined {
  [self screenWillFinishPresenting];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  RecordAction(UserMetricsAction("Signin_TwoScreens_SwipeDismiss"));
  [self interruptWithAction:SigninCoordinatorInterrupt::DismissWithoutAnimation
                 completion:nil];
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
