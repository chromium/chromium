// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/identity_chooser/identity_chooser_coordinator.h"

#import <ostream>

#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/identity_chooser/identity_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/identity_chooser/identity_chooser_mediator.h"
#import "ios/chrome/browser/authentication/ui_bundled/identity_chooser/identity_chooser_transition_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/identity_chooser/identity_chooser_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/identity_chooser/identity_chooser_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

@interface IdentityChooserCoordinator () <
    IdentityChooserViewControllerPresentationDelegate>

// Mediator.
@property(nonatomic, strong) IdentityChooserMediator* identityChooserMediator;
// View controller.
@property(nonatomic, strong)
    IdentityChooserViewController* identityChooserViewController;
// Transition delegate for the view controller presentation.
@property(nonatomic, strong)
    IdentityChooserTransitionDelegate* transitionController;

@end

@implementation IdentityChooserCoordinator {
  id<SystemIdentity> _defaultIdentity;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                           defaultIdentity:(id<SystemIdentity>)defaultIdentity {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _defaultIdentity = defaultIdentity;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  base::RecordAction(base::UserMetricsAction("Signin_AccountPicker_Open"));
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
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  self.identityChooserMediator = [[IdentityChooserMediator alloc]
      initWithIdentityManager:identityManager
        accountManagerService:accountManagerService
              defaultIdentity:_defaultIdentity];

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
  self.identityChooserViewController.presentationDelegate = nil;
  [self.identityChooserViewController dismissViewControllerAnimated:NO
                                                         completion:nil];
  self.identityChooserViewController = nil;
  base::RecordAction(base::UserMetricsAction("Signin_AccountPicker_Close"));
  [self.identityChooserMediator disconnect];
  self.identityChooserMediator.consumer = nil;
  self.identityChooserMediator = nil;
}

#pragma mark - IdentityChooserViewControllerPresentationDelegate

- (void)identityChooserViewControllerDidDisappear:
    (IdentityChooserViewController*)viewController {
  DCHECK_EQ(self.identityChooserViewController, viewController);
  // The view being dismissed is similar to selecting the default account.
  // This method is not called if a button was tapped first.
  GaiaId gaiaId = _defaultIdentity.gaiaId;
  __weak __typeof(self) weakSelf = self;
  [self closeViewControllerWithCompletion:^{
    [weakSelf selectGaiaID:gaiaId];
  }];
}

- (void)identityChooserViewControllerDidTapOnAddAccount:
    (IdentityChooserViewController*)viewController {
  DCHECK_EQ(self.identityChooserViewController, viewController);
  __weak __typeof(self) weakSelf = self;
  [self closeViewControllerWithCompletion:^{
    [weakSelf addAccount];
  }];
}

- (void)identityChooserViewController:
            (IdentityChooserViewController*)viewController
          didSelectIdentityWithGaiaID:(const GaiaId&)gaiaId {
  DCHECK_EQ(self.identityChooserViewController, viewController);
  // Variable used keep the gaia id alive until the completion is called.
  const GaiaId selectedGaiaID = gaiaId;
  __weak __typeof(self) weakSelf = self;
  // Copy the id in order to capture it in the block.
  [self closeViewControllerWithCompletion:^{
    [weakSelf selectGaiaID:selectedGaiaID];
  }];
}

#pragma mark - Private

- (void)addAccount {
  [self.delegate identityChooserCoordinatorDidTapOnAddAccount:self];
}

- (void)selectGaiaID:(const GaiaId&)gaiaID {
  id<SystemIdentity> identity =
      ChromeAccountManagerServiceFactory::GetForProfile(self.profile)
          ->GetIdentityOnDeviceWithGaiaID(gaiaID);
  [self.delegate identityChooserCoordinator:self
               didCloseWithSelectedIdentity:identity];
}

- (void)closeViewControllerWithCompletion:(ProceduralBlock)completion {
  CHECK(completion);
  self.identityChooserViewController.presentationDelegate = nil;
  [self.identityChooserViewController dismissViewControllerAnimated:YES
                                                         completion:^{
                                                           completion();
                                                         }];
  self.identityChooserViewController = nil;
}

@end
