// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_coordinator.h"

#import <ostream>

#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_mediator.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_transition_delegate.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_view_controller.h"
#import "ios/chrome/browser/ui/authentication/identity_chooser/identity_chooser_view_controller_presentation_delegate.h"

namespace {

// Coordinator states.
typedef NS_ENUM(NSInteger, IdentityChooserCoordinatorState) {
  // Initiale state.
  IdentityChooserCoordinatorStateNotStarted = 0,
  // State when the view controller is displayed.
  IdentityChooserCoordinatorStateStarted,
  // State when the view is closed by tapping on "Add Account…" button.
  IdentityChooserCoordinatorStateClosedByAddingAccount,
  // State when the view is closed by selecting an identity.
  IdentityChooserCoordinatorStateClosedBySelectingIdentity,
  // State when the view is dismissed by tapping outside of the view.
  IdentityChooserCoordinatorStateClosedByDismiss,
};

}  // namespace

@interface IdentityChooserCoordinator ()<
    IdentityChooserViewControllerPresentationDelegate>

// Mediator.
@property(nonatomic, strong) IdentityChooserMediator* identityChooserMediator;
// View controller.
@property(nonatomic, strong)
    IdentityChooserViewController* identityChooserViewController;
// Coordinator state.
@property(nonatomic, assign) IdentityChooserCoordinatorState state;
// Transition delegate for the view controller presentation.
@property(nonatomic, strong)
    IdentityChooserTransitionDelegate* transitionController;

@end

@implementation IdentityChooserCoordinator

@synthesize delegate = _delegate;
@synthesize origin = _origin;
@synthesize identityChooserMediator = _identityChooserMediator;
@synthesize identityChooserViewController = _identityChooserViewController;
@synthesize state = _state;
@synthesize transitionController = _transitionController;

- (void)start {
  [super start];
  base::RecordAction(base::UserMetricsAction("Signin_AccountPicker_Open"));
  DCHECK_EQ(IdentityChooserCoordinatorStateNotStarted, self.state);
  self.state = IdentityChooserCoordinatorStateStarted;
  // Creates the controller.
  self.identityChooserViewController = [[IdentityChooserViewController alloc]
      initWithStyle:UITableViewStylePlain];
  self.identityChooserViewController.modalPresentationStyle =
      UIModalPresentationCustom;
  self.transitionController = [[IdentityChooserTransitionDelegate alloc] init];
  self.transitionController.origin = self.origin;
  self.identityChooserViewController.transitioningDelegate =
      self.transitionController;

  // Creates the mediator.
  self.identityChooserMediator = [[IdentityChooserMediator alloc]
      initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                        GetForProfile(
                                            self.browser->GetProfile())];

  self.identityChooserMediator.consumer = self.identityChooserViewController;
  // Setups.
  self.identityChooserViewController.presentationDelegate = self;
  // Starts.
  [self.identityChooserMediator start];
  [self.baseViewController
      presentViewController:self.identityChooserViewController
                   animated:YES
                 completion:nil];
}

- (void)stop {
  [super stop];
  base::RecordAction(base::UserMetricsAction("Signin_AccountPicker_Close"));
  [self.identityChooserMediator disconnect];
  self.identityChooserMediator = nil;
}

#pragma mark - Setters/Getters

- (void)setSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  self.identityChooserMediator.selectedIdentity = selectedIdentity;
}

- (id<SystemIdentity>)selectedIdentity {
  return self.identityChooserMediator.selectedIdentity;
}

#pragma mark - IdentityChooserViewControllerPresentationDelegate

- (void)identityChooserViewControllerDidDisappear:
    (IdentityChooserViewController*)viewController {
  DCHECK_EQ(self.identityChooserViewController, viewController);
  switch (self.state) {
    case IdentityChooserCoordinatorStateNotStarted:
    case IdentityChooserCoordinatorStateClosedByDismiss:
      NOTREACHED_IN_MIGRATION();
      break;
    case IdentityChooserCoordinatorStateStarted:
      // Dismissing the identity chooser dialog should be the same as accepting
      // the identity selected by default.
      [self.delegate identityChooserCoordinator:self
                              didSelectIdentity:self.selectedIdentity];
      self.state = IdentityChooserCoordinatorStateClosedByDismiss;
      break;
    case IdentityChooserCoordinatorStateClosedByAddingAccount:
      [self.delegate identityChooserCoordinatorDidTapOnAddAccount:self];
      break;
    case IdentityChooserCoordinatorStateClosedBySelectingIdentity:
      [self.delegate identityChooserCoordinator:self
                              didSelectIdentity:self.selectedIdentity];
      break;
  }
  [self.delegate identityChooserCoordinatorDidClose:self];
}

- (void)identityChooserViewControllerDidTapOnAddAccount:
    (IdentityChooserViewController*)viewController {
  DCHECK_EQ(self.identityChooserViewController, viewController);
  DCHECK_EQ(IdentityChooserCoordinatorStateStarted, self.state);
  self.state = IdentityChooserCoordinatorStateClosedByAddingAccount;
  [self.identityChooserViewController dismissViewControllerAnimated:YES
                                                         completion:nil];
}

- (void)identityChooserViewController:
            (IdentityChooserViewController*)viewController
          didSelectIdentityWithGaiaID:(NSString*)gaiaID {
  DCHECK_EQ(self.identityChooserViewController, viewController);
  DCHECK_EQ(IdentityChooserCoordinatorStateStarted, self.state);
  [self.identityChooserMediator selectIdentityWithGaiaID:gaiaID];
  // If the account refresh token is invalidated during this
  // operation then `identity` will be nil.
  if (self.selectedIdentity) {
    self.state = IdentityChooserCoordinatorStateClosedBySelectingIdentity;
  }
  [self.identityChooserViewController dismissViewControllerAnimated:YES
                                                         completion:nil];
}

@end
