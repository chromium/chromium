// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/age_mismatch_signout_coordinator.h"

#import "base/not_fatal_until.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/age_mismatch_learn_more/coordinator/age_mismatch_learn_more_coordinator.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/age_mismatch_signout_mediator.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/ui/age_mismatch_signout_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/avatar/avatar_provider.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@interface AgeMismatchSignoutCoordinator () <
    AgeMismatchLearnMoreCoordinatorDelegate,
    PromoStyleViewControllerDelegate>
@end

@implementation AgeMismatchSignoutCoordinator {
  // View controller for the Age Mismatch Prompt.
  AgeMismatchSignoutViewController* _viewController;

  // Mediator for the Age Mismatch Prompt.
  AgeMismatchSignoutMediator* _mediator;

  // The identity to display the account details for.
  id<SystemIdentity> _identity;

  // The mode of the prompt.
  AgeMismatchPromptMode _mode;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  identity:(id<SystemIdentity>)identity
                                      mode:(AgeMismatchPromptMode)mode {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _identity = identity;
    _mode = mode;
  }
  return self;
}

- (void)start {
  [super start];
  _mediator = [[AgeMismatchSignoutMediator alloc]
            initWithIdentity:_identity
      identityAvatarProvider:GetApplicationContext()
                                 ->GetIdentityAvatarProvider()
             identityManager:IdentityManagerFactory::GetForProfile(
                                 self.browser->GetProfile())];
  _viewController =
      [[AgeMismatchSignoutViewController alloc] initWithMode:_mode];
  _viewController.delegate = self;
  _mediator.consumer = _viewController;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  [_mediator disconnect];
  _mediator = nil;
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController.delegate = nil;
  _viewController = nil;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [_viewController blockUI];
  [self.delegate ageMismatchSignoutCoordinatorWantsToSignIn:self];
}

- (void)didTapSecondaryActionButton {
  [_viewController blockUI];
  [self.delegate ageMismatchSignoutCoordinatorWantsToBeStopped:self];
}

- (void)didTapURLInDisclaimer:(NSURL*)URL {
  CHECK([URL isEqual:[NSURL URLWithString:kAgeMismatchSignoutLearnMoreURL]],
        base::NotFatalUntil::M153);
  AgeMismatchLearnMoreCoordinator* coordinator =
      [[AgeMismatchLearnMoreCoordinator alloc]
          initWithBaseViewController:_viewController
                             browser:self.browser];
  coordinator.delegate = self;
  [self.childCoordinators addObject:coordinator];
  [coordinator start];
}

#pragma mark - AgeMismatchLearnMoreCoordinatorDelegate

- (void)ageMismatchLearnMoreCoordinatorWantsToBeStopped:
    (AgeMismatchLearnMoreCoordinator*)coordinator {
  [coordinator stop];
  [self.childCoordinators removeObject:coordinator];
}

@end
