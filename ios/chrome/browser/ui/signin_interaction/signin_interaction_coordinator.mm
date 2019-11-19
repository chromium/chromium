// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_coordinator.h"

#import "base/ios/block_types.h"
#include "base/logging.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/settings/google_services/advanced_signin_settings_coordinator.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_presenting.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninInteractionCoordinator () <
    AdvancedSigninSettingsCoordinatorDelegate,
    SigninInteractionPresenting>

// Coordinator to present alerts.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// The controller managed by this coordinator.
@property(nonatomic, strong) SigninInteractionController* controller;

// The dispatcher to which commands should be sent.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

// The UIViewController upon which UI should be presented.
@property(nonatomic, strong) UIViewController* presentingViewController;

// Bookkeeping for the top-most view controller.
@property(nonatomic, strong) UIViewController* topViewController;

// Sign-in completion.
@property(nonatomic, copy) signin_ui::CompletionCallback signinCompletion;

// Advanced sign-in settings coordinator.
@property(nonatomic, strong)
    AdvancedSigninSettingsCoordinator* advancedSigninSettingsCoordinator;

@end

@implementation SigninInteractionCoordinator

- (instancetype)initWithBrowser:(Browser*)browser
                     dispatcher:
                         (id<ApplicationCommands, BrowserCommands>)dispatcher {
  DCHECK(browser);
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _dispatcher = dispatcher;
  }
  return self;
}

- (void)signInWithIdentity:(ChromeIdentity*)identity
                 accessPoint:(signin_metrics::AccessPoint)accessPoint
                 promoAction:(signin_metrics::PromoAction)promoAction
    presentingViewController:(UIViewController*)viewController
                  completion:(signin_ui::CompletionCallback)completion {
  // Ensure that nothing is done if a sign in operation is already in progress.
  if (self.controller) {
    return;
  }

  [self setupForSigninOperationWithAccessPoint:accessPoint
                                   promoAction:promoAction
                      presentingViewController:viewController
                                    completion:completion];

  [self.controller signInWithIdentity:identity
                           completion:[self callbackToClearState]];
}

- (void)reAuthenticateWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                          promoAction:(signin_metrics::PromoAction)promoAction
             presentingViewController:(UIViewController*)viewController
                           completion:
                               (signin_ui::CompletionCallback)completion {
  // Ensure that nothing is done if a sign in operation is already in progress.
  if (self.controller) {
    return;
  }

  [self setupForSigninOperationWithAccessPoint:accessPoint
                                   promoAction:promoAction
                      presentingViewController:viewController
                                    completion:completion];

  [self.controller reAuthenticateWithCompletion:[self callbackToClearState]];
}

- (void)addAccountWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction
         presentingViewController:(UIViewController*)viewController
                       completion:(signin_ui::CompletionCallback)completion {
  // Ensure that nothing is done if a sign in operation is already in progress.
  if (self.controller) {
    return;
  }

  [self setupForSigninOperationWithAccessPoint:accessPoint
                                   promoAction:promoAction
                      presentingViewController:viewController
                                    completion:completion];

  [self.controller addAccountWithCompletion:[self callbackToClearState]];
}

- (void)showAdvancedSigninSettingsWithPresentingViewController:
    (UIViewController*)viewController {
  self.presentingViewController = viewController;
  [self showAdvancedSigninSettings];
}

- (void)cancel {
  [self.controller cancel];
  [self.advancedSigninSettingsCoordinator abortWithDismiss:NO
                                                  animated:YES
                                                completion:nil];
}

- (void)cancelAndDismiss {
  [self.controller cancelAndDismiss];
  [self.advancedSigninSettingsCoordinator abortWithDismiss:YES
                                                  animated:YES
                                                completion:nil];
}

- (void)abortAndDismissSettingsViewAnimated:(BOOL)animated
                                 completion:(ProceduralBlock)completion {
  DCHECK(!self.controller);
  DCHECK(self.advancedSigninSettingsCoordinator);
  [self.advancedSigninSettingsCoordinator abortWithDismiss:YES
                                                  animated:animated
                                                completion:completion];
}

#pragma mark - Properties

- (BOOL)isActive {
  return self.controller != nil || self.isSettingsViewPresented;
}

- (BOOL)isSettingsViewPresented {
  return self.advancedSigninSettingsCoordinator != nil;
}

#pragma mark - AdvancedSigninSettingsCoordinatorDelegate

- (void)advancedSigninSettingsCoordinatorDidClose:
            (AdvancedSigninSettingsCoordinator*)coordinator
                                         signedin:(BOOL)signedin {
  DCHECK_EQ(self.advancedSigninSettingsCoordinator, coordinator);
  self.advancedSigninSettingsCoordinator = nil;
  [self signinDoneWithSuccess:signedin];
}

#pragma mark - SigninInteractionPresenting

- (void)presentViewController:(UIViewController*)viewController
                     animated:(BOOL)animated
                   completion:(ProceduralBlock)completion {
  DCHECK_EQ(self.presentingViewController, self.topViewController);
  [self presentTopViewController:viewController
                        animated:animated
                      completion:completion];
}

- (void)presentTopViewController:(UIViewController*)viewController
                        animated:(BOOL)animated
                      completion:(ProceduralBlock)completion {
  DCHECK(viewController);
  DCHECK(self.topViewController);
  [self.topViewController presentViewController:viewController
                                       animated:animated
                                     completion:completion];
  self.topViewController = viewController;
}

- (void)dismissAllViewControllersAnimated:(BOOL)animated
                               completion:(ProceduralBlock)completion {
  DCHECK([self isPresenting]);
  [self.presentingViewController dismissViewControllerAnimated:animated
                                                    completion:completion];
  self.topViewController = self.presentingViewController;
}

- (void)presentError:(NSError*)error
       dismissAction:(ProceduralBlock)dismissAction {
  DCHECK(!self.alertCoordinator);
  DCHECK(self.topViewController);
  DCHECK(![self.topViewController presentedViewController]);
  self.alertCoordinator =
      ErrorCoordinator(error, dismissAction, self.topViewController);
  [self.alertCoordinator start];
}

- (void)dismissError {
  [self.alertCoordinator executeCancelHandler];
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

- (BOOL)isPresenting {
  return self.presentingViewController.presentedViewController != nil;
}

#pragma mark - Private Methods

// Sets up relevant instance variables for a sign in operation.
- (void)setupForSigninOperationWithAccessPoint:
            (signin_metrics::AccessPoint)accessPoint
                                   promoAction:
                                       (signin_metrics::PromoAction)promoAction
                      presentingViewController:
                          (UIViewController*)presentingViewController
                                    completion:(signin_ui::CompletionCallback)
                                                   completion {
  DCHECK(![self isPresenting]);
  DCHECK(!self.signinCompletion);
  self.signinCompletion = completion;
  self.presentingViewController = presentingViewController;
  self.topViewController = presentingViewController;

  self.controller =
      [[SigninInteractionController alloc] initWithBrowser:self.browser
                                      presentationProvider:self
                                               accessPoint:accessPoint
                                               promoAction:promoAction
                                                dispatcher:self.dispatcher];
}

// Returns a callback that clears the state of the coordinator and runs
// |completion|.
- (SigninInteractionControllerCompletionCallback)callbackToClearState {
  __weak SigninInteractionCoordinator* weakSelf = self;
  SigninInteractionControllerCompletionCallback completionCallback =
      ^(SigninResult signinResult) {
        [weakSelf
            signinInteractionControllerCompletionWithSigninResult:signinResult];
      };
  return completionCallback;
}

// Called when SigninInteractionController is completed.
- (void)signinInteractionControllerCompletionWithSigninResult:
    (SigninResult)signinResult {
  self.controller = nil;
  self.topViewController = nil;
  self.alertCoordinator = nil;
  if (signinResult == SigninResultSignedInnAndOpennSettings) {
    [self showAdvancedSigninSettings];
  } else {
    [self signinDoneWithSuccess:signinResult != SigninResultCanceled];
  }
}

// Shows the advanced sign-in settings UI.
- (void)showAdvancedSigninSettings {
  DCHECK(!self.advancedSigninSettingsCoordinator);
  DCHECK(self.presentingViewController);
  self.advancedSigninSettingsCoordinator =
      [[AdvancedSigninSettingsCoordinator alloc]
          initWithBaseViewController:self.presentingViewController
                             browser:self.browser];
  self.advancedSigninSettingsCoordinator.delegate = self;
  self.advancedSigninSettingsCoordinator.dispatcher = self.dispatcher;
  [self.advancedSigninSettingsCoordinator start];
}

// Called when the sign-in is done.
- (void)signinDoneWithSuccess:(BOOL)success {
  DCHECK(!self.controller);
  DCHECK(!self.topViewController);
  DCHECK(!self.alertCoordinator);
  if (self.signinCompletion) {
    self.signinCompletion(success);
    self.signinCompletion = nil;
  }
  [self.advancedSigninSettingsCoordinator stop];
  self.advancedSigninSettingsCoordinator = nil;
  self.presentingViewController = nil;
}

@end
