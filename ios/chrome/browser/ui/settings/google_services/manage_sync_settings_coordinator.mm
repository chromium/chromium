// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"

#import "base/check_op.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/google/core/common/google_util.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_service_utils.h"
#import "components/sync/driver/sync_user_settings.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/sync_error_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;
using DismissViewCallback = SystemIdentityManager::DismissViewCallback;

@interface ManageSyncSettingsCoordinator () <
    ManageSyncSettingsCommandHandler,
    ManageSyncSettingsTableViewControllerPresentationDelegate,
    SignoutActionSheetCoordinatorDelegate,
    SyncErrorSettingsCommandHandler,
    SyncObserverModelBridge> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
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
@property(nonatomic, assign) BOOL signOutFlowInProgress;

@end

@implementation ManageSyncSettingsCoordinator {
  // Dismiss callback for Web and app setting details view.
  DismissViewCallback _dismissWebAndAppSettingDetailsController;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:navigationController
                                       browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  DCHECK(self.baseNavigationController);
  self.mediator = [[ManageSyncSettingsMediator alloc]
      initWithSyncService:self.syncService
          userPrefService:self.browser->GetBrowserState()->GetPrefs()];
  self.mediator.syncSetupService = SyncSetupServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.mediator.commandHandler = self;
  self.mediator.syncErrorHandler = self;
  self.mediator.forcedSigninEnabled =
      self.authService->GetServiceStatus() ==
      AuthenticationService::ServiceStatus::SigninForcedByPolicy;
  self.viewController = [[ManageSyncSettingsTableViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.viewController.title = self.delegate.manageSyncSettingsCoordinatorTitle;
  self.viewController.serviceDelegate = self.mediator;
  self.viewController.presentationDelegate = self;
  self.viewController.modelDelegate = self.mediator;
  self.viewController.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());

  self.mediator.consumer = self.viewController;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  _syncObserver.reset(new SyncObserverBridge(self, self.syncService));
}

- (void)stop {
  [super stop];
  // This coordinator displays the main view and it is in charge to enable sync
  // or not when being closed.
  // Sync changes should only be commited if the user is authenticated.
  if (self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    SyncSetupService* syncSetupService =
        SyncSetupServiceFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    syncSetupService->CommitSyncChanges();
  }
  _syncObserver.reset();
}

#pragma mark - Properties

- (syncer::SyncService*)syncService {
  return SyncServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

#pragma mark - Private

// Closes the Manage sync settings view controller.
- (void)closeManageSyncSettings {
  if (self.viewController.navigationController) {
    if (!_dismissWebAndAppSettingDetailsController.is_null()) {
      std::move(_dismissWebAndAppSettingDetailsController)
          .Run(/*animated*/ false);
    }
    [self.baseNavigationController popToViewController:self.viewController
                                              animated:NO];
    [self.baseNavigationController popViewControllerAnimated:YES];
  }
}

#pragma mark - ManageSyncSettingsTableViewControllerPresentationDelegate

- (void)manageSyncSettingsTableViewControllerWasRemoved:
    (ManageSyncSettingsTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate manageSyncSettingsCoordinatorWasRemoved:self];
}

#pragma mark - ManageSyncSettingsCommandHandler

- (void)openWebAppActivityDialog {
  base::RecordAction(base::UserMetricsAction(
      "Signin_AccountSettings_GoogleActivityControlsClicked"));
  id<SystemIdentity> identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  _dismissWebAndAppSettingDetailsController =
      GetApplicationContext()
          ->GetSystemIdentityManager()
          ->PresentWebAndAppSettingDetailsController(identity,
                                                     self.viewController, YES);
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

- (void)showTurnOffSyncOptionsFromTargetRect:(CGRect)targetRect {
  self.signoutActionSheetCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                            rect:targetRect
                            view:self.viewController.view
                      withSource:signin_metrics::ProfileSignout::
                                     kUserClickedSignoutSettings];
  self.signoutActionSheetCoordinator.delegate = self;
  __weak ManageSyncSettingsCoordinator* weakSelf = self;
  self.signoutActionSheetCoordinator.completion = ^(BOOL success) {
    if (success) {
      [weakSelf closeManageSyncSettings];
    }
  };
  [self.signoutActionSheetCoordinator start];
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

- (void)openPassphraseDialog {
  DCHECK(self.mediator.shouldEncryptionItemBeEnabled);
  UIViewController<SettingsRootViewControlling>* controllerToPush;
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  if (self.syncService->GetUserSettings()->IsPassphraseRequired()) {
    controllerToPush = [[SyncEncryptionPassphraseTableViewController alloc]
        initWithBrowser:self.browser];
  } else {
    controllerToPush = [[SyncEncryptionTableViewController alloc]
        initWithBrowser:self.browser];
  }
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  controllerToPush.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());
  [self.baseNavigationController pushViewController:controllerToPush
                                           animated:YES];
}

- (void)openTrustedVaultReauthForFetchKeys {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  [applicationCommands
      showTrustedVaultReauthForFetchKeysFromViewController:self.viewController
                                                   trigger:
                                                       syncer::
                                                           TrustedVaultUserActionTriggerForUMA::
                                                               kSettings];
}

- (void)openTrustedVaultReauthForDegradedRecoverability {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  [applicationCommands
      showTrustedVaultReauthForDegradedRecoverabilityFromViewController:
          self.viewController
                                                                trigger:
                                                                    syncer::TrustedVaultUserActionTriggerForUMA::
                                                                        kSettings];
}

- (void)openReauthDialogAsSyncIsInAuthError {
  AuthenticationService* authService = self.authService;
  id<SystemIdentity> identity =
      authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (authService->HasCachedMDMErrorForIdentity(identity)) {
    authService->ShowMDMErrorDialogForIdentity(identity);
    return;
  }
  // Sync enters in a permanent auth error state when fetching an access token
  // fails with invalid credentials. This corresponds to Gaia responding with an
  // "invalid grant" error. The current implementation of the iOS SSOAuth
  // library user by Chrome removes the identity from the device when receiving
  // an "invalid grant" response, which leads to the account being also signed
  // out of Chrome. So the sync permanent auth error is a transient state on
  // iOS. The decision was to avoid handling this error in the UI, which means
  // that the reauth dialog is not actually presented on iOS.
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (self.signOutFlowInProgress) {
    return;
  }
  syncer::SyncService::DisableReasonSet disableReasons =
      self.syncService->GetDisableReasons();
  syncer::SyncService::DisableReasonSet userChoiceDisableReason =
      syncer::SyncService::DisableReasonSet(
          syncer::SyncService::DISABLE_REASON_USER_CHOICE);
  // Manage sync settings needs to stay opened if sync is disabled with
  // DISABLE_REASON_USER_CHOICE. Manage sync settings is the only way for a
  // user to turn on the sync engine (and remove DISABLE_REASON_USER_CHOICE).
  // The sync engine turned back on automatically by enabling any datatype.
  if (!disableReasons.Empty() && disableReasons != userChoiceDisableReason) {
    [self closeManageSyncSettings];
  }
}

@end
