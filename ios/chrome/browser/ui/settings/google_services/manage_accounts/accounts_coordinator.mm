// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"

@interface AccountsCoordinator () <SettingsNavigationControllerDelegate>
@end

@implementation AccountsCoordinator {
  // Mediator.
  AccountsMediator* _mediator;

  // View controller.
  AccountsTableViewController* _viewController;

  BOOL _closeSettingsOnAddAccount;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                 closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount {
  DCHECK(browser);
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());

  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _closeSettingsOnAddAccount = closeSettingsOnAddAccount;
  }
  return self;
}

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                       closeSettingsOnAddAccount:
                           (BOOL)closeSettingsOnAddAccount {
  DCHECK(browser);
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());

  if (self = [super initWithBaseViewController:navigationController
                                       browser:browser]) {
    _closeSettingsOnAddAccount = closeSettingsOnAddAccount;
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  base::RecordAction(base::UserMetricsAction("Signin_AccountsTableView_Open"));
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  _mediator = [[AccountsMediator alloc]
        initWithSyncService:SyncServiceFactory::GetForBrowserState(browserState)
      accountManagerService:ChromeAccountManagerServiceFactory::
                                GetForBrowserState(browserState)
                authService:AuthenticationServiceFactory::GetForBrowserState(
                                browserState)
            identityManager:IdentityManagerFactory::GetForBrowserState(
                                browserState)];

  AccountsTableViewController* viewController =
      [[AccountsTableViewController alloc]
                              initWithBrowser:self.browser
                    closeSettingsOnAddAccount:_closeSettingsOnAddAccount
                   applicationCommandsHandler:HandlerForProtocol(
                                                  self.browser
                                                      ->GetCommandDispatcher(),
                                                  ApplicationCommands)
          signoutDismissalByParentCoordinator:
              self.signoutDismissalByParentCoordinator];
  _viewController = viewController;
  _mediator.consumer = viewController;
  _viewController.modelIdentityDataSource = _mediator;

  if (_baseNavigationController) {
    [self.baseNavigationController pushViewController:viewController
                                             animated:YES];
  } else {
    SettingsNavigationController* navigationController =
        [[SettingsNavigationController alloc]
            initWithRootViewController:_viewController
                               browser:self.browser
                              delegate:self];
    UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:self
                             action:@selector(closeSettings)];
    doneButton.accessibilityIdentifier = kSettingsAccountsTableViewDoneButtonId;
    _viewController.navigationItem.rightBarButtonItem = doneButton;
    [self.baseViewController presentViewController:navigationController
                                          animated:YES
                                        completion:nil];
  }
}

- (void)stop {
  [super stop];
  _viewController.modelIdentityDataSource = nil;
  _viewController = nil;
  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  base::RecordAction(base::UserMetricsAction("Signin_AccountsTableView_Close"));
  [_viewController settingsWillBeDismissed];
  [_viewController.navigationController dismissViewControllerAnimated:YES
                                                           completion:nil];
  [self stop];
}

- (void)settingsWasDismissed {
  [self stop];
}

@end
