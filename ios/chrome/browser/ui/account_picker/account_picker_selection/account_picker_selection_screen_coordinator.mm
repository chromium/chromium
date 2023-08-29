// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/account_picker/account_picker_selection/account_picker_selection_screen_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_logger.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_selection/account_picker_selection_screen_mediator.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_selection/account_picker_selection_screen_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_selection/account_picker_selection_screen_view_controller.h"

@interface AccountPickerSelectionScreenCoordinator () <
    AccountPickerSelectionScreenTableViewControllerActionDelegate>

@end

@implementation AccountPickerSelectionScreenCoordinator {
  __strong AccountPickerSelectionScreenViewController*
      _accountListViewController;

  __strong AccountPickerSelectionScreenMediator* _mediator;
}

- (void)startWithSelectedIdentity:(id<SystemIdentity>)selectedIdentity {
  [super start];
  [_logger logAccountPickerSelectionScreenOpened];
  _mediator = [[AccountPickerSelectionScreenMediator alloc]
      initWithSelectedIdentity:selectedIdentity
         accountManagerService:ChromeAccountManagerServiceFactory::
                                   GetForBrowserState(
                                       self.browser->GetBrowserState())];

  _accountListViewController =
      [[AccountPickerSelectionScreenViewController alloc] init];
  _accountListViewController.modelDelegate = _mediator;
  _mediator.consumer = _accountListViewController.consumer;
  _accountListViewController.actionDelegate = self;
  _accountListViewController.layoutDelegate = self.layoutDelegate;
  [_accountListViewController view];
}

- (void)stop {
  [super stop];
  [_mediator disconnect];
  _mediator = nil;
  _accountListViewController = nil;
  [_logger logAccountPickerSelectionScreenClosed];
  _logger = nil;
}

- (void)dealloc {
  // TODO(crbug.com/1454777)
  DUMP_WILL_BE_CHECK(!_mediator);
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
                 didSelectIdentityWithGaiaID:(NSString*)gaiaID {
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  id<SystemIdentity> identity = accountManagerService->GetIdentityWithGaiaID(
      base::SysNSStringToUTF8(gaiaID));
  DCHECK(identity);
  _mediator.selectedIdentity = identity;
  [self.delegate accountPickerSelectionScreenCoordinatorIdentitySelected:self];
}

- (void)accountPickerListTableViewControllerDidTapOnAddAccount:
    (AccountPickerSelectionScreenTableViewController*)viewController {
  [self.delegate accountPickerSelectionScreenCoordinatorOpenAddAccount:self];
}

- (void)showManagementHelpPage {
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:GURL(kManagementLearnMoreURL)];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler closeSettingsUIAndOpenURL:command];
}

@end
