// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/welcome_back/coordinator/welcome_back_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/welcome_back_promo_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/welcome_back/coordinator/welcome_back_mediator.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/browser/welcome_back/ui/welcome_back_action_handler.h"
#import "ios/chrome/browser/welcome_back/ui/welcome_back_view_controller.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"

@interface WelcomeBackCoordinator () <ConfirmationAlertActionHandler,
                                      WelcomeBackActionHandler>
@end

@implementation WelcomeBackCoordinator {
  // Welcome Back mediator.
  WelcomeBackMediator* _mediator;
  // Welcome Back view controller.
  WelcomeBackViewController* _viewController;
  // Base navigation controller.
  UINavigationController* _navigationController;
}

#pragma mark - ChromeCoordinator

- (void)start {
  ProfileIOS* profile = self.profile->GetOriginalProfile();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);

  _mediator = [[WelcomeBackMediator alloc]
      initWithAuthenticationService:authenticationService
              accountManagerService:accountManagerService];

  _viewController = [[WelcomeBackViewController alloc] init];
  _viewController.actionHandler = self;
  _viewController.welcomeBackActionHandler = self;

  _mediator.consumer = _viewController;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.navigationBarHidden = YES;

  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  // Dismiss the presented view controller.
  if (_navigationController.presentingViewController &&
      !_navigationController.isBeingDismissed) {
    [_navigationController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
  }

  // Clean up references.
  _viewController = nil;
  _navigationController = nil;
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self hidePromo];
}

#pragma mark - WelcomeBackActionHandler

- (void)didTapBestFeatureItem:(BestFeaturesItem*)item {
  // TODO(crbug.com/457592200): Implement action for item tap.
}

#pragma mark - Private

// Dismisses the promo by sending a command.
- (void)hidePromo {
  id<WelcomeBackPromoCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), WelcomeBackPromoCommands);
  [handler hideWelcomeBackPromo];
}

@end
