// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_mediator.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_mediator_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_view_controller.h"
#import "ios/chrome/browser/authentication/add_account_signin/coordinator/add_account_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"

@interface AccountPickerSelectionScreenCoordinator () <
    AccountPickerSelectionScreenMediatorDelegate,
    AccountPickerSelectionScreenTableViewControllerActionDelegate>

@end

@implementation AccountPickerSelectionScreenCoordinator {
  __strong AccountPickerSelectionScreenViewController*
      _accountListViewController;
  AddAccountSigninCoordinator* _addAccountSigninCoordinator;
  signin_metrics::AccessPoint _accessPoint;
  // The identity to display when the view is started.
  id<SystemIdentity> _selectedIdentity;

  __strong AccountPickerSelectionScreenMediator* _mediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          selectedIdentity:(id<SystemIdentity>)selectedIdentity
                               accessPoint:
                                   (signin_metrics::AccessPoint)accessPoint {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CHECK(selectedIdentity, base::NotFatalUntil::M155);
    _accessPoint = accessPoint;
    _selectedIdentity = selectedIdentity;
  }
  return self;
}

- (void)start {
  [super start];
  _mediator = [[AccountPickerSelectionScreenMediator alloc]
      initWithSelectedIdentity:_selectedIdentity
               identityManager:IdentityManagerFactory::GetForProfile(
                                   self.profile)
         accountManagerService:ChromeAccountManagerServiceFactory::
                                   GetForProfile(self.profile)
         authenticationService:AuthenticationServiceFactory::GetForProfile(
                                   self.profile)];

  _accountListViewController =
      [[AccountPickerSelectionScreenViewController alloc] init];
  _accountListViewController.modelDelegate = _mediator;
  _mediator.consumer = _accountListViewController.consumer;
  _mediator.delegate = self;
  _accountListViewController.actionDelegate = self;
  _accountListViewController.layoutDelegate = self.layoutDelegate;
  [_accountListViewController view];
}

- (void)stop {
  [super stop];
  [_mediator disconnect];
  [self stopAddAccountSigninCoordinator];
  _mediator.delegate = nil;
  _selectedIdentity = nil;
  _mediator = nil;
  _accountListViewController = nil;
}

- (void)dealloc {
  CHECK(!_mediator, base::NotFatalUntil::M151);
}

#pragma mark - Properties

- (UIViewController*)viewController {
  return _accountListViewController;
}

- (id<SystemIdentity>)selectedIdentity {
  return _mediator.selectedIdentity;
}

#pragma mark - AccountPickerSelectionScreenTableViewControllerActionDelegate

- (void)accountPickerListTableViewController:
            (AccountPickerSelectionScreenTableViewController*)viewController
                 didSelectIdentityWithGaiaID:(const GaiaId&)gaiaID {
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(self.profile);

  id<SystemIdentity> identity =
      accountManagerService->GetIdentityOnDeviceWithGaiaID(gaiaID);
  DCHECK(identity);
  _mediator.selectedIdentity = identity;
  [self.delegate accountPickerSelectionScreenCoordinatorIdentitySelected:self];
}

- (void)accountPickerListTableViewControllerDidTapOnAddAccount:
    (AccountPickerSelectionScreenTableViewController*)viewController {
  if (_addAccountSigninCoordinator.viewWillPersist) {
    return;
  }
  [_addAccountSigninCoordinator stop];
  SigninContextStyle contextStyle = SigninContextStyle::kDefault;
  auto promoAction = signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  _addAccountSigninCoordinator = [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                    contextStyle:contextStyle
                     accessPoint:_accessPoint
                     promoAction:promoAction
                    signinIntent:AddAccountSigninIntent::kAddAccount
                  prefilledEmail:nil
            continuationProvider:DoNothingContinuationProvider()];
  __weak __typeof(self) weakSelf = self;
  _addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        [weakSelf addAccountDoneWithSigninCoordinator:coordinator
                                             identity:identity];
      };
  [_addAccountSigninCoordinator start];
}

- (void)addAccountDoneWithSigninCoordinator:(SigninCoordinator*)coordinator
                                   identity:(id<SystemIdentity>)identity {
  CHECK_EQ(coordinator, _addAccountSigninCoordinator,
           base::NotFatalUntil::M155);
  [self stopAddAccountSigninCoordinator];
  if (identity) {
    _mediator.selectedIdentity = identity;
    [self.delegate
        accountPickerSelectionScreenCoordinatorIdentitySelected:self];
  }
}

- (void)showManagementHelpPage {
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:GURL(kManagementLearnMoreURL)];
  id<SceneCommands> handler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), SceneCommands);
  [handler closePresentedViewsAndOpenURL:command];
}

#pragma mark - AccountPickerSelectionScreenMediatorDelegate

- (void)accountPickerSelectionScreenMediatorWantsToBeStopped:
    (AccountPickerSelectionScreenMediator*)mediator {
  CHECK_EQ(mediator, _mediator, base::NotFatalUntil::M151);
  [self.delegate accountPickerSelectionScreenCoordinatorWantsToBeStopped:self];
}

#pragma mark - Private

- (void)stopAddAccountSigninCoordinator {
  [_addAccountSigninCoordinator stop];
  _addAccountSigninCoordinator = nil;
}

@end
