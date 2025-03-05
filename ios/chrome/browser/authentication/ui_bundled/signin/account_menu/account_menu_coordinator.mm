// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/check.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_mediator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/account_menu/account_menu_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/add_account_signin/add_account_signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/interruptible_chrome_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/policy/model/management_state.h"
#import "ios/chrome/browser/policy/ui_bundled/management_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_view_controlling.h"
#import "ios/chrome/browser/settings/ui_bundled/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/sync/sync_encryption_table_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/identity_snackbar/identity_snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface AccountMenuCoordinator () <AccountMenuMediatorDelegate,
                                      ManageAccountsCoordinatorDelegate,
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
  ManageAccountsCoordinator* _manageAccountsCoordinator;
  // The coordinator for the action sheet to sign out.
  SignoutActionSheetCoordinator* _signoutActionSheetCoordinator;
  raw_ptr<syncer::SyncService> _syncService;
  SyncEncryptionTableViewController* _syncEncryptionTableViewController;
  SyncEncryptionPassphraseTableViewController*
      _syncEncryptionPassphraseTableViewController;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // Callback to hide the activity overlay.
  base::ScopedClosureRunner _activityOverlayCallback;
  // The child signin coordinator if it’s open. It may be presented by the
  // Manage Account’s coordinator view controller.
  SigninCoordinator* _signinCoordinator;
  // Clicked view, used to anchor the menu to it when using
  // UIModalPresentationPopover mode
  UIView* _anchorView;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                anchorView:(UIView*)anchorView {
  self = [super
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:signin_metrics::AccessPoint::kAccountMenu];
  if (self) {
    _anchorView = anchorView;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_mediator);
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

  _viewController = [[AccountMenuViewController alloc]
      initWithHideEllipsisMenu:IdentityDiscAccountMenuEnabledWithoutEllipsis()];

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];

  if (_anchorView) {
    _navigationController.modalPresentationStyle = UIModalPresentationPopover;
    _navigationController.popoverPresentationController.sourceView =
        _anchorView;
    _navigationController.popoverPresentationController
        .permittedArrowDirections = UIPopoverArrowDirectionUp;
  } else {
    // If no anchor view was provided, fall back to a form sheet. For narrow
    // width devices (i.e. iPhone) it's the same thing as a popover anyway, and
    // for regular width (i.e. iPad) it's a dialog centered in the window.
    _navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
  }
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
  [_syncEncryptionPassphraseTableViewController settingsWillBeDismissed];
  _syncEncryptionPassphraseTableViewController = nil;
  [_syncEncryptionTableViewController settingsWillBeDismissed];
  _syncEncryptionTableViewController = nil;

  // Sets to nil the account menu objects.
  [_mediator disconnect];
  _mediator.delegate = nil;
  _mediator = nil;

  // Sets the service to nil.
  _authenticationService = nil;
  _identityManager = nil;
  _prefService = nil;
  _syncService = nullptr;
  _accountManagerService = nullptr;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("Signin_AccountMenu_Dismissed_By_User"));
  [self runCompletionWithSigninResult:SigninCoordinatorResultCanceledByUser
                   completionIdentity:nil];
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

- (void)didTapManageAccounts {
  CHECK(!_manageAccountsCoordinator, base::NotFatalUntil::M133);
  _manageAccountsCoordinator = [[ManageAccountsCoordinator alloc]
      initWithBaseViewController:_navigationController
                         browser:self.browser
       closeSettingsOnAddAccount:NO];
  _manageAccountsCoordinator.delegate = self;
  _manageAccountsCoordinator.signoutDismissalByParentCoordinator = YES;
  [_manageAccountsCoordinator start];
}

- (void)didTapSettingsButton {
  // Close the account menu and open the Settings page.
  [self stopChildrenAndViewControllerWithAction:SigninCoordinatorInterrupt::
                                                    DismissWithAnimation];
  [self runCompletionWithSigninResult:SigninCoordinatorResultCanceledByUser
                   completionIdentity:nil];
  id<ApplicationCommands> applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationHandler showSettingsFromViewController:nil];
}

- (void)signOutFromTargetRect:(CGRect)targetRect
                   completion:(void (^)(BOOL))completion {
  if (!_authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // This could happen in very rare cases, if the account somehow got removed
    // after the accounts menu was created.
    return;
  }
  signin_metrics::ProfileSignout metricSignOut =
      signin_metrics::ProfileSignout::kUserClickedSignoutInAccountMenu;
  _signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                            rect:targetRect
                            view:_viewController.view
        forceSnackbarOverToolbar:YES
                      withSource:metricSignOut];
  __weak __typeof(self) weakSelf = self;
  _signoutActionSheetCoordinator.signoutCompletion = ^(BOOL success) {
    [weakSelf stopSignoutActionSheetCoordinator];
    if (completion) {
      completion(success);
    }
  };
  [_signoutActionSheetCoordinator start];
}

- (void)didTapAddAccountWithCompletion:
    (SigninCoordinatorCompletionCallback)completion {
  [self openAddAccountWithBaseViewController:_navigationController
                                  completion:completion];
}

- (void)mediatorWantsToBeDismissed:(AccountMenuMediator*)mediator
                        withResult:(SigninCoordinatorResult)signinResult
                    signedIdentity:(id<SystemIdentity>)signedIdentity {
  CHECK_EQ(mediator, _mediator);
  [self stopChildrenAndViewControllerWithAction:SigninCoordinatorInterrupt::
                                                    DismissWithAnimation];
  [self runCompletionWithSigninResult:signinResult
                   completionIdentity:signedIdentity];
}

- (AuthenticationFlow*)
    triggerSigninWithSystemIdentity:(id<SystemIdentity>)identity
                         anchorRect:(CGRect)anchorRect
                         completion:
                             (signin_ui::SigninCompletionCallback)completion {
  AuthenticationFlow* authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:self.browser
                      identity:identity
                   accessPoint:signin_metrics::AccessPoint::kAccountMenu
             postSignInActions:PostSignInActionSet()
      presentingViewController:_navigationController
                    anchorView:_viewController.view
                    anchorRect:anchorRect];

  [authenticationFlow
      startSignInWithCompletion:^(SigninCoordinatorResult result) {
        if (completion) {
          completion(result);
        }
      }];
  return authenticationFlow;
}

- (void)triggerAccountSwitchSnackbarWithIdentity:
    (id<SystemIdentity>)systemIdentity {
  UIImage* avatar = _accountManagerService->GetIdentityAvatarWithIdentity(
      systemIdentity, IdentityAvatarSize::Regular);
  ManagementState managementState = GetManagementState(
      _identityManager, _authenticationService, _prefService);
  MDCSnackbarMessage* snackbarTitle = [[IdentitySnackbarMessage alloc]
      initWithName:systemIdentity.userGivenName
             email:systemIdentity.userEmail
            avatar:avatar
           managed:managementState.is_profile_managed()];
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<SnackbarCommands> snackbarCommandsHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);
  [snackbarCommandsHandler showSnackbarMessageOverBrowserToolbar:snackbarTitle];
}

#pragma mark - SyncErrorSettingsCommandHandler

- (void)openPassphraseDialogWithModalPresentation:(BOOL)presentModally {
  CHECK(presentModally);
  if (self.sceneState.isUIBlocked) {
    // This could occur due to race condition with multiple windows and
    // simultaneous taps. See crbug.com/368310663.
    return;
  }
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
  trusted_vault::SecurityDomainId securityDomainID =
      trusted_vault::SecurityDomainId::kChromeSync;
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kAccountMenu;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::kAccountMenu;
  SigninTrustedVaultDialogIntent intent =
      SigninTrustedVaultDialogIntentFetchKeys;
  _signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordinatorWithBaseViewController:
          _navigationController
                                                            browser:self.browser
                                                             intent:intent
                                                   securityDomainID:
                                                       securityDomainID
                                                            trigger:trigger
                                                        accessPoint:
                                                            accessPoint];
  [self startSigninCoordinatorWithCompletion:nil];
}

- (void)openTrustedVaultReauthForDegradedRecoverability {
  trusted_vault::SecurityDomainId securityDomainID =
      trusted_vault::SecurityDomainId::kChromeSync;
  syncer::TrustedVaultUserActionTriggerForUMA trigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kAccountMenu;
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::kAccountMenu;
  SigninTrustedVaultDialogIntent intent =
      SigninTrustedVaultDialogIntentDegradedRecoverability;
  _signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordinatorWithBaseViewController:
          _navigationController
                                                            browser:self.browser
                                                             intent:intent
                                                   securityDomainID:
                                                       securityDomainID
                                                            trigger:trigger
                                                        accessPoint:
                                                            accessPoint];
  [self startSigninCoordinatorWithCompletion:nil];
}

- (void)openMDMErrodDialogWithSystemIdentity:(id<SystemIdentity>)identity {
  _authenticationService->ShowMDMErrorDialogForIdentity(identity);
}

- (void)openPrimaryAccountReauthDialog {
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::kAccountMenu;
  signin_metrics::PromoAction promoAction =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  _signinCoordinator = [SigninCoordinator
      primaryAccountReauthCoordinatorWithBaseViewController:
          _navigationController
                                                    browser:self.browser
                                                accessPoint:accessPoint
                                                promoAction:promoAction];
  [self startSigninCoordinatorWithCompletion:nil];
}

#pragma mark - SigninCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterrupt)action
                 completion:(ProceduralBlock)completion {
  [self stopChildrenAndViewControllerWithAction:action];
  [self runCompletionWithSigninResult:SigninCoordinatorResultInterrupted
                   completionIdentity:nil];
  if (completion) {
    completion();
  }
}

#pragma mark - ManageAccountsCoordinatorDelegate

- (void)manageAccountsCoordinatorWantsToBeStopped:
    (ManageAccountsCoordinator*)coordinator {
  CHECK_EQ(coordinator, _manageAccountsCoordinator, base::NotFatalUntil::M133);
  [self stopManageAccountsCoordinator];
}

- (void)manageAccountsCoordinator:
            (ManageAccountsCoordinator*)manageAccountsCoordinator
    didRequestAddAccountWithBaseViewController:(UIViewController*)viewController
                                    completion:
                                        (SigninCoordinatorCompletionCallback)
                                            completion {
  CHECK_EQ(manageAccountsCoordinator, _manageAccountsCoordinator);
  [self openAddAccountWithBaseViewController:viewController
                                  completion:completion];
}

#pragma mark - Private

- (void)stopSigninCoordinator {
  [_signinCoordinator stop];
  _signinCoordinator = nil;
}

- (void)startSigninCoordinatorWithCompletion:
    (SigninCoordinatorCompletionCallback)completion {
  CHECK(_signinCoordinator);
  __weak __typeof(self) weakSelf = self;
  _signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult,
        id<SystemIdentity> signinCompletionIdentity) {
        [weakSelf
            signinCoordinatorCompletionWithSigninResult:signinResult
                                     completionIdentity:signinCompletionIdentity
                                             completion:completion];
      };
  [_signinCoordinator start];
}

// Opens the add account coordinator on top of `baseViewController`.
- (void)openAddAccountWithBaseViewController:baseViewController
                                  completion:
                                      (SigninCoordinatorCompletionCallback)
                                          completion {
  _signinCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:baseViewController
                                          browser:self.browser
                                      accessPoint:self.accessPoint];
  [self startSigninCoordinatorWithCompletion:completion];
}

// Clean up the add account coordinator.
- (void)
    signinCoordinatorCompletionWithSigninResult:
        (SigninCoordinatorResult)signinResult
                             completionIdentity:
                                 (id<SystemIdentity>)completionIdentity
                                     completion:
                                         (SigninCoordinatorCompletionCallback)
                                             completion {
  [self stopSigninCoordinator];
  if (completion) {
    completion(signinResult, completionIdentity);
  }
}

- (void)stopManageAccountsCoordinator {
  [_manageAccountsCoordinator stop];
  _manageAccountsCoordinator.delegate = nil;
  _manageAccountsCoordinator = nil;
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

// Stops all children, then dismiss the view controller. Executes
// `completion` synchronously.
- (void)stopChildrenAndViewControllerWithAction:
    (SigninCoordinatorInterrupt)action {
  // Stopping all potentially open children views.
  if (!_accountDetailsControllerDismissCallback.is_null()) {
    std::move(_accountDetailsControllerDismissCallback).Run(/*animated=*/false);
  }
  [self stopSignoutActionSheetCoordinator];
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock dismissAndCompletion = ^() {
    // Add Account coordinator should be stopped before the Manage Accounts
    // Coordinator, as the former may be presented by the latter.
    [weakSelf stopManageAccountsCoordinator];
    [weakSelf dismissViewControllerAction:action];
  };
  if (_signinCoordinator) {
    SigninCoordinatorInterrupt subviewAction =
        (action == SigninCoordinatorInterrupt::UIShutdownNoDismiss)
            ? SigninCoordinatorInterrupt::UIShutdownNoDismiss
            : SigninCoordinatorInterrupt::DismissWithoutAnimation;
    [_signinCoordinator interruptWithAction:subviewAction
                                 completion:dismissAndCompletion];
  } else {
    dismissAndCompletion();
  }
}

// Unplugs the view and navigation controller. Dismisses the navigation
// controller as specified by the action.
- (void)dismissViewControllerAction:(SigninCoordinatorInterrupt)action {
  if (!_navigationController) {
    // The view controller was already dismissed.
    return;
  }
  _activityOverlayCallback.RunAndReset();
  _mediator.consumer = nil;
  _viewController.dataSource = nil;
  _viewController.mutator = nil;
  UINavigationController* navigationController = _navigationController;
  _navigationController = nil;
  _viewController = nil;
  switch (action) {
    case SigninCoordinatorInterrupt::UIShutdownNoDismiss: {
      CHECK(!IsInterruptibleCoordinatorAlwaysDismissedEnabled(),
            base::NotFatalUntil::M136);
      break;
    }
    case SigninCoordinatorInterrupt::DismissWithoutAnimation: {
      [navigationController.presentingViewController
          dismissViewControllerAnimated:NO
                             completion:nil];
      break;
    }
    case SigninCoordinatorInterrupt::DismissWithAnimation: {
      [navigationController.presentingViewController
          dismissViewControllerAnimated:YES
                             completion:nil];
      break;
    }
  }
}

@end
