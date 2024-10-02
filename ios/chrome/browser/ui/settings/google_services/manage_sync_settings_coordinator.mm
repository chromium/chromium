// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"

#import "base/check_op.h"
#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "components/google/core/common/google_util.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_service_utils.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/trusted_vault/trusted_vault_server_constants.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/sync/model/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/google_services/features.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/personalize_google_services_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;
using DismissViewCallback = SystemIdentityManager::DismissViewCallback;

@interface ManageSyncSettingsCoordinator () <
    BulkUploadCoordinatorDelegate,
    ManageSyncSettingsCommandHandler,
    ManageSyncSettingsTableViewControllerPresentationDelegate,
    PersonalizeGoogleServicesCoordinatorDelegate,
    SettingsNavigationControllerDelegate,
    SignoutActionSheetCoordinatorDelegate,
    SyncErrorSettingsCommandHandler,
    SyncObserverModelBridge> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
  // The coordinator for the view Save in Account.
  BulkUploadCoordinator* _bulkUploadCoordinator;
  // The coordinator for the Accounts view.
  AccountsCoordinator* _accountsCoordinator;
  SyncEncryptionTableViewController* _syncEncryptionTableViewController;
  SyncEncryptionPassphraseTableViewController*
      _syncEncryptionPassphraseTableViewController;
}

// View controller.
@property(nonatomic, strong)
    ManageSyncSettingsTableViewController* viewController;
// Mediator.
@property(nonatomic, strong) ManageSyncSettingsMediator* mediator;
// The navigation controller used to present child controllers of
// ManageSyncSettings.
@property(nonatomic, readonly)
    UINavigationController* _navigationControllerForChildPages;
// Sync service.
@property(nonatomic, assign, readonly) syncer::SyncService* syncService;
// Authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;
// Displays the sign-out options for a syncing user.
@property(nonatomic, strong)
    SignoutActionSheetCoordinator* signoutActionSheetCoordinator;
@property(nonatomic, assign) BOOL signOutFlowInProgress;

@end

@implementation ManageSyncSettingsCoordinator {
  // Dismiss callback for Web and app setting details view.
  DismissViewCallback _dismissWebAndAppSettingDetailsController;
  // Dismiss callback for account details view.
  DismissViewCallback _accountDetailsControllerDismissCallback;
  // The account sync state.
  SyncSettingsAccountState _accountState;
  // The navigation controller to use only when presenting the
  // ManageSyncSettings modally.
  SettingsNavigationController* _navigationControllerInModalView;
  // The coordinator for the Personalize Google Services view.
  PersonalizeGoogleServicesCoordinator* _personalizeGoogleServicesCoordinator;
  // Prevents any data from syncing while the UI is open.
  // TODO(crbug.com/40066949): This is currently needed for syncing users,
  // otherwise accidentally touching a toggle immediately uploads existing data.
  // For non-syncing users that's not true. So remove this after the syncing
  // state is gone on iOS.
  std::unique_ptr<syncer::SyncSetupInProgressHandle> _syncSetupInProgressHandle;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                    accountState:
                                        (SyncSettingsAccountState)accountState {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _baseNavigationController = navigationController;
    _accountState = accountState;
  }
  return self;
}

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile();
  syncer::SyncService* syncService = SyncServiceFactory::GetForProfile(profile);
  switch (_accountState) {
    case SyncSettingsAccountState::kSyncing:
      // Ensure that SyncService::IsSetupInProgress is true while the
      // manage-sync-settings UI is open.
      _syncSetupInProgressHandle = syncService->GetSetupInProgressHandle();
      break;
    case SyncSettingsAccountState::kSignedIn:
      break;
    case SyncSettingsAccountState::kSignedOut:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  self.mediator = [[ManageSyncSettingsMediator alloc]
        initWithSyncService:self.syncService
            identityManager:IdentityManagerFactory::GetForProfile(profile)
      authenticationService:self.authService
      accountManagerService:ChromeAccountManagerServiceFactory::GetForProfile(
                                profile)
                prefService:profile->GetPrefs()
        initialAccountState:_accountState];
  self.mediator.commandHandler = self;
  self.mediator.syncErrorHandler = self;
  self.mediator.forcedSigninEnabled =
      self.authService->GetServiceStatus() ==
      AuthenticationService::ServiceStatus::SigninForcedByPolicy;
  if (IsLinkedServicesSettingIosEnabled()) {
    self.mediator.isEEAAccount = ios::TemplateURLServiceFactory::GetForProfile(
                                     self.browser->GetProfile())
                                     ->IsEeaChoiceCountry();
  }

  ManageSyncSettingsTableViewController* viewController =
      [[ManageSyncSettingsTableViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  self.viewController = viewController;

  NSString* title = self.mediator.overrideViewControllerTitle;
  if (!title) {
    title = self.delegate.manageSyncSettingsCoordinatorTitle;
  }
  viewController.title = title;
  viewController.isAccountStateSignedIn =
      _accountState == SyncSettingsAccountState::kSignedIn;
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

  CHECK(_baseNavigationController, base::NotFatalUntil::M129);
  [self.baseNavigationController pushViewController:viewController
                                           animated:YES];
  _syncObserver = std::make_unique<SyncObserverBridge>(self, self.syncService);
}

- (void)stop {
  [super stop];
  [self.mediator disconnect];
  [self stopBulkUpload];
  [_accountsCoordinator stop];
  _accountsCoordinator = nil;
  self.mediator = nil;
  self.viewController = nil;
  // Unblock any sync data type changes.
  _syncSetupInProgressHandle.reset();
  [_syncEncryptionPassphraseTableViewController settingsWillBeDismissed];
  _syncEncryptionPassphraseTableViewController = nil;
  [_syncEncryptionTableViewController settingsWillBeDismissed];
  _syncEncryptionTableViewController = nil;

  _syncObserver.reset();
  [self.signoutActionSheetCoordinator stop];
  _signoutActionSheetCoordinator = nil;

  [self stopPersonalizedGoogleServicesCoordinator];
}

#pragma mark - Properties

- (UINavigationController*)navigationControllerForChildPages {
  if (_baseNavigationController) {
    return _baseNavigationController;
  }
  CHECK(_navigationControllerInModalView);
  return _navigationControllerInModalView;
}

- (syncer::SyncService*)syncService {
  return SyncServiceFactory::GetForProfile(self.browser->GetProfile());
}

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForProfile(
      self.browser->GetProfile());
}

#pragma mark - Private

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

// Closes the Manage sync settings view controller.
- (void)closeManageSyncSettings {
  if (_settingsAreDismissed) {
    return;
  }
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
        [self.navigationControllerForChildPages
                .viewControllers reverseObjectEnumerator];
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
                  isKindOfClass:[SettingsNavigationController class]],
              base::NotFatalUntil::M129);
        [self.baseNavigationController
            performSelector:@selector(closeSettings)];
      } else {
        [self.baseNavigationController popToViewController:self.viewController
                                                  animated:NO];
        [self.baseNavigationController popViewControllerAnimated:YES];
        [self.delegate manageSyncSettingsCoordinatorWasRemoved:self];
      }
    } else {
      [self.navigationControllerForChildPages.presentingViewController
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

  _personalizeGoogleServicesCoordinator = [[PersonalizeGoogleServicesCoordinator
      alloc]
      initWithBaseNavigationController:self.navigationControllerForChildPages
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
      GURL(kSyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler closeSettingsUIAndOpenURL:command];
}

- (void)signOutFromTargetRect:(CGRect)targetRect {
  if (!self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    // This could happen in very rare cases, if the account somehow got removed
    // after the settings UI was created.
    return;
  }
  self.signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                            rect:targetRect
                            view:self.viewController.view
        forceSnackbarOverToolbar:NO
                      withSource:signin_metrics::ProfileSignout::
                                     kUserClickedSignoutSettings];
  self.signoutActionSheetCoordinator.delegate = self;
  __weak ManageSyncSettingsCoordinator* weakSelf = self;
  self.signoutActionSheetCoordinator.signoutCompletion = ^(BOOL success) {
    [weakSelf.signoutActionSheetCoordinator stop];
    weakSelf.signoutActionSheetCoordinator = nil;
    if (success) {
      [weakSelf closeManageSyncSettings];
    }
  };
  [self.signoutActionSheetCoordinator start];
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
  AccountsCoordinator* accountsCoordinator = [[AccountsCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
       closeSettingsOnAddAccount:NO];
  accountsCoordinator.signoutDismissalByParentCoordinator = YES;
  _accountsCoordinator = accountsCoordinator;
  [accountsCoordinator start];
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

#pragma mark - SignoutActionSheetCoordinatorDelegate

- (void)signoutActionSheetCoordinatorPreventUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  self.signOutFlowInProgress = YES;
  [self.viewController preventUserInteraction];
}

- (void)signoutActionSheetCoordinatorAllowUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  [self.viewController allowUserInteraction];
  self.signOutFlowInProgress = NO;
}

#pragma mark - SyncErrorSettingsCommandHandler

- (void)openPassphraseDialogWithModalPresentation:(BOOL)presentModally {
  if (presentModally) {
    _syncEncryptionPassphraseTableViewController =
        [[SyncEncryptionPassphraseTableViewController alloc]
            initWithBrowser:self.browser];
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
  } else {
    controllerToPush = _syncEncryptionTableViewController =
        [[SyncEncryptionTableViewController alloc]
            initWithBrowser:self.browser];
  }

  [self.viewController configureHandlersForRootViewController:controllerToPush];
  [self.navigationControllerForChildPages pushViewController:controllerToPush
                                                    animated:YES];
}

- (void)openTrustedVaultReauthForFetchKeys {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  trusted_vault::SecurityDomainId chromeSyncID =
      trusted_vault::SecurityDomainId::kChromeSync;
  syncer::TrustedVaultUserActionTriggerForUMA settingsTrigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  AccessPoint settingsAccessPoint = AccessPoint::ACCESS_POINT_SETTINGS;
  [applicationCommands
      showTrustedVaultReauthForFetchKeysFromViewController:self.viewController
                                          securityDomainID:chromeSyncID
                                                   trigger:settingsTrigger
                                               accessPoint:settingsAccessPoint];
}

- (void)openTrustedVaultReauthForDegradedRecoverability {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  trusted_vault::SecurityDomainId chromeSyncID =
      trusted_vault::SecurityDomainId::kChromeSync;
  syncer::TrustedVaultUserActionTriggerForUMA settingsTrigger =
      syncer::TrustedVaultUserActionTriggerForUMA::kSettings;
  AccessPoint settingsAccessPoint = AccessPoint::ACCESS_POINT_SETTINGS;
  [applicationCommands
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
          self.viewController
                                                       securityDomainID:
                                                           chromeSyncID
                                                                trigger:
                                                                    settingsTrigger
                                                            accessPoint:
                                                                settingsAccessPoint];
}

- (void)openMDMErrodDialogWithSystemIdentity:(id<SystemIdentity>)identity {
  self.authService->ShowMDMErrorDialogForIdentity(identity);
}

- (void)openPrimaryAccountReauthDialog {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  ShowSigninCommand* signinCommand = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperation::kPrimaryAccountReauth
            accessPoint:AccessPoint::ACCESS_POINT_SETTINGS];
  [applicationCommands showSignin:signinCommand
               baseViewController:self.viewController];
}

#pragma mark - BulkUploadCoordinatorDelegate

- (void)bulkUploadCoordinatorShouldStop:(BulkUploadCoordinator*)coordinator {
  DCHECK_EQ(coordinator, _bulkUploadCoordinator);
  [self stopBulkUpload];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (self.signOutFlowInProgress) {
    return;
  }
  if (!self.syncService->GetDisableReasons().empty()) {
    [self closeManageSyncSettings];
  }
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self.navigationControllerForChildPages.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  [self.delegate manageSyncSettingsCoordinatorWasRemoved:self];
}

- (void)settingsWasDismissed {
  [self.delegate manageSyncSettingsCoordinatorWasRemoved:self];
}

@end
