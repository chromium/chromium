// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/account_picker/account_picker_confirmation/account_picker_confirmation_screen_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_confirmation/account_picker_confirmation_screen_coordinator_delegate.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_confirmation/account_picker_confirmation_screen_mediator.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_confirmation/account_picker_confirmation_screen_view_controller.h"

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
  }
  return self;
}

- (void)start {
  [super start];
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  _mediator = [[AccountPickerConfirmationScreenMediator alloc]
      initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                        GetForBrowserState(browserState)
                      configuration:_configuration];
  _mediator.delegate = self;
  _confirmationViewController =
      [[AccountPickerConfirmationScreenViewController alloc]
          initWithConfiguration:_configuration];
  _mediator.consumer = _confirmationViewController;
  _confirmationViewController.actionDelegate = self;
  _confirmationViewController.layoutDelegate = _layoutDelegate;
  [_confirmationViewController view];
}

- (void)startValidationSpinner {
  [_confirmationViewController startSpinner];
}

- (void)stopValidationSpinner {
  [_confirmationViewController stopSpinner];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  _confirmationViewController = nil;
  [super stop];
}

- (void)dealloc {
  // TODO(crbug.com/1454777)
  DUMP_WILL_BE_CHECK(!_mediator);
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

#pragma mark - AccountPickerConfirmationScreenMediatorDelegate

- (void)accountPickerConfirmationScreenMediatorNoIdentities:
    (AccountPickerConfirmationScreenMediator*)mediator {
  [_delegate accountPickerConfirmationScreenCoordinatorAllIdentityRemoved:self];
}

#pragma mark - AccountPickerConfirmationScreenActionDelegate

- (void)accountPickerConfirmationScreenViewControllerCancel:
    (AccountPickerConfirmationScreenViewController*)viewController {
  [_delegate accountPickerConfirmationScreenCoordinatorCancel:self];
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

- (void)accountPickerConfirmationScreenViewControllerAddAccountAndSignin:
    (AccountPickerConfirmationScreenViewController*)viewController {
  [_delegate accountPickerConfirmationScreenCoordinatorOpenAddAccount:self];
}

@end
