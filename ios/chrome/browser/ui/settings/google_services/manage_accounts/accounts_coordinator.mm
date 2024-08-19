// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/legacy_accounts_table_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface AccountsCoordinator () <AccountsMediatorDelegate,
                                   SettingsNavigationControllerDelegate>
@end

@implementation AccountsCoordinator {
  // Mediator.
  AccountsMediator* _mediator;

  // The view controller.
  SettingsRootTableViewController<WithOverridableModelIdentityDataSource>*
      _viewController;

  BOOL _closeSettingsOnAddAccount;

  // Alert coordinator to confirm identity removal.
  AlertCoordinator* _confirmRemoveIdentityAlertCoordinator;

  // Block the UI when the identity removal is in progress.
  std::unique_ptr<ScopedUIBlocker> _UIBlocker;

  // Coordinator to display modal alerts to the user.
  AlertCoordinator* _errorAlertCoordinator;
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
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);
  _mediator = [[AccountsMediator alloc]
        initWithSyncService:syncService
      accountManagerService:ChromeAccountManagerServiceFactory::
                                GetForBrowserState(browserState)
                authService:AuthenticationServiceFactory::GetForBrowserState(
                                browserState)
            identityManager:IdentityManagerFactory::GetForBrowserState(
                                browserState)];

  if (base::FeatureList::IsEnabled(kIdentityDiscAccountMenu) &&
      !syncService->HasSyncConsent()) {
    AccountsTableViewController* viewController =
        [[AccountsTableViewController alloc]
            initWithCloseSettingsOnAddAccount:_closeSettingsOnAddAccount
                   applicationCommandsHandler:HandlerForProtocol(
                                                  self.browser
                                                      ->GetCommandDispatcher(),
                                                  ApplicationCommands)];
    _viewController = viewController;
    _mediator.consumer = viewController;
    _mediator.delegate = self;
    _viewController.modelIdentityDataSource = _mediator;
    AccountsTableViewController* accountsTableViewController =
        base::apple::ObjCCast<AccountsTableViewController>(_viewController);
    accountsTableViewController.mutator = _mediator;
  } else {
    LegacyAccountsTableViewController* viewController =
        [[LegacyAccountsTableViewController alloc]
                                initWithBrowser:self.browser
                      closeSettingsOnAddAccount:_closeSettingsOnAddAccount
                     applicationCommandsHandler:
                         HandlerForProtocol(
                             self.browser->GetCommandDispatcher(),
                             ApplicationCommands)
            signoutDismissalByParentCoordinator:
                self.signoutDismissalByParentCoordinator];
    _viewController = viewController;
    _mediator.consumer = viewController;
    _viewController.modelIdentityDataSource = _mediator;
  }

  if (_baseNavigationController) {
    [self.baseNavigationController pushViewController:_viewController
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
  AccountsTableViewController* accountsTableViewController =
      base::apple::ObjCCast<AccountsTableViewController>(_viewController);
  if (accountsTableViewController) {
    accountsTableViewController.mutator = nil;
  }
  _viewController.modelIdentityDataSource = nil;
  _viewController = nil;
  _mediator.consumer = nil;
  [_mediator disconnect];
  _mediator = nil;
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  base::RecordAction(base::UserMetricsAction("Signin_AccountsTableView_Close"));
  if ([_viewController respondsToSelector:@selector(settingsWillBeDismissed)]) {
    [_viewController performSelector:@selector(settingsWillBeDismissed)];
  }
  [_viewController.navigationController dismissViewControllerAnimated:YES
                                                           completion:nil];
  [self stop];
}

- (void)settingsWasDismissed {
  [self stop];
}

#pragma mark - AccountsMediatorDelegate

- (void)handleRemoveIdentity:(id<SystemIdentity>)identity
                    itemView:(UIView*)itemView {
  _confirmRemoveIdentityAlertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                           title:nil
                         message:
                             l10n_util::GetNSString(
                                 IDS_IOS_REMOVE_ACCOUNT_CONFIRMATION_MESSAGE)
                            rect:itemView.frame
                            view:itemView];

  __weak __typeof(self) weakSelf = self;
  // TODO(crbug.com/349071402): Record actions.
  [_confirmRemoveIdentityAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_REMOVE_ACCOUNT_LABEL)
                action:^{
                  [weakSelf removeAccountDialogConfirmedWithIdentity:identity];
                }
                 style:UIAlertActionStyleDestructive];
  [_confirmRemoveIdentityAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^() {
                  [weakSelf handleAlertCoordinatorCancel];
                }
                 style:UIAlertActionStyleCancel];

  [_confirmRemoveIdentityAlertCoordinator start];
}

#pragma mark - Private

- (void)removeAccountDialogConfirmedWithIdentity:(id<SystemIdentity>)identity {
  [self dismissConfirmRemoveIdentityAlertCoordinator];
  NSArray<id<SystemIdentity>>* allIdentities =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->GetAllIdentities();
  if (![allIdentities containsObject:identity]) {
    // If the identity was removed by another way (another window, another app
    // or by gaia), there is nothing to do.
    return;
  }
  SceneState* sceneState = self.browser->GetSceneState();
  _UIBlocker = std::make_unique<ScopedUIBlocker>(sceneState);
  [_viewController preventUserInteraction];
  __weak __typeof(self) weakSelf = self;
  GetApplicationContext()->GetSystemIdentityManager()->ForgetIdentity(
      identity, base::BindOnce(
                    [](__typeof(self) strongSelf, NSError* error) {
                      if (error) {
                        LOG(ERROR) << "Failed to remove idenity.";
                        [strongSelf forgetIdentityFailedWithError:error];
                      }
                      [strongSelf forgetIdentityDone];
                    },
                    weakSelf));
}

- (void)forgetIdentityDone {
  _UIBlocker.reset();
  [_viewController allowUserInteraction];
}

- (void)forgetIdentityFailedWithError:(NSError*)error {
  DCHECK(error);
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock dismissAction = ^{
    [weakSelf dismissAlertCoordinator];
  };

  _errorAlertCoordinator =
      ErrorCoordinator(error, dismissAction, _viewController, self.browser);
  [_errorAlertCoordinator start];
}

- (void)dismissAlertCoordinator {
  [_errorAlertCoordinator stop];
  _errorAlertCoordinator = nil;
}

- (void)dismissConfirmRemoveIdentityAlertCoordinator {
  [_confirmRemoveIdentityAlertCoordinator stop];
  _confirmRemoveIdentityAlertCoordinator = nil;
}

- (void)handleAlertCoordinatorCancel {
  DCHECK(_confirmRemoveIdentityAlertCoordinator);
  [_confirmRemoveIdentityAlertCoordinator stop];
  _confirmRemoveIdentityAlertCoordinator = nil;
}

@end
