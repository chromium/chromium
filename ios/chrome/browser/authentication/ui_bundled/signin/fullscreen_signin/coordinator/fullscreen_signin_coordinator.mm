// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/fullscreen_signin/coordinator/fullscreen_signin_coordinator.h"

#import <UIKit/UIKit.h>

#import "base/notreached.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/ui_bundled/fullscreen_signin_screen/coordinator/fullscreen_signin_screen_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_provider.h"
#import "ios/chrome/browser/screen/ui_bundled/screen_type.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@interface FullscreenSigninCoordinator () <FirstRunScreenDelegate>

@property(nonatomic, strong) ScreenProvider* screenProvider;
@property(nonatomic, strong) ChromeCoordinator* childCoordinator;

// The view controller used by FullscreenSigninCoordinator.
@property(nonatomic, strong) UINavigationController* navigationController;

@end

@implementation FullscreenSigninCoordinator {
  ChangeProfileContinuationProvider _changeProfileContinuationProvider;
}

- (instancetype)
           initWithBaseViewController:(UIViewController*)viewController
                              browser:(Browser*)browser
                       screenProvider:(ScreenProvider*)screenProvider
                         contextStyle:(SigninContextStyle)contextStyle
                          accessPoint:(signin_metrics::AccessPoint)accessPoint
    changeProfileContinuationProvider:(const ChangeProfileContinuationProvider&)
                                          changeProfileContinuationProvider {
  DCHECK(!browser->GetProfile()->IsOffTheRecord());
  self = [super initWithBaseViewController:viewController
                                   browser:browser
                              contextStyle:contextStyle
                               accessPoint:accessPoint];
  if (self) {
    CHECK(changeProfileContinuationProvider);
    _screenProvider = screenProvider;
    _changeProfileContinuationProvider = changeProfileContinuationProvider;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(self.profile);
  CHECK(!identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin),
        base::NotFatalUntil::M142);
  self.navigationController =
      [[UINavigationController alloc] initWithNavigationBarClass:nil
                                                    toolbarClass:nil];
  self.navigationController.modalPresentationStyle =
      UIModalPresentationFormSheet;

  [self presentScreen:[self.screenProvider nextScreenType]];

  // Note: If the user was already signed in, then the `presentScreen` call
  // above may have already synchronously completed all the screens, and then
  // `self.navigationController` would already be nil again. That is invalid;
  // the caller must have checked for this case before.
  CHECK(self.navigationController);

  [self.navigationController setNavigationBarHidden:YES animated:NO];
  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - AnimatedCoordinator

- (void)stopAnimated:(BOOL)animated {
  // Stop the child coordinator UI first before dismissing the forced
  // sign-in navigation controller.
  [self stopChildCoordinator];
  self.screenProvider = nil;

  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:nil];
  self.navigationController = nil;

  [super stopAnimated:animated];
}

#pragma mark - Private

- (void)stopChildCoordinator {
  [self.childCoordinator stop];
  self.childCoordinator = nil;
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
  void (^completion)(void) = ^{
    SigninCoordinatorResult result =
        identity ? SigninCoordinatorResultSuccess
                 : SigninCoordinatorResultCanceledByUser;
    [weakSelf finishWithResult:result identity:identity];
  };
  [self.navigationController dismissViewControllerAnimated:YES
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
  self.childCoordinator = [self createChildCoordinatorWithScreenType:type];
  [self.childCoordinator start];
}

// Creates a screen coordinator according to `type`.
- (ChromeCoordinator*)createChildCoordinatorWithScreenType:(ScreenType)type {
  switch (type) {
    case kSignIn:
      return [[FullscreenSigninScreenCoordinator alloc]
           initWithBaseNavigationController:self.navigationController
                                    browser:self.browser
                                   delegate:self
                               contextStyle:self.contextStyle
                                accessPoint:self.accessPoint
                                promoAction:signin_metrics::PromoAction::
                                                PROMO_ACTION_NO_SIGNIN_PROMO
          changeProfileContinuationProvider:_changeProfileContinuationProvider];
    case kHistorySync:
    case kDefaultBrowserPromo:
    case kChoice:
    case kDockingPromo:
    case kBestFeatures:
    case kLensInteractivePromo:
    case kStepsCompleted:
      NOTREACHED() << "Type of screen not supported." << static_cast<int>(type);
  }
  return nil;
}

- (void)finishWithResult:(SigninCoordinatorResult)result
                identity:(id<SystemIdentity>)identity {
  [self stopChildCoordinator];
  self.navigationController = nil;
  self.screenProvider = nil;
  id<SystemIdentity> completionIdentity = identity;
  [self runCompletionWithSigninResult:result
                   completionIdentity:completionIdentity];
}

#pragma mark - FirstRunScreenDelegate

// This is called before finishing the presentation of a screen.
// Stops the child coordinator and prepares the next screen to present.
- (void)screenWillFinishPresenting {
  [self stopChildCoordinator];
  [self presentScreen:[self.screenProvider nextScreenType]];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:@"<%@: %p, screenProvider: %p, childCoordinator: %p, "
                       @"navigationController %p>",
                       self.class.description, self, self.screenProvider,
                       self.childCoordinator, self.navigationController];
}

@end
