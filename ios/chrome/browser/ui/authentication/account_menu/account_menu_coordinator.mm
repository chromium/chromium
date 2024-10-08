// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/ui/authentication/account_menu/account_menu_view_controller.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/scoped_ui_blocker/scoped_ui_blocker.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_coordinator.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_root_view_controlling.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AccountMenuCoordinator () <AccountMenuMediatorDelegate,
                                      UIAdaptivePresentationControllerDelegate>

// The view controller.
@property(nonatomic, strong) AccountMenuViewController* viewController;
// The mediator.
@property(nonatomic, strong) AccountMenuMediator* mediator;

@end

@implementation AccountMenuCoordinator {
  UINavigationController* _navigationController;
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<signin::IdentityManager> _identityManager;
  raw_ptr<PrefService> _prefService;
  // Dismiss callback for account details view.
  SystemIdentityManager::DismissViewCallback
      _accountDetailsControllerDismissCallback;
  // The coordinators for the "Edit account list"
  AccountsCoordinator* _accountsCoordinator;
  // The coordinator for the action sheet to sign out.
  SignoutActionSheetCoordinator* _signoutActionSheetCoordinator;
  raw_ptr<syncer::SyncService> _syncService;
  SyncEncryptionTableViewController* _syncEncryptionTableViewController;
  SyncEncryptionPassphraseTableViewController*
      _syncEncryptionPassphraseTableViewController;
  // ApplicationCommands handler.
  id<ApplicationCommands> _applicationHandler;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // BrowserCoordinatorCommands to show the activity indicator when the view
  // controller is dismissed.
  id<BrowserCoordinatorCommands> _browserCoordinatorCommands;
  // Callback to hide the activity overlay.
  base::ScopedClosureRunner _activityOverlayCallback;

  // Block the UI when the identity removal or switch is in progress.
  std::unique_ptr<ScopedUIBlocker> _UIBlocker;
}

- (void)dealloc {
  CHECK(!_mediator);
}

- (void)start {
  [super start];

  ProfileIOS* profile = self.browser->GetProfile();
  _syncService = SyncServiceFactory::GetForProfile(profile);
  _authenticationService = AuthenticationServiceFactory::GetForProfile(profile);
  _accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);
  _identityManager = IdentityManagerFactory::GetForProfile(profile);
  _prefService = profile->GetPrefs();
  _applicationHandler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                           ApplicationCommands);

  _browserCoordinatorCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowserCoordinatorCommands);

  _viewController = [[AccountMenuViewController alloc]
      initWithStyle:UITableViewStyleInsetGrouped];

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];

  _navigationController.modalPresentationStyle = UIModalPresentationPopover;
  _navigationController.popoverPresentationController.sourceView =
      self.anchorView;
  _navigationController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionUp;
  _navigationController.presentationController.delegate = self;

  PrefService* prefs = profile->GetPrefs();

  _mediator =
      [[AccountMenuMediator alloc] initWithSyncService:_syncService
                                 accountManagerService:_accountManagerService
                                           authService:_authenticationService
                                       identityManager:_identityManager
                                                 prefs:prefs];
  _mediator.delegate = self;
  _mediator.consumer = _viewController;
  _viewController.mutator = _mediator;
  _viewController.dataSource = _mediator;
  [_viewController setUpBottomSheetPresentationController];
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  // TODO(crbug.com/336719423): Change condition to CHECK(_mediator). But
  // first inform the parent coordinator at didTapClose that this view was
  // dismissed.
  if (!_mediator) {
    return;
  }
  if (!_accountDetailsControllerDismissCallback.is_null()) {
    std::move(_accountDetailsControllerDismissCallback).Run(/*animated=*/false);
  }
  // Stopping all potentially open children views.
  [self stopSignoutActionSheetCoordinator];
  [self stopAccountsCoordinator];
  _activityOverlayCallback.RunAndReset();
  [_syncEncryptionPassphraseTableViewController settingsWillBeDismissed];
  _syncEncryptionPassphraseTableViewController = nil;
  [_syncEncryptionTableViewController settingsWillBeDismissed];
  _syncEncryptionTableViewController = nil;

  // Sets to nil the account menu objects.
  [self dismissTheViewController];
  [_mediator disconnect];
  _mediator.delegate = nil;
  _mediator = nil;

  // Sets the service to nil.
  _authenticationService = nil;
  _browserCoordinatorCommands = nil;
  _identityManager = nil;
  _prefService = nil;
  _applicationHandler = nil;
  _syncService = nullptr;
  _accountManagerService = nullptr;

  [self unblockOtherScene];
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self.delegate acountMenuCoordinatorShouldStop:self];
  _navigationController = nil;
}

#pragma mark - AccountMenuMediatorDelegate

- (void)didTapManageYourGoogleAccount {
  __weak __typeof(self) weakSelf = self;
  _accountDetailsControllerDismissCallback =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentAccountDetailsController(
              _authenticationService->GetPrimaryIdentity(
                  signin::ConsentLevel::kSignin),
              _viewController,
              /*animated=*/YES,
              base::BindOnce(
                  [](__typeof(self) strongSelf) {
                    [strongSelf resetAccountDetailsControllerDismissCallback];
                  },
                  weakSelf));
}

- (void)didTapEditAccountList {
  _accountsCoordinator = [[AccountsCoordinator alloc]
      initWithBaseViewController:_navigationController
                         browser:self.browser
       closeSettingsOnAddAccount:NO];
  _accountsCoordinator.signoutDismissalByParentCoordinator = YES;
  [_accountsCoordinator start];
}

- (void)signOutFromTargetRect:(CGRect)targetRect
                     callback:(void (^)(BOOL))callback {
  if (!_authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // This could happen in very rare cases, if the account somehow got removed
    // after the accounts menu was created.
    return;
  }
  _signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                            rect:targetRect
                            view:_viewController.view
        forceSnackbarOverToolbar:YES
                      withSource:signin_metrics::ProfileSignout::
                                     kUserClickedSignoutInAccountMenu];
  __weak __typeof(self) weakSelf = self;
  _signoutActionSheetCoordinator.signoutCompletion = ^(BOOL success) {
    [weakSelf stopSignoutActionSheetCoordinator];
    if (success) {
      [weakSelf.delegate acountMenuCoordinatorShouldStop:weakSelf];
    }
    if (callback) {
      callback(success);
    }
  };
  [_signoutActionSheetCoordinator start];
}

- (void)didTapAddAccount:(ShowSigninCommandCompletionCallback)callback {
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kAddAccount
               identity:nil
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU
            promoAction:signin_metrics::PromoAction::
                            PROMO_ACTION_NO_SIGNIN_PROMO
               callback:callback];
  [_applicationHandler showSignin:command
               baseViewController:_navigationController];
}

- (void)mediatorWantsToBeDismissed:(AccountMenuMediator*)mediator {
  CHECK_EQ(mediator, _mediator);
  [self.delegate acountMenuCoordinatorShouldStop:self];
}

// Requests to dismiss the account menu view. Keeps the coordinator open and
// show a spinner instead.
- (void)mediatorWantsToDismissTheView:(AccountMenuMediator*)mediator {
  CHECK_EQ(mediator, _mediator);
  CHECK(_viewController);
  [self dismissTheViewController];
  _activityOverlayCallback = [_browserCoordinatorCommands showActivityOverlay];
}

- (void)triggerAccountSwitchWithTargetRect:(CGRect)targetRect
                               newIdentity:(id<SystemIdentity>)newIdentity
           viewWillBeDismissedAfterSignout:(BOOL)viewWillBeDismissedAfterSignout
                    userDecisionCompletion:(void (^)())userDecisionCompletion
                          signInCompletion:(ShowSigninCommandCompletionCallback)
                                               signInCompletion {
  CHECK(
      _authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSignin),
      base::NotFatalUntil::M130)
      << "There must be a signed-in account to view the menu and be able to "
         "switch accounts.";

  [_applicationHandler
      switchAccountWithBaseViewController:_navigationController
                              newIdentity:newIdentity
                                     rect:targetRect
                           rectAnchorView:_viewController.view
          viewWillBeDismissedAfterSignout:viewWillBeDismissedAfterSignout
                   userDecisionCompletion:userDecisionCompletion
                         signInCompletion:signInCompletion];
}

- (void)blockOtherScene {
  SceneState* sceneState = self.browser->GetSceneState();
  _UIBlocker = std::make_unique<ScopedUIBlocker>(sceneState);
}

- (void)unblockOtherScene {
  _UIBlocker.reset();
}

#pragma mark - SyncErrorSettingsCommandHandler

- (void)openPassphraseDialogWithModalPresentation:(BOOL)presentModally {
  CHECK(presentModally);
  _syncEncryptionPassphraseTableViewController =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowser:self.browser];
  _syncEncryptionPassphraseTableViewController.presentModally = YES;
  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:_syncEncryptionPassphraseTableViewController];
  navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  [self configureHandlersForRootViewController:
            _syncEncryptionPassphraseTableViewController];
  [_navigationController presentViewController:navigationController
                                      animated:YES
                                    completion:nil];
}

- (void)openTrustedVaultReauthForFetchKeys {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  trusted_vault::SecurityDomainId securityDomainID =
      trusted_vault::SecurityDomainId::kChromeSync;
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU;
  [applicationCommands
      showTrustedVaultReauthForFetchKeysFromViewController:_navigationController
                                          securityDomainID:securityDomainID
                                                   trigger:trigger
                                               accessPoint:accessPoint];
}

- (void)openTrustedVaultReauthForDegradedRecoverability {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  trusted_vault::SecurityDomainId securityDomainID =
      trusted_vault::SecurityDomainId::kChromeSync;
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU;
  [applicationCommands
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
          _navigationController
                                                       securityDomainID:
                                                           securityDomainID
                                                                trigger:trigger
                                                            accessPoint:
                                                                accessPoint];
}

- (void)openMDMErrodDialogWithSystemIdentity:(id<SystemIdentity>)identity {
  _authenticationService->ShowMDMErrorDialogForIdentity(identity);
}

- (void)openPrimaryAccountReauthDialog {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  ShowSigninCommand* signinCommand = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kPrimaryAccountReauth
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_ACCOUNT_MENU];
  [applicationCommands showSignin:signinCommand
               baseViewController:_navigationController];
}

#pragma mark - Private

- (void)stopAccountsCoordinator {
  [_accountsCoordinator stop];
  _accountsCoordinator = nil;
}

- (void)resetAccountDetailsControllerDismissCallback {
  _accountDetailsControllerDismissCallback.Reset();
}

- (void)configureHandlersForRootViewController:
    (id<SettingsRootViewControlling>)controller {
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  controller.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  controller.browserHandler = HandlerForProtocol(dispatcher, BrowserCommands);
  controller.settingsHandler = HandlerForProtocol(dispatcher, SettingsCommands);
  controller.snackbarHandler = HandlerForProtocol(dispatcher, SnackbarCommands);
}

- (void)stopSignoutActionSheetCoordinator {
  [_signoutActionSheetCoordinator stop];
  _signoutActionSheetCoordinator = nil;
}

// Dismisses the view controller.
- (void)dismissTheViewController {
  _mediator.consumer = nil;
  _viewController.dataSource = nil;
  _viewController.mutator = nil;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  _navigationController.delegate = nil;
  _navigationController = nil;
  _viewController = nil;
}

@end
