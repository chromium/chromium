// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller.h"

@interface AccountsCoordinator () <SettingsNavigationControllerDelegate>
@end

@implementation AccountsCoordinator {
  // The navigation controller to use only when presenting the
  // Accounts view modally.
  SettingsNavigationController* _navigationControllerInModalView;

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

  if (_baseNavigationController) {
    [self.baseNavigationController pushViewController:viewController
                                             animated:YES];
  } else {
    SettingsNavigationController* navigationController =
        [[SettingsNavigationController alloc]
            initWithRootViewController:viewController
                               browser:self.browser
                              delegate:self];
    _navigationControllerInModalView = navigationController;
    [self.baseViewController presentViewController:navigationController
                                          animated:YES
                                        completion:nil];
  }
}

- (void)stop {
  [super stop];
  _viewController = nil;
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [_viewController.navigationController dismissViewControllerAnimated:YES
                                                           completion:nil];
  [_viewController settingsWillBeDismissed];
  [self stop];
}

- (void)settingsWasDismissed {
  [self stop];
}

@end
