// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/change_profile_commands.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_mediator.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_mediator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/account_menu/ui/account_menu_view_controller.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/authentication_flow.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/reauth/signin_reauth_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_in_progress.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_view_controlling.h"
#import "ios/chrome/browser/settings/ui_bundled/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/sync/sync_encryption_table_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/animated_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Potentially shows an IPH, informing the user that they can find Settings in
// the overflow menu. The handler contains the logic for whether to actually
// show it.
void maybeShowSettingsIPH(Browser* browser) {
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<HelpCommands> helpCommandsHandler =
      HandlerForProtocol(dispatcher, HelpCommands);
  [helpCommandsHandler
      presentInProductHelpWithType:InProductHelpType::kSettingsInOverflowMenu];
}

}  // namespace

@interface AccountMenuCoordinator () <
    AccountMenuMediatorDelegate,
    SettingsNavigationControllerDelegate,
    SigninReauthCoordinatorDelegate,
    SyncErrorSettingsCommandHandler,
    SyncEncryptionPassphraseTableViewControllerPresentationDelegate,
    TrustedVaultReauthenticationCoordinatorDelegate,
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
  // Dismiss callback for account details view.
  SystemIdentityManager::DismissViewCallback
      _accountDetailsControllerDismissCallback;
  // The view controller for the "Edit account list"
  SettingsNavigationController* _manageAccountsNavigationController;
  // The coordinator for the action sheet to sign out.
  SignoutActionSheetCoordinator* _signoutActionSheetCoordinator;
  raw_ptr<syncer::SyncService> _syncService;
  SyncEncryptionPassphraseTableViewController*
      _syncEncryptionPassphraseTableViewController;
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // The child signin coordinator if it’s open.
  SigninCoordinator* _addAccountSigninCoordinator;
  // Reauth coordinator for the reauthentication flow if it's open.
  SigninReauthCoordinator* _reauthCoordinator;
  // Clicked view, used to anchor the menu to it when using
  // UIModalPresentationPopover mode
  UIView* _anchorView;
  // The access point from which this account menu was triggered.
  AccountMenuAccessPoint _accessPoint;
  // The URL which the the account menu was viewed from when
  // AccountMenuAccessPoint::kWeb.
  GURL _url;
  TrustedVaultReauthenticationCoordinator*
      _trustedVaultReauthenticationCoordinator;
  // While this value is set, the scene state considers the sign-in to be in
  // progress.
  std::unique_ptr<SigninInProgress> _signinInProgress;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                anchorView:(UIView*)anchorView
                               accessPoint:(AccountMenuAccessPoint)accessPoint
                                       URL:(const GURL&)url {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    // All authentication related work must be done in the regular profile, even
    // if started from incognito browser.
    CHECK_EQ(browser->type(), Browser::Type::kRegular,
             base::NotFatalUntil::M145);
    _accessPoint = accessPoint;
    _anchorView = anchorView;
    _accessPoint = accessPoint;
    _url = url;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!_mediator);
}

- (void)start {
  ProfileIOS* profile = self.profile;
  _identityManager = IdentityManagerFactory::GetForProfile(profile);
  BOOL has_primary_identity =
      _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  if (!has_primary_identity) {
    // The owner of this coordinator is supposed to have checked that there is a
    // primary identity. If not, the coordinator will request to be stopped
    // immediately. This is not a security issue, so we don’t want to CHECK.
    // However, this is not the optimal user experience, so we DUMP in order for
    // the owner of this coordinator to be edited so that it choses what to do
    // instead.
    DUMP_WILL_BE_NOTREACHED();
    [self.delegate accountMenuCoordinatorWantsToBeStopped:self];
    return;
  }

  [super start];

  _syncService = SyncServiceFactory::GetForProfile(profile);
  _authenticationService = AuthenticationServiceFactory::GetForProfile(profile);
  _accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);

  _viewController = [[AccountMenuViewController alloc]
      initWithHideEllipsisMenu:_accessPoint == AccountMenuAccessPoint::kWeb];

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

  id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         BrowserCoordinatorCommands);
  void (^prepareChangeProfile)() = ^() {
    [browserCoordinatorCommandsHandler closeCurrentTab];
  };

  _mediator =
      [[AccountMenuMediator alloc] initWithSyncService:_syncService
                                 accountManagerService:_accountManagerService
                                           authService:_authenticationService
                                       identityManager:_identityManager
                                                 prefs:prefs
                                           accessPoint:_accessPoint
                                                   URL:_url
                                  prepareChangeProfile:prepareChangeProfile];
  _mediator.delegate = self;
  _mediator.syncErrorSettingsCommandHandler = self;
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
  [self stopTrustedVaultReauthenticationCoordinator];
  [self stopChildrenAndViewControllerAnimated:NO];
  [_syncEncryptionPassphraseTableViewController settingsWillBeDismissed];
  _syncEncryptionPassphraseTableViewController = nil;

  // Sets to nil the account menu objects.
  [_mediator disconnect];
  _mediator.delegate = nil;
  _mediator = nil;

  // Sets the service to nil.
  _authenticationService = nil;
  _identityManager = nil;
  _syncService = nullptr;
  _accountManagerService = nullptr;
  [super stop];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return !_mediator.userInteractionsBlocked;
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("Signin_AccountMenu_Dismissed_By_User"));
  // `self` may be deallocated when -accountMenuCoordinatorWantsToBeStopped
  // returns. We must access the browser first.
  Browser* browser = self.browser;
  [self.delegate accountMenuCoordinatorWantsToBeStopped:self];
  maybeShowSettingsIPH(browser);
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
  CHECK(!_manageAccountsNavigationController);
  _manageAccountsNavigationController = [SettingsNavigationController
             accountsControllerForBrowser:self.browser
                       baseViewController:_navigationController
                                 delegate:self
                closeSettingsOnAddAccount:NO
                        showSignoutButton:NO
                           showDoneButton:YES
      signoutDismissalByParentCoordinator:YES];
}

- (void)signOutFromTargetRect:(CGRect)targetRect
                   completion:(signin_ui::SignoutCompletionCallback)completion {
  if (!_authenticationService->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // This could happen in very rare cases, if the account somehow got removed
    // after the accounts menu was created.
    return;
  }
  constexpr signin_metrics::ProfileSignout metricSignOut =
      signin_metrics::ProfileSignout::kUserClickedSignoutInAccountMenu;

  __weak __typeof(self) weakSelf = self;
  _signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                            rect:targetRect
                            view:_viewController.view
        forceSnackbarOverToolbar:YES
                      withSource:metricSignOut
                      completion:^(BOOL success, SceneState* scene_state) {
                        [weakSelf stopSignoutActionSheetCoordinator];
                        if (completion) {
                          completion(success, scene_state);
                        }
                      }];
  [_signoutActionSheetCoordinator start];
}

- (void)didTapAddAccount {
  auto style = SigninContextStyle::kDefault;
  auto accessPoint = signin_metrics::AccessPoint::kAccountMenuSwitchAccount;
  if (_addAccountSigninCoordinator.viewWillPersist) {
    return;
  }
  [_addAccountSigninCoordinator stop];
  _addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:_navigationController
                                          browser:self.browser
                                     contextStyle:style
                                      accessPoint:accessPoint
                                   prefilledEmail:nil
                             continuationProvider:
                                 DoNothingContinuationProvider()];
  __weak __typeof(self) weakSelf = self;
  _addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult signinResult,
        id<SystemIdentity> signinCompletionIdentity) {
        [weakSelf signinCoordinatorCompletionWithCoordinator:coordinator];
      };
  [_addAccountSigninCoordinator start];
}

- (void)mediatorWantsToBeDismissed:(AccountMenuMediator*)mediator
             withCancelationReason:
                 (signin_ui::CancelationReason)cancelationReason
                    signedIdentity:(id<SystemIdentity>)signedIdentity
                   userTappedClose:(BOOL)userTappedClose {
  CHECK_EQ(mediator, _mediator);
  [self stopChildrenAndViewControllerAnimated:YES];
  [self.delegate accountMenuCoordinatorWantsToBeStopped:self];

  if (userTappedClose) {
    // `self` may be deallocated when -accountMenuCoordinatorWantsToBeStopped
    // returns. We must access the browser first.
    Browser* browser = self.browser;
    [self.delegate accountMenuCoordinatorWantsToBeStopped:self];
    maybeShowSettingsIPH(browser);
  }
}

- (AuthenticationFlow*)authenticationFlow:(id<SystemIdentity>)identity
                               anchorRect:(CGRect)anchorRect {
  _signinInProgress = [self.sceneState createSigninInProgress];
  AuthenticationFlow* authenticationFlow = [[AuthenticationFlow alloc]
               initWithBrowser:self.browser
                      identity:identity
                   accessPoint:signin_metrics::AccessPoint::
                                   kAccountMenuSwitchAccount
          precedingHistorySync:NO
             postSignInActions:
                 {PostSignInAction::kShowIdentityConfirmationSnackbar}
      presentingViewController:_navigationController
                    anchorView:_viewController.view
                    anchorRect:anchorRect];
  return authenticationFlow;
}

- (void)signinFinished {
  CHECK(_signinInProgress, base::NotFatalUntil::M147);
  _signinInProgress.reset();
}

- (void)profileWillSwitchWithCompletion:(void (^)())completion {
  // There is no point to call the coordinator delegate to be stopped once the
  // view controller is dismissed.
  // The profile is going to be destroyed by calling `completion` since the
  // profile switching will start.
  [self dismissViewControllerAnimated:YES completion:completion];
}

#pragma mark - SyncErrorSettingsCommandHandler

- (void)openPassphraseDialogWithModalPresentation:(BOOL)presentModally {
  CHECK(presentModally);
  if (self.sceneState.isUIBlocked ||
      _syncEncryptionPassphraseTableViewController) {
    // This could occur due to race condition with multiple windows and
    // simultaneous taps. See crbug.com/368310663.
    return;
  }
  // In case of double tap, close the first view before opening a second one.
  [_syncEncryptionPassphraseTableViewController settingsWillBeDismissed];
  _syncEncryptionPassphraseTableViewController =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowser:self.browser];
  _syncEncryptionPassphraseTableViewController.presentationDelegate = self;
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
  if (_trustedVaultReauthenticationCoordinator) {
    // The user double-tapped the button. Don’t open the coordinator a second
    // time.
    return;
  }
  trusted_vault::SecurityDomainId securityDomainID =
      trusted_vault::SecurityDomainId::kChromeSync;
  trusted_vault::TrustedVaultUserActionTriggerForUMA trigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kAccountMenu;
  SigninTrustedVaultDialogIntent intent =
      SigninTrustedVaultDialogIntentFetchKeys;
  CHECK(!_trustedVaultReauthenticationCoordinator, base::NotFatalUntil::M145);
  _trustedVaultReauthenticationCoordinator =
      [[TrustedVaultReauthenticationCoordinator alloc]
          initWithBaseViewController:_navigationController
                             browser:self.browser
                              intent:intent
                    securityDomainID:securityDomainID
                             trigger:trigger];
  _trustedVaultReauthenticationCoordinator.delegate = self;
  [_trustedVaultReauthenticationCoordinator start];
}

- (void)openTrustedVaultReauthForDegradedRecoverability {
  if (_trustedVaultReauthenticationCoordinator) {
    // The user double-tapped the button. Don’t open the coordinator a second
    // time.
    return;
  }
  trusted_vault::SecurityDomainId securityDomainID =
      trusted_vault::SecurityDomainId::kChromeSync;
  trusted_vault::TrustedVaultUserActionTriggerForUMA trigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kAccountMenu;
  SigninTrustedVaultDialogIntent intent =
      SigninTrustedVaultDialogIntentDegradedRecoverability;
  CHECK(!_trustedVaultReauthenticationCoordinator, base::NotFatalUntil::M145);
  _trustedVaultReauthenticationCoordinator =
      [[TrustedVaultReauthenticationCoordinator alloc]
          initWithBaseViewController:_navigationController
                             browser:self.browser
                              intent:intent
                    securityDomainID:securityDomainID
                             trigger:trigger];
  _trustedVaultReauthenticationCoordinator.delegate = self;
  [_trustedVaultReauthenticationCoordinator start];
}

- (void)openMDMErrodDialogWithSystemIdentity:(id<SystemIdentity>)identity {
  _authenticationService->ShowMDMErrorDialogForIdentity(identity);
}

- (void)openPrimaryAccountReauthDialog {
  if (base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError)) {
    [self openReauthCoordinator];
  } else {
    [self openAddAccountReauthCoordinator];
  }
}

- (void)openReauthCoordinator {
  if (_reauthCoordinator.viewWillPersist) {
    return;
  }
  [_reauthCoordinator stop];

  CoreAccountInfo account =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (account.IsEmpty()) {
    // A sign-out was triggered in the meantime, don't do anything.
    return;
  }
  _reauthCoordinator = [[SigninReauthCoordinator alloc]
      initWithBaseViewController:_navigationController
                         browser:self.browser
                         account:account
               reauthAccessPoint:signin_metrics::ReauthAccessPoint::
                                     kAccountMenu];
  _reauthCoordinator.delegate = self;
  [_reauthCoordinator start];
}

- (void)openAddAccountReauthCoordinator {
  if (_addAccountSigninCoordinator.viewWillPersist) {
    return;
  }
  [_addAccountSigninCoordinator stop];
  signin_metrics::AccessPoint accessPoint =
      signin_metrics::AccessPoint::kAccountMenuSwitchAccount;
  signin_metrics::PromoAction promoAction =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  SigninContextStyle style = SigninContextStyle::kDefault;
  _addAccountSigninCoordinator = [SigninCoordinator
      primaryAccountReauthCoordinatorWithBaseViewController:
          _navigationController
                                                    browser:self.browser
                                               contextStyle:style
                                                accessPoint:accessPoint
                                                promoAction:promoAction
                                       continuationProvider:
                                           DoNothingContinuationProvider()];
  __weak __typeof(self) weakSelf = self;
  _addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult signinResult,
        id<SystemIdentity> signinCompletionIdentity) {
        [weakSelf signinCoordinatorCompletionWithCoordinator:coordinator];
      };
  [_addAccountSigninCoordinator start];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self stopManageAccountsNavigationController];
  [self.mediator accountMenuIsUsable];
}

- (void)settingsWasDismissed {
  [self stopManageAccountsNavigationController];
  [self.mediator accountMenuIsUsable];
}

#pragma mark - Private

- (void)stopTrustedVaultReauthenticationCoordinator {
  [_trustedVaultReauthenticationCoordinator stop];
  _trustedVaultReauthenticationCoordinator.delegate = nil;
  _trustedVaultReauthenticationCoordinator = nil;
}

- (void)stopAddAccountCoordinator {
  [_addAccountSigninCoordinator stop];
  _addAccountSigninCoordinator = nil;
}

// Clean up the add account coordinator.
- (void)signinCoordinatorCompletionWithCoordinator:
    (SigninCoordinator*)coordinator {
  CHECK_EQ(_addAccountSigninCoordinator, coordinator,
           base::NotFatalUntil::M151);
  [self.mediator accountMenuIsUsable];
  [self stopAddAccountCoordinator];
}

- (void)stopManageAccountsNavigationController {
  [_manageAccountsNavigationController dismissViewControllerAnimated:YES
                                                          completion:nil];
  [_manageAccountsNavigationController cleanUpSettings];
  _manageAccountsNavigationController.delegate = nil;
  _manageAccountsNavigationController = nil;
}

- (void)stopReauthCoordinator {
  [_reauthCoordinator stop];
  _reauthCoordinator.delegate = nil;
  _reauthCoordinator = nil;
}

- (void)resetAccountDetailsControllerDismissCallback {
  _accountDetailsControllerDismissCallback.Reset();
  [self.mediator accountMenuIsUsable];
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
- (void)stopChildrenAndViewControllerAnimated:(BOOL)animated {
  // Stopping all potentially open children views.
  if (!_accountDetailsControllerDismissCallback.is_null()) {
    std::move(_accountDetailsControllerDismissCallback).Run(/*animated=*/false);
  }
  [self stopSignoutActionSheetCoordinator];
  [self stopAddAccountCoordinator];
  [self stopReauthCoordinator];
  // Add Account coordinator should be stopped before the Manage Accounts
  // Coordinator, as the former may be presented by the latter.
  [self stopManageAccountsNavigationController];
  [self dismissViewControllerAnimated:animated completion:nil];
}

// Unplugs the view and navigation controller. Dismisses the navigation
// controller as specified by the action.
- (void)dismissViewControllerAnimated:(BOOL)animated
                           completion:(void (^)())completion {
  if (!_navigationController) {
    // The view controller was already dismissed.
    return;
  }
  _mediator.consumer = nil;
  _viewController.dataSource = nil;
  _viewController.mutator = nil;
  UINavigationController* navigationController = _navigationController;
  _navigationController = nil;
  _viewController = nil;
  [navigationController.presentingViewController
      dismissViewControllerAnimated:animated
                         completion:completion];
}

#pragma mark - SigninReauthCoordinatorDelegate

- (void)reauthFinishedWithResult:(ReauthResult)result
                          gaiaID:(const GaiaId*)gaiaID {
  // We expect the user reauthentified in the current account, so there is
  // nothing to do in this callback.
  [self stopReauthCoordinator];
}

#pragma mark - SyncEncryptionPassphraseTableViewControllerPresentationDelegate

- (void)syncEncryptionPassphraseTableViewControllerDidDisappear:
    (SyncEncryptionPassphraseTableViewController*)viewController {
  CHECK_EQ(_syncEncryptionPassphraseTableViewController, viewController,
           base::NotFatalUntil::M142);
  _syncEncryptionPassphraseTableViewController.presentationDelegate = nil;
  [_syncEncryptionPassphraseTableViewController settingsWillBeDismissed];
  _syncEncryptionPassphraseTableViewController = nil;
  [_mediator accountMenuIsUsable];
}

#pragma mark - TrustedVaultReauthenticationCoordinatorDelegate

- (void)trustedVaultReauthenticationCoordinatorWantsToBeStopped:
    (TrustedVaultReauthenticationCoordinator*)coordinator {
  CHECK_EQ(coordinator, _trustedVaultReauthenticationCoordinator,
           base::NotFatalUntil::M145);
  [self stopTrustedVaultReauthenticationCoordinator];
}

@end
