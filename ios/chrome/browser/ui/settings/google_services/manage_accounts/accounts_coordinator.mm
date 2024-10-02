// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/legacy_accounts_table_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface AccountsCoordinator () <AccountsMediatorDelegate,
                                   SettingsNavigationControllerDelegate,
                                   SignoutActionSheetCoordinatorDelegate>
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

  // Modal alert for sign out.
  SignoutActionSheetCoordinator* _signoutCoordinator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                 closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount {
  DCHECK(browser);
  DCHECK(!browser->GetProfile()->IsOffTheRecord());

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
  DCHECK(!browser->GetProfile()->IsOffTheRecord());

  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _closeSettingsOnAddAccount = closeSettingsOnAddAccount;
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  base::RecordAction(base::UserMetricsAction("Signin_AccountsTableView_Open"));
  ProfileIOS* profile = self.browser->GetProfile();
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  _mediator = [[AccountsMediator alloc]
        initWithSyncService:syncService
      accountManagerService:ChromeAccountManagerServiceFactory::GetForProfile(
                                profile)
                authService:AuthenticationServiceFactory::GetForProfile(profile)
            identityManager:IdentityManagerFactory::GetForProfile(profile)];

  if (base::FeatureList::IsEnabled(kIdentityDiscAccountMenu) &&
      !syncService->HasSyncConsent()) {
    AccountsTableViewController* viewController =
        [[AccountsTableViewController alloc]
            initWithOfferSignout:self.showSignoutButton];
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
  [_signoutCoordinator stop];
  _signoutCoordinator = nil;
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
  if (_closeSettingsOnAddAccount) {
    [base::apple::ObjCCastStrict<SettingsNavigationController>(
        _viewController.navigationController)
        popViewControllerOrCloseSettingsAnimated:YES];
  } else {
    [_viewController.navigationController dismissViewControllerAnimated:YES
                                                             completion:nil];
  }
  [self stop];
}

- (void)settingsWasDismissed {
  [self stop];
}

#pragma mark - SignoutActionSheetCoordinatorDelegate

- (void)signoutActionSheetCoordinatorPreventUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  [_viewController preventUserInteraction];
}

- (void)signoutActionSheetCoordinatorAllowUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  [_viewController allowUserInteraction];
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
                  base::RecordAction(base::UserMetricsAction(
                      "Signin_AccountsTableView_AccountDetail_RemoveAccount_"
                      "Confirmed"));
                  [weakSelf removeAccountDialogConfirmedWithIdentity:identity];
                }
                 style:UIAlertActionStyleDestructive];
  [_confirmRemoveIdentityAlertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^() {
                  base::RecordAction(base::UserMetricsAction(
                      "Signin_AccountsTableView_AccountDetail_RemoveAccount_"
                      "ConfirmationCancelled"));
                  [weakSelf handleAlertCoordinatorCancel];
                }
                 style:UIAlertActionStyleCancel];

  [_confirmRemoveIdentityAlertCoordinator start];
}

- (void)showAddAccountToDevice {
  [_viewController preventUserInteraction];
  __weak __typeof(self) weakSelf = self;
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
               identity:nil
            accessPoint:AccessPoint::ACCESS_POINT_SETTINGS
            promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO
               callback:^(SigninCoordinatorResult result,
                          SigninCompletionInfo* completionInfo) {
                 [weakSelf addAccountToDeviceCompleted];
               }];
  [HandlerForProtocol(self.browser->GetCommandDispatcher(), ApplicationCommands)
              showSignin:command
      baseViewController:_viewController];
}

- (void)signOutWithItemView:(UIView*)itemView {
  DCHECK(!_signoutCoordinator);
  _signoutCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                            rect:itemView.bounds
                            view:itemView
        forceSnackbarOverToolbar:NO
                      withSource:signin_metrics::ProfileSignout::
                                     kUserClickedSignoutSettings];
  __weak __typeof(self) weakSelf = self;
  _signoutCoordinator.signoutCompletion = ^(BOOL success) {
    [weakSelf handleSignOutCompleted:success];
  };
  _signoutCoordinator.delegate = self;
  [_signoutCoordinator start];
}

#pragma mark - Private

- (void)removeAccountDialogConfirmedWithIdentity:(id<SystemIdentity>)identity {
  [self dismissConfirmRemoveIdentityAlertCoordinator];
  NSArray<id<SystemIdentity>>* allIdentities =
      ChromeAccountManagerServiceFactory::GetForProfile(
          self.browser->GetProfile())
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
  ProfileIOS* profile = self.browser->GetProfile();
  if (!AuthenticationServiceFactory::GetForProfile(profile)->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // If there is no signed-in account after identity removal, then the primary
    // identity was removed, and there is no signed-in account at this stage.
    [self closeSettings];
    return;
  }
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

- (void)addAccountToDeviceCompleted {
  [_viewController allowUserInteraction];
  if (_closeSettingsOnAddAccount) {
    [self closeSettings];
  }
}

- (void)handleSignOutCompleted:(BOOL)success {
  [_signoutCoordinator stop];
  _signoutCoordinator = nil;
  if (!success) {
    return;
  }
  ProfileIOS* profile = self.browser->GetProfile();
  CHECK(
      !AuthenticationServiceFactory::GetForProfile(profile)->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin));
  [self closeSettings];
}

@end
