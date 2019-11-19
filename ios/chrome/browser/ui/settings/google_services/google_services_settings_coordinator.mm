// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"

#include "base/mac/foundation_util.h"
#include "components/google/core/common/google_util.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_coordinator.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GoogleServicesSettingsCoordinator () <
    GoogleServicesSettingsCommandHandler,
    GoogleServicesSettingsViewControllerPresentationDelegate,
    ManageSyncSettingsCoordinatorDelegate>

// Google services settings mode.
@property(nonatomic, assign, readonly) GoogleServicesSettingsMode mode;
// Google services settings mediator.
@property(nonatomic, strong) GoogleServicesSettingsMediator* mediator;
// Returns the authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;
// Manages the authentication flow for a given identity.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;
// View controller presented by this coordinator.
@property(nonatomic, strong, readonly)
    GoogleServicesSettingsViewController* googleServicesSettingsViewController;
// The SigninInteractionCoordinator that presents Sign In UI for the
// Settings page.
@property(nonatomic, strong)
    SigninInteractionCoordinator* signinInteractionCoordinator;
// Coordinator to present the manage sync settings.
@property(nonatomic, strong)
    ManageSyncSettingsCoordinator* manageSyncSettingsCoordinator;
// YES if stop has been called.
@property(nonatomic, assign) BOOL stopDone;

@end

@implementation GoogleServicesSettingsCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      mode:(GoogleServicesSettingsMode)mode {
  if ([super initWithBaseViewController:viewController browser:browser]) {
    _mode = mode;
  }
  return self;
}

- (void)dealloc {
  // -[GoogleServicesSettingsCoordinator stop] needs to be called explicitly.
  DCHECK(self.stopDone);
}

- (void)start {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;

  GoogleServicesSettingsViewController* viewController =
      [[GoogleServicesSettingsViewController alloc]
          initWithTableViewStyle:style
                     appBarStyle:ChromeTableViewControllerStyleNoAppBar];
  viewController.presentationDelegate = self;
  self.viewController = viewController;
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(self.browserState);
  self.mediator = [[GoogleServicesSettingsMediator alloc]
      initWithUserPrefService:self.browserState->GetPrefs()
             localPrefService:GetApplicationContext()->GetLocalState()
             syncSetupService:syncSetupService
                         mode:self.mode];
  self.mediator.consumer = viewController;
  self.mediator.authService = self.authService;
  self.mediator.identityManager =
      IdentityManagerFactory::GetForBrowserState(self.browserState);
  self.mediator.commandHandler = self;
  self.mediator.syncService =
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
  viewController.modelDelegate = self.mediator;
  viewController.serviceDelegate = self.mediator;
  DCHECK(self.navigationController);
  [self.navigationController pushViewController:self.viewController
                                       animated:YES];
}

- (void)stop {
  if (self.stopDone) {
    return;
  }
  // Sync changes should only be commited if the user is authenticated and
  // there is no sign-in progress.
  if (self.authService->IsAuthenticated() &&
      !self.signinInteractionCoordinator) {
    SyncSetupService* syncSetupService =
        SyncSetupServiceFactory::GetForBrowserState(self.browserState);
    if (self.mode == GoogleServicesSettingsModeSettings &&
        syncSetupService->GetSyncServiceState() ==
            SyncSetupService::kSyncSettingsNotConfirmed) {
      // If Sync is still in aborted state, this means the user didn't turn on
      // sync, and wants Sync off. To acknowledge, Sync has to be turned off.
      syncSetupService->SetSyncEnabled(false);
    }
    syncSetupService->CommitSyncChanges();
  }
  if (self.signinInteractionCoordinator) {
    [self.signinInteractionCoordinator cancel];
    // |self.signinInteractionCoordinator| is set to nil by
    // the completion block called by -[GoogleServicesSettingsCoordinator
    // signInInteractionCoordinatorDidComplete]
    DCHECK(!self.signinInteractionCoordinator);
  }
  self.stopDone = YES;
}

#pragma mark - Private

- (void)authenticationFlowDidComplete {
  DCHECK(self.authenticationFlow);
  self.authenticationFlow = nil;
  [self.googleServicesSettingsViewController allowUserInteraction];
}

- (void)signInInteractionCoordinatorDidComplete {
  self.signinInteractionCoordinator = nil;
}

#pragma mark - Properties

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForBrowserState(self.browserState);
}

- (GoogleServicesSettingsViewController*)googleServicesSettingsViewController {
  return base::mac::ObjCCast<GoogleServicesSettingsViewController>(
      self.viewController);
}

#pragma mark - GoogleServicesSettingsCommandHandler

- (void)restartAuthenticationFlow {
  ChromeIdentity* authenticatedIdentity =
      AuthenticationServiceFactory::GetForBrowserState(self.browserState)
          ->GetAuthenticatedIdentity();
  [self.googleServicesSettingsViewController preventUserInteraction];
  DCHECK(!self.authenticationFlow);
  self.authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:authenticatedIdentity
                                  shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
                                 postSignInAction:POST_SIGNIN_ACTION_START_SYNC
                         presentingViewController:self.viewController];
  self.authenticationFlow.dispatcher = self.dispatcher;
  __weak GoogleServicesSettingsCoordinator* weakSelf = self;
  [self.authenticationFlow startSignInWithCompletion:^(BOOL success) {
    // TODO(crbug.com/889919): Needs to add histogram for |success|.
    [weakSelf authenticationFlowDidComplete];
  }];
}

- (void)openReauthDialogAsSyncIsInAuthError {
  ChromeIdentity* identity = self.authService->GetAuthenticatedIdentity();
  if (self.authService->HasCachedMDMErrorForIdentity(identity)) {
    self.authService->ShowMDMErrorDialogForIdentity(identity);
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

- (void)openPassphraseDialog {
  SyncEncryptionPassphraseTableViewController* controller =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowserState:self.browserState];
  controller.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)showSignIn {
  signin_metrics::RecordSigninUserActionForAccessPoint(
      signin_metrics::AccessPoint::ACCESS_POINT_GOOGLE_SERVICES_SETTINGS,
      signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO);
  DCHECK(!self.signinInteractionCoordinator);
  self.signinInteractionCoordinator =
      [[SigninInteractionCoordinator alloc] initWithBrowser:self.browser
                                                 dispatcher:self.dispatcher];
  __weak __typeof(self) weakSelf = self;
  [self.signinInteractionCoordinator
            signInWithIdentity:nil
                   accessPoint:signin_metrics::AccessPoint::
                                   ACCESS_POINT_GOOGLE_SERVICES_SETTINGS
                   promoAction:signin_metrics::PromoAction::
                                   PROMO_ACTION_NO_SIGNIN_PROMO
      presentingViewController:self.navigationController
                    completion:^(BOOL success) {
                      [weakSelf signInInteractionCoordinatorDidComplete];
                    }];
}

- (void)openAccountSettings {
  AccountsTableViewController* controller =
      [[AccountsTableViewController alloc] initWithBrowser:self.browser
                                 closeSettingsOnAddAccount:NO];
  [self.navigationController pushViewController:controller animated:YES];
}

- (void)openManageSyncSettings {
  DCHECK(!self.manageSyncSettingsCoordinator);
  self.manageSyncSettingsCoordinator = [[ManageSyncSettingsCoordinator alloc]
      initWithBaseViewController:self.viewController
                    browserState:self.browserState];
  self.manageSyncSettingsCoordinator.dispatcher = self.dispatcher;
  self.manageSyncSettingsCoordinator.navigationController =
      self.navigationController;
  self.manageSyncSettingsCoordinator.delegate = self;
  [self.manageSyncSettingsCoordinator start];
}

- (void)openManageGoogleAccountWebPage {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(kManageYourGoogleAccountURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - GoogleServicesSettingsViewControllerPresentationDelegate

- (void)googleServicesSettingsViewControllerDidRemove:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate googleServicesSettingsCoordinatorDidRemove:self];
}

#pragma mark - ManageSyncSettingsCoordinatorDelegate

- (void)manageSyncSettingsCoordinatorWasPopped:
    (ManageSyncSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.manageSyncSettingsCoordinator, coordinator);
  self.manageSyncSettingsCoordinator = nil;
}

@end
