// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_coordinator.h"

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/callback_helpers.h"
#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/google/core/common/google_util.h"
#import "components/regional_capabilities/regional_capabilities_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator.h"
#import "ios/chrome/browser/authentication/account_menu/coordinator/account_menu_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/account_menu/public/account_menu_constants.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator.h"
#import "ios/chrome/browser/authentication/trusted_vault_reauthentication/coordinator/trusted_vault_reauthentication_coordinator_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/continuation.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/regional_capabilities/model/regional_capabilities_service_factory.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/bulk_upload/bulk_upload_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/bulk_upload/bulk_upload_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/features.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_coordinator_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_mediator.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/personalize_google_services_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_controller_protocol.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/sync/sync_encryption_table_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/google_one_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;
using DismissViewCallback = SystemIdentityManager::DismissViewCallback;

@interface ManageSyncSettingsCoordinator () <
    AccountMenuCoordinatorDelegate,
    BulkUploadCoordinatorDelegate,
    ManageSyncSettingsCommandHandler,
    ManageSyncSettingsTableViewControllerPresentationDelegate,
    PersonalizeGoogleServicesCoordinatorDelegate,
    SettingsNavigationControllerDelegate,
    SignoutActionSheetCoordinatorDelegate,
    SyncEncryptionPassphraseTableViewControllerPresentationDelegate,
    SyncEncryptionTableViewControllerPresentationDelegate,
    SyncErrorSettingsCommandHandler,
    TrustedVaultReauthenticationCoordinatorDelegate> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
  // The coordinator for the view Save in Account.
  BulkUploadCoordinator* _bulkUploadCoordinator;
  // The navigation controller displaying the Manage Accounts view.
  SettingsNavigationController* _manageAccountsNavigationController;
  SyncEncryptionTableViewController* _syncEncryptionTableViewController;
  SyncEncryptionPassphraseTableViewController*
      _syncEncryptionPassphraseTableViewController;
  // Account menu coordinator.
  AccountMenuCoordinator* _accountMenuCoordinator;
  TrustedVaultReauthenticationCoordinator*
      _trustedVaultReauthenticationCoordinator;
}

// View controller.
@property(nonatomic, strong)
    ManageSyncSettingsTableViewController* viewController;
// Mediator.
@property(nonatomic, strong) ManageSyncSettingsMediator* mediator;
// Sync service.
@property(nonatomic, assign, readonly) syncer::SyncService* syncService;
// Authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;
// Displays the sign-out options for a syncing user.
@property(nonatomic, strong)
    SignoutActionSheetCoordinator* signoutActionSheetCoordinator;

@end

@implementation ManageSyncSettingsCoordinator {
  // Dismiss callback for Web and app setting details view.
  DismissViewCallback _dismissWebAndAppSettingDetailsController;
  // Dismiss callback for account details view.
  DismissViewCallback _accountDetailsControllerDismissCallback;
  // The coordinator for the Personalize Google Services view.
  PersonalizeGoogleServicesCoordinator* _personalizeGoogleServicesCoordinator;
  SigninCoordinator* _addAccountCoordinator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    CHECK(navigationController, base::NotFatalUntil::M142);
    CHECK_EQ(browser->type(), Browser::Type::kRegular,
             base::NotFatalUntil::M145);
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  ProfileIOS* profile = self.profile;

  self.mediator = [[ManageSyncSettingsMediator alloc]
        initWithSyncService:self.syncService
            identityManager:IdentityManagerFactory::GetForProfile(profile)
      authenticationService:self.authService
      accountManagerService:ChromeAccountManagerServiceFactory::GetForProfile(
                                profile)
                prefService:profile->GetPrefs()];
  self.mediator.commandHandler = self;
  self.mediator.syncErrorHandler = self;
  self.mediator.forcedSigninEnabled =
      self.authService->GetServiceStatus() ==
      AuthenticationService::ServiceStatus::SigninForcedByPolicy;
  if (IsLinkedServicesSettingIosEnabled()) {
    self.mediator.isEEAAccount =
        ios::RegionalCapabilitiesServiceFactory::GetForProfile(self.profile)
            ->IsInEeaCountry();
  }

  ManageSyncSettingsTableViewController* viewController =
      [[ManageSyncSettingsTableViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  self.viewController = viewController;

  viewController.title = self.mediator.overrideViewControllerTitle;
  viewController.serviceDelegate = self.mediator;
  viewController.presentationDelegate = self;
  viewController.modelDelegate = self.mediator;

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  viewController.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  viewController.browserHandler =
      HandlerForProtocol(dispatcher, BrowserCommands);
  viewController.settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  viewController.snackbarHandler =
      HandlerForProtocol(dispatcher, SnackbarCommands);

  self.mediator.consumer = viewController;

  CHECK(_baseNavigationController);
  [self.baseNavigationController pushViewController:viewController
                                           animated:YES];
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
  self.mediator = nil;
  self.viewController = nil;
  [self stopChildren];
  _syncObserver.reset();
}

#pragma mark - Properties

- (syncer::SyncService*)syncService {
  return SyncServiceFactory::GetForProfile(self.profile);
}

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForProfile(self.profile);
}

#pragma mark - Private

// Called when the add account coordinator is complete.
- (void)addAccountCoordinatorCompletedWithCoordinator:
    (SigninCoordinator*)coordinator {
  CHECK_EQ(_addAccountCoordinator, coordinator, base::NotFatalUntil::M151);
  [self stopAddAccountCoordinator];
}

// Stops properly all views opened by the current coordinator.
- (void)stopChildren {
  [self stopBulkUpload];
  [self stopManageAccountsNavigationController];
  [self stopAccountMenuCoordinator];
  [self stopTrustedVaultReauthenticationCoordinator];
  [self stopAddAccountCoordinator];
  [self stopSignoutActionSheetCoordinator];
  [self stopPersonalizedGoogleServicesCoordinator];

  // The view controller below don’t have coordinator, so they must be stopped
  // with `settingsWillBeDismissed`.
  [_syncEncryptionPassphraseTableViewController settingsWillBeDismissed];
  _syncEncryptionPassphraseTableViewController = nil;
  [_syncEncryptionTableViewController settingsWillBeDismissed];
  _syncEncryptionTableViewController = nil;
}

- (void)stopAddAccountCoordinator {
  [_addAccountCoordinator stop];
  _addAccountCoordinator = nil;
}

- (void)stopTrustedVaultReauthenticationCoordinator {
  [_trustedVaultReauthenticationCoordinator stop];
  _trustedVaultReauthenticationCoordinator.delegate = nil;
  _trustedVaultReauthenticationCoordinator = nil;
}

- (void)stopManageAccountsNavigationController {
  _manageAccountsNavigationController.delegate = nil;
  [_manageAccountsNavigationController cleanUpSettings];
  [_manageAccountsNavigationController dismissViewControllerAnimated:YES
                                                          completion:nil];
  _manageAccountsNavigationController = nil;
}

- (void)stopSignoutActionSheetCoordinator {
  [self.signoutActionSheetCoordinator stop];
  _signoutActionSheetCoordinator.delegate = nil;
  _signoutActionSheetCoordinator = nil;
}

- (void)resetDismissAccountDetailsController {
  _accountDetailsControllerDismissCallback.Reset();
}

- (void)stopBulkUpload {
  [_bulkUploadCoordinator stop];
  _bulkUploadCoordinator.delegate = nil;
  _bulkUploadCoordinator = nil;
}

- (void)stopPersonalizedGoogleServicesCoordinator {
  [_personalizeGoogleServicesCoordinator stop];
  _personalizeGoogleServicesCoordinator = nil;
}

- (void)stopAccountMenuCoordinator {
  [_accountMenuCoordinator stop];
  _accountMenuCoordinator.delegate = nil;
  _accountMenuCoordinator = nil;
}

// Closes the Manage sync settings view controller.
- (void)closeManageSyncSettings {
  if (_settingsAreDismissed) {
    return;
  }
  [self stopChildren];
  if (self.viewController.navigationController) {
    if (!_dismissWebAndAppSettingDetailsController.is_null()) {
      std::move(_dismissWebAndAppSettingDetailsController)
          .Run(/*animated*/ false);
    }
    if (!_accountDetailsControllerDismissCallback.is_null()) {
      std::move(_accountDetailsControllerDismissCallback)
          .Run(/*animated=*/false);
    }

    NSEnumerator<UIViewController*>* inversedViewControllers =
        [_baseNavigationController.viewControllers reverseObjectEnumerator];
    for (UIViewController* controller in inversedViewControllers) {
      if (controller == self.viewController) {
        break;
      }
      if ([controller respondsToSelector:@selector(settingsWillBeDismissed)]) {
        [controller performSelector:@selector(settingsWillBeDismissed)];
      }
    }

    if (_baseNavigationController) {
      if (self.viewController.presentedViewController) {
        if ([self.viewController.presentedViewController
                isKindOfClass:[SettingsNavigationController class]]) {
          [self.viewController.presentedViewController
              performSelector:@selector(closeSettings)];
        } else {
          [self.viewController.presentedViewController.presentingViewController
              dismissViewControllerAnimated:YES
                                 completion:nil];
        }
      }
      if (self.baseNavigationController.viewControllers.count == 1) {
        // If the manage sync settings is the only view in
        // `baseNavigationController`, `baseNavigationController` needs to be
        // closed.
        CHECK([self.baseNavigationController
            isKindOfClass:[SettingsNavigationController class]]);
        [self.baseNavigationController
            performSelector:@selector(closeSettings)];
      } else {
        [self.baseNavigationController popToViewController:self.viewController
                                                  animated:NO];
        [self.baseNavigationController popViewControllerAnimated:YES];
        [self.delegate manageSyncSettingsCoordinatorWasRemoved:self];
      }
    } else {
      [_baseNavigationController.presentingViewController
          dismissViewControllerAnimated:YES
                             completion:nil];
      [self.delegate manageSyncSettingsCoordinatorWasRemoved:self];
    }
  }
  _settingsAreDismissed = YES;
}

#pragma mark - ManageSyncSettingsTableViewControllerPresentationDelegate

- (void)manageSyncSettingsTableViewControllerWasRemoved:
    (ManageSyncSettingsTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate manageSyncSettingsCoordinatorWasRemoved:self];
}

#pragma mark - PersonalizeGoogleServicesCoordinatorDelegate

- (void)personalizeGoogleServicesCoordinatorWasRemoved:
    (PersonalizeGoogleServicesCoordinator*)coordinator {
  CHECK_EQ(_personalizeGoogleServicesCoordinator, coordinator);
  [self stopPersonalizedGoogleServicesCoordinator];
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self stopManageAccountsNavigationController];
}

- (void)settingsWasDismissed {
  [self stopManageAccountsNavigationController];
}

#pragma mark - ManageSyncSettingsCommandHandler

- (void)openBulkUpload {
  [self stopBulkUpload];
  base::RecordAction(base::UserMetricsAction("BulkUploadSettingsOpen"));
  _bulkUploadCoordinator = [[BulkUploadCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  _bulkUploadCoordinator.delegate = self;
  [_bulkUploadCoordinator start];
}

- (void)openWebAppActivityDialog {
  base::RecordAction(base::UserMetricsAction(
      "Signin_AccountSettings_GoogleActivityControlsClicked"));
  id<SystemIdentity> identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  _dismissWebAndAppSettingDetailsController =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentWebAndAppSettingDetailsController(
              identity, self.viewController, /*animated=*/YES,
              base::DoNothing());
}

- (void)openPersonalizeGoogleServices {
  CHECK(!_personalizeGoogleServicesCoordinator);

  base::RecordAction(base::UserMetricsAction(
      "Signin_AccountSettings_PersonalizeGoogleServicesClicked"));

  _personalizeGoogleServicesCoordinator =
      [[PersonalizeGoogleServicesCoordinator alloc]
          initWithBaseNavigationController:_baseNavigationController
                                   browser:self.browser];
  _personalizeGoogleServicesCoordinator.delegate = self;
  [_personalizeGoogleServicesCoordinator start];
}

- (void)openDataFromChromeSyncWebPage {
  if ([self.delegate
          respondsToSelector:@selector
          (manageSyncSettingsCoordinatorNeedToOpenChromeSyncWebPage:)]) {
    [self.delegate
        manageSyncSettingsCoordinatorNeedToOpenChromeSyncWebPage:self];
  }
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(base::FeatureList::IsEnabled(syncer::kSyncEnableNewSyncDashboardUrl)
               ? kNewSyncGoogleDashboardURL
               : kLegacySyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocaleStorage()->Get());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler closePresentedViewsAndOpenURL:command];
}

- (void)signOutFromTargetRect:(CGRect)targetRect {
  if (!self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    // This could happen in very rare cases, if the account somehow got removed
    // after the settings UI was created.
    return;
  }
  constexpr signin_metrics::ProfileSignout metricSignOut =
      signin_metrics::ProfileSignout::kUserClickedSignoutSettings;

  __weak ManageSyncSettingsCoordinator* weakSelf = self;
  self.signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                            rect:targetRect
                            view:self.viewController.view
        forceSnackbarOverToolbar:NO
                      withSource:metricSignOut
                      completion:^(BOOL success, SceneState* scene_state) {
                        [weakSelf handleSignOutCompleted:success];
                      }];
  self.signoutActionSheetCoordinator.delegate = self;
  [self.signoutActionSheetCoordinator start];
}

// Handles signout operation with `success` or failure.
- (void)handleSignOutCompleted:(BOOL)success {
  [self stopSignoutActionSheetCoordinator];
  if (success) {
    [self closeManageSyncSettings];
  }
}

- (void)showAdressesNotEncryptedDialog {
  AlertCoordinator* alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                           title:l10n_util::GetNSString(
                                     IDS_IOS_SYNC_ADDRESSES_DIALOG_TITLE)
                         message:l10n_util::GetNSString(
                                     IDS_IOS_SYNC_ADDRESSES_DIALOG_MESSAGE)];

  __weak __typeof(self) weakSelf = self;
  [alertCoordinator addItemWithTitle:l10n_util::GetNSString(
                                         IDS_IOS_SYNC_ADDRESSES_DIALOG_CONTINUE)
                              action:^{
                                [weakSelf.mediator autofillAlertConfirmed:YES];
                              }
                               style:UIAlertActionStyleDefault];

  [alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                              action:^{
                                [weakSelf.mediator autofillAlertConfirmed:NO];
                              }
                               style:UIAlertActionStyleCancel];

  [alertCoordinator start];
}

- (void)showAccountsPage {
  // Stopping the manage accounts coordinator if it’s already opened. See
  // crbug.com/383373460
  [self stopManageAccountsNavigationController];
  _manageAccountsNavigationController = [SettingsNavigationController
             accountsControllerForBrowser:self.browser
                       baseViewController:self.viewController
                                 delegate:self
                closeSettingsOnAddAccount:NO
                        showSignoutButton:NO
                           showDoneButton:YES
      signoutDismissalByParentCoordinator:YES];
}

- (void)showManageYourGoogleAccount {
  __weak __typeof(self) weakself = self;
  _accountDetailsControllerDismissCallback =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentAccountDetailsController(
              self.authService->GetPrimaryIdentity(
                  signin::ConsentLevel::kSignin),
              self.viewController,
              /*animated=*/YES,
              base::BindOnce(
                  [](__typeof(self) weakSelf) {
                    [weakSelf resetDismissAccountDetailsController];
                  },
                  weakself));
}

- (void)openAccountMenu {
  if (_accountMenuCoordinator) {
    // This can occurs in cause of double tap.
    return;
  }
  _accountMenuCoordinator = [[AccountMenuCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                      anchorView:nil
                     accessPoint:AccountMenuAccessPoint::kNewTabPage
                             URL:GURL()];
  _accountMenuCoordinator.delegate = self;
  [_accountMenuCoordinator start];
}

#pragma mark - SignoutActionSheetCoordinatorDelegate

- (void)signoutActionSheetCoordinatorPreventUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  self.mediator.signOutFlowInProgress = YES;
  [self.viewController preventUserInteraction];
}

- (void)signoutActionSheetCoordinatorAllowUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  [self.viewController allowUserInteraction];
  self.mediator.signOutFlowInProgress = NO;
}

#pragma mark - SyncErrorSettingsCommandHandler

- (void)openPassphraseDialogWithModalPresentation:(BOOL)presentModally {
  SceneState* sceneState = self.browser->GetSceneState();
  if (sceneState.isUIBlocked || _syncEncryptionTableViewController ||
      _syncEncryptionPassphraseTableViewController) {
    // This could occur due to race condition with multiple windows and
    // simultaneous taps. See crbug.com/368310663.
    return;
  }
  if (presentModally) {
    _syncEncryptionPassphraseTableViewController =
        [[SyncEncryptionPassphraseTableViewController alloc]
            initWithBrowser:self.browser];
    _syncEncryptionPassphraseTableViewController.presentationDelegate = self;
    _syncEncryptionPassphraseTableViewController.presentModally = YES;
    UINavigationController* navigationController =
        [[UINavigationController alloc]
            initWithRootViewController:
                _syncEncryptionPassphraseTableViewController];
    navigationController.modalPresentationStyle = UIModalPresentationFormSheet;
    [self.viewController configureHandlersForRootViewController:
                             _syncEncryptionPassphraseTableViewController];
    [self.viewController presentViewController:navigationController
                                      animated:YES
                                    completion:nil];
    return;
  }
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  UIViewController<SettingsRootViewControlling>* controllerToPush;
  if (self.syncService->GetUserSettings()->IsPassphraseRequired()) {
    controllerToPush = _syncEncryptionPassphraseTableViewController =
        [[SyncEncryptionPassphraseTableViewController alloc]
            initWithBrowser:self.browser];
    _syncEncryptionPassphraseTableViewController.presentationDelegate = self;
  } else {
    controllerToPush = _syncEncryptionTableViewController =
        [[SyncEncryptionTableViewController alloc]
            initWithBrowser:self.browser];
    _syncEncryptionTableViewController.presentationDelegate = self;
  }

  [self.viewController configureHandlersForRootViewController:controllerToPush];
  [_baseNavigationController pushViewController:controllerToPush animated:YES];
}

- (void)openTrustedVaultReauthForFetchKeys {
  if (_trustedVaultReauthenticationCoordinator) {
    // This can occur if the user double tap on the error button.
    return;
  }
  trusted_vault::SecurityDomainId chromeSyncID =
      trusted_vault::SecurityDomainId::kChromeSync;
  trusted_vault::TrustedVaultUserActionTriggerForUMA settingsTrigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kSettings;
  _trustedVaultReauthenticationCoordinator =
      [[TrustedVaultReauthenticationCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                              intent:SigninTrustedVaultDialogIntentFetchKeys
                    securityDomainID:chromeSyncID
                             trigger:settingsTrigger];
  _trustedVaultReauthenticationCoordinator.delegate = self;
  [_trustedVaultReauthenticationCoordinator start];
}

- (void)openTrustedVaultReauthForDegradedRecoverability {
  if (_trustedVaultReauthenticationCoordinator) {
    // This can occurs in case of double tap.
    return;
  }
  trusted_vault::SecurityDomainId chromeSyncID =
      trusted_vault::SecurityDomainId::kChromeSync;
  trusted_vault::TrustedVaultUserActionTriggerForUMA settingsTrigger =
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kSettings;
  SigninTrustedVaultDialogIntent intent =
      SigninTrustedVaultDialogIntentDegradedRecoverability;
  CHECK(!_trustedVaultReauthenticationCoordinator, base::NotFatalUntil::M145);
  _trustedVaultReauthenticationCoordinator =
      [[TrustedVaultReauthenticationCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser
                              intent:intent
                    securityDomainID:chromeSyncID
                             trigger:settingsTrigger];
  _trustedVaultReauthenticationCoordinator.delegate = self;
  [_trustedVaultReauthenticationCoordinator start];
}

- (void)openMDMErrodDialogWithSystemIdentity:(id<SystemIdentity>)identity {
  self.authService->ShowMDMErrorDialogForIdentity(identity);
}

- (void)openPrimaryAccountReauthDialog {
  if (_addAccountCoordinator.viewWillPersist) {
    return;
  }
  [_addAccountCoordinator stop];
  SigninContextStyle contextStyle = SigninContextStyle::kDefault;
  AccessPoint accessPoint = AccessPoint::kSettings;
  signin_metrics::PromoAction promoAction =
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO;
  _addAccountCoordinator = [SigninCoordinator
      primaryAccountReauthCoordinatorWithBaseViewController:self.viewController
                                                    browser:self.browser
                                               contextStyle:contextStyle
                                                accessPoint:accessPoint
                                                promoAction:promoAction
                                       continuationProvider:
                                           DoNothingContinuationProvider()];
  __weak __typeof(self) weakSelf = self;
  _addAccountCoordinator.signinCompletion =
      ^(SigninCoordinator* coordinator, SigninCoordinatorResult result,
        id<SystemIdentity> identity) {
        [weakSelf addAccountCoordinatorCompletedWithCoordinator:coordinator];
      };
  [_addAccountCoordinator start];
}

- (void)openAccountStorage {
  id<SystemIdentity> identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  id<GoogleOneCommands> googleOneCommands = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), GoogleOneCommands);
  [googleOneCommands showGoogleOneForIdentity:identity
                                   entryPoint:GoogleOneEntryPoint::kSettings
                           baseViewController:self.viewController];
}

#pragma mark - BulkUploadCoordinatorDelegate

- (void)bulkUploadCoordinatorShouldStop:(BulkUploadCoordinator*)coordinator {
  DCHECK_EQ(coordinator, _bulkUploadCoordinator);
  [self stopBulkUpload];
}

#pragma mark - TrustedVaultReauthenticationCoordinatorDelegate

- (void)trustedVaultReauthenticationCoordinatorWantsToBeStopped:
    (TrustedVaultReauthenticationCoordinator*)coordinator {
  CHECK_EQ(coordinator, _trustedVaultReauthenticationCoordinator);
  [self stopTrustedVaultReauthenticationCoordinator];
}

#pragma mark - AccountMenuCoordinatorDelegate

- (void)accountMenuCoordinatorWantsToBeStopped:
    (AccountMenuCoordinator*)coordinator {
  CHECK_EQ(_accountMenuCoordinator, coordinator, base::NotFatalUntil::M140);
  [self stopAccountMenuCoordinator];
}

#pragma mark - SyncEncryptionPassphraseTableViewControllerPresentationDelegate

- (void)syncEncryptionPassphraseTableViewControllerDidDisappear:
    (SyncEncryptionPassphraseTableViewController*)viewController {
  CHECK_EQ(_syncEncryptionPassphraseTableViewController, viewController,
           base::NotFatalUntil::M142);
  _syncEncryptionPassphraseTableViewController.presentationDelegate = nil;
  [_syncEncryptionPassphraseTableViewController settingsWillBeDismissed];
  _syncEncryptionPassphraseTableViewController = nil;
}

#pragma mark - SyncEncryptionTableViewControllerPresentationDelegate

- (void)syncEncryptionTableViewControllerDidDisappear:
    (SyncEncryptionTableViewController*)viewController {
  CHECK_EQ(_syncEncryptionTableViewController, viewController,
           base::NotFatalUntil::M150);
  _syncEncryptionTableViewController.presentationDelegate = nil;
  [_syncEncryptionTableViewController settingsWillBeDismissed];
  _syncEncryptionTableViewController = nil;
}

@end
