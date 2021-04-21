// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/signin_screen_coordinator.h"

#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/first_run/signin_screen_consumer.h"
#import "ios/chrome/browser/ui/first_run/signin_screen_mediator.h"
#import "ios/chrome/browser/ui/first_run/signin_screen_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninScreenCoordinator () <IdentityChooserCoordinatorDelegate,
                                       SigninScreenViewControllerDelegate>

// First run screen delegate.
@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;
// Sign-in screen view controller.
@property(nonatomic, strong) SigninScreenViewController* viewController;
// Sign-in screen mediator.
@property(nonatomic, strong) SigninScreenMediator* mediator;
// Coordinator handling choosing the account to sign in with.
@property(nonatomic, strong)
    IdentityChooserCoordinator* identityChooserCoordinator;
// Coordinator handling adding a user account.
@property(nonatomic, strong) SigninCoordinator* addAccountSigninCoordinator;

@end

@implementation SigninScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
  }
  return self;
}

- (void)start {
  // TODO(crbug.com/1189836): Check if sign-in screen need to be shown.
  // if not:
  // [self.delegate willFinishPresenting]
  // if yes:
  self.viewController = [[SigninScreenViewController alloc] init];
  self.viewController.delegate = self;
  self.mediator = [[SigninScreenMediator alloc] init];
  self.mediator.consumer = self.viewController;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
}

- (void)stop {
  self.delegate = nil;
  self.viewController = nil;
  self.mediator = nil;
  [self.identityChooserCoordinator stop];
  self.identityChooserCoordinator = nil;
  [self.addAccountSigninCoordinator stop];
  self.addAccountSigninCoordinator = nil;
}

#pragma mark - SigninScreenViewControllerDelegate

- (void)showAccountPickerFromPoint:(CGPoint)point {
  self.identityChooserCoordinator = [[IdentityChooserCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.identityChooserCoordinator.delegate = self;
  self.identityChooserCoordinator.origin = point;
  [self.identityChooserCoordinator start];
  self.identityChooserCoordinator.selectedIdentity =
      self.mediator.selectedIdentity;
}

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/1189836): store the sync status
  [self.delegate willFinishPresenting];
}

#pragma mark - IdentityChooserCoordinatorDelegate

- (void)identityChooserCoordinatorDidClose:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  self.identityChooserCoordinator.delegate = nil;
  self.identityChooserCoordinator = nil;
}

- (void)identityChooserCoordinatorDidTapOnAddAccount:
    (IdentityChooserCoordinator*)coordinator {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  DCHECK(!self.addAccountSigninCoordinator);
  // TODO(crbug.com/1189836): add metrics recording as in
  // RecordFirstRunMetricsInternal for signin attempt.

  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                      accessPoint:signin_metrics::AccessPoint::
                                                      ACCESS_POINT_START_PAGE];

  __weak __typeof(self) weakSelf = self;
  self.addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf addAccountSigninCompleteWithResult:signinResult
                                      completionInfo:signinCompletionInfo];
      };
  [self.addAccountSigninCoordinator start];
}

- (void)identityChooserCoordinator:(IdentityChooserCoordinator*)coordinator
                 didSelectIdentity:(ChromeIdentity*)identity {
  CHECK_EQ(self.identityChooserCoordinator, coordinator);
  self.mediator.selectedIdentity = identity;
}

#pragma mark - Private

// Callback handling the completion of the AddAccount action.
- (void)addAccountSigninCompleteWithResult:(SigninCoordinatorResult)signinResult
                            completionInfo:
                                (SigninCompletionInfo*)signinCompletionInfo {
  [self.addAccountSigninCoordinator stop];
  self.addAccountSigninCoordinator = nil;
  if (signinResult == SigninCoordinatorResultSuccess) {
    self.mediator.selectedIdentity = signinCompletionInfo.identity;
  }
  if (signinCompletionInfo.signinCompletionAction ==
      SigninCompletionActionOpenCompletionURL) {
    // TODO(crbug.com/1189836): handle URL opening.
  }
}

@end
