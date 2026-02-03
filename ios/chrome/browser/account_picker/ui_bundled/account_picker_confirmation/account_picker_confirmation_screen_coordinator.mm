// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_coordinator.h"

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_coordinator_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_mediator.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_mediator_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_confirmation/account_picker_confirmation_screen_view_controller.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"

@interface AccountPickerConfirmationScreenCoordinator () <
    AccountPickerConfirmationScreenActionDelegate,
    AccountPickerConfirmationScreenMediatorDelegate>

@end

@implementation AccountPickerConfirmationScreenCoordinator {
  // The account picker configuration.
  __strong AccountPickerConfiguration* _configuration;
  // View controller.
  __strong AccountPickerConfirmationScreenViewController*
      _confirmationViewController;
  // Mediator.
  __strong AccountPickerConfirmationScreenMediator* _mediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                             configuration:
                                 (AccountPickerConfiguration*)configuration {
  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _configuration = configuration;
    _askEveryTime = YES;
  }
  return self;
}

- (void)dealloc {
  CHECK(!_mediator, base::NotFatalUntil::M151);
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  AuthenticationService* authentication_service =
      AuthenticationServiceFactory::GetForProfile(self.profile);
  _mediator = [[AccountPickerConfirmationScreenMediator alloc]
      initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                        GetForProfile(self.profile)
                    identityManager:IdentityManagerFactory::GetForProfile(
                                        self.profile)
                      configuration:_configuration
              authenticationService:authentication_service];
  _confirmationViewController =
      [[AccountPickerConfirmationScreenViewController alloc]
          initWithConfiguration:_configuration];
  _confirmationViewController.accountConfirmationChildViewController =
      self.childViewController;
  _mediator.consumer = _confirmationViewController;
  _mediator.delegate = self;
  _confirmationViewController.actionDelegate = self;
  _confirmationViewController.layoutDelegate = _layoutDelegate;
  [_confirmationViewController view];
}

- (void)stop {
  [_mediator disconnect];
  _mediator.delegate = nil;
  _mediator = nil;
  _confirmationViewController = nil;
  [super stop];
}

#pragma mark - AccountPickerConfirmationScreenCoordinator

- (void)startValidationSpinner {
  [_confirmationViewController startSpinner];
}

- (void)stopValidationSpinner {
  [_confirmationViewController stopSpinner];
}

- (void)setIdentityButtonHidden:(BOOL)hidden animated:(BOOL)animated {
  [_confirmationViewController setIdentityButtonHidden:hidden
                                              animated:animated];
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return _confirmationViewController;
}

- (id<SystemIdentity>)selectedIdentity {
  return _mediator.selectedIdentity;
}

- (void)setSelectedIdentity:(id<SystemIdentity>)identity {
  DCHECK(_mediator);
  _mediator.selectedIdentity = identity;
}

#pragma mark - AccountPickerConfirmationScreenActionDelegate

- (void)accountPickerConfirmationScreenViewControllerCancel:
    (AccountPickerConfirmationScreenViewController*)viewController {
  [_delegate accountPickerConfirmationScreenCoordinatorWantsToBeStopped:self];
}

- (void)accountPickerConfirmationScreenViewControllerOpenAccountList:
    (AccountPickerConfirmationScreenViewController*)viewController {
  [_delegate
      accountPickerConfirmationScreenCoordinatorOpenAccountPickerSelectionScreen:
          self];
}

- (void)accountPickerConfirmationScreenViewController:
            (AccountPickerConfirmationScreenViewController*)viewController
                                      setAskEveryTime:(BOOL)askEveryTime {
  _askEveryTime = askEveryTime;
}

- (void)
    accountPickerConfirmationScreenViewControllerContinueWithSelectedIdentity:
        (AccountPickerConfirmationScreenViewController*)viewController {
  [_delegate accountPickerConfirmationScreenCoordinatorSubmit:self];
}

#pragma mark - AccountPickerConfirmationScreenMediatorDelegate

- (void)accountPickerConfirmationScreenMediatorWantsToBeStopped:
    (AccountPickerConfirmationScreenMediator*)mediator {
  [_delegate accountPickerConfirmationScreenCoordinatorWantsToBeStopped:self];
}

@end
