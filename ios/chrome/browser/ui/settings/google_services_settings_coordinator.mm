// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services_settings_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/google/core/common/google_util.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_local_commands.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync_encryption_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync_encryption_passphrase_collection_view_controller.h"
#include "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_browser_opener.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface GoogleServicesSettingsCoordinator ()<
    ChromeIdentityBrowserOpener,
    GoogleServicesSettingsLocalCommands,
    GoogleServicesSettingsViewControllerPresentationDelegate>

// Google services settings mediator.
@property(nonatomic, strong) GoogleServicesSettingsMediator* mediator;
// Returns the authentication service.
@property(nonatomic, assign, readonly) AuthenticationService* authService;
// Manages the authentication flow for a given identity.
@property(nonatomic, strong) AuthenticationFlow* authenticationFlow;
// View controller presented by this coordinator.
@property(nonatomic, strong, readonly)
    GoogleServicesSettingsViewController* googleServicesSettingsViewController;

@end

@implementation GoogleServicesSettingsCoordinator

@synthesize viewController = _viewController;
@synthesize delegate = _delegate;
@synthesize dispatcher = _dispatcher;
@synthesize mediator = _mediator;
@synthesize authenticationFlow = _authenticationFlow;

- (void)start {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  GoogleServicesSettingsViewController* viewController =
      [[GoogleServicesSettingsViewController alloc]
          initWithLayout:layout
                   style:CollectionViewControllerStyleAppBar];
  viewController.presentationDelegate = self;
  viewController.localDispatcher = self;
  self.viewController = viewController;
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(self.browserState);
  browser_sync::ProfileSyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
  unified_consent::UnifiedConsentService* unifiedConsentService =
      UnifiedConsentServiceFactory::GetForBrowserState(self.browserState);
  self.mediator = [[GoogleServicesSettingsMediator alloc]
      initWithUserPrefService:self.browserState->GetPrefs()
             localPrefService:GetApplicationContext()->GetLocalState()
                  syncService:syncService
             syncSetupService:syncSetupService
        unifiedConsentService:unifiedConsentService];
  self.mediator.consumer = viewController;
  self.mediator.authService = self.authService;
  viewController.modelDelegate = self.mediator;
  viewController.serviceDelegate = self.mediator;
  DCHECK(self.navigationController);
  [self.navigationController pushViewController:self.viewController
                                       animated:YES];
}

#pragma mark - Private

- (void)closeGoogleActivitySettings:(id)sender {
  [self.navigationController dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - Properties

- (AuthenticationService*)authService {
  return AuthenticationServiceFactory::GetForBrowserState(self.browserState);
}

- (GoogleServicesSettingsViewController*)googleServicesSettingsViewController {
  return base::mac::ObjCCast<GoogleServicesSettingsViewController>(
      self.viewController);
}

#pragma mark - GoogleServicesSettingsLocalCommands

- (void)restartAuthenticationFlow {
  ChromeIdentity* authenticatedIdentity =
      AuthenticationServiceFactory::GetForBrowserState(self.browserState)
          ->GetAuthenticatedIdentity();
  [self.googleServicesSettingsViewController preventUserInteraction];
  DCHECK(!self.authenticationFlow);
  self.authenticationFlow = [[AuthenticationFlow alloc]
          initWithBrowserState:self.browserState
                      identity:authenticatedIdentity
               shouldClearData:SHOULD_CLEAR_DATA_USER_CHOICE
              postSignInAction:POST_SIGNIN_ACTION_START_SYNC
      presentingViewController:self.viewController];
  self.authenticationFlow.dispatcher = self.dispatcher;
  __weak GoogleServicesSettingsCoordinator* weakSelf = self;
  [self.authenticationFlow startSignInWithCompletion:^(BOOL success) {
    // TODO(crbug.com/889919): Needs to add histogram for |success|.
    [weakSelf.googleServicesSettingsViewController allowUserInteraction];
  }];
}

- (void)openReauthDialogAsSyncIsInAuthError {
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
  [self.dispatcher
      showSyncPassphraseSettingsFromViewController:self.viewController];
}

- (void)openGoogleActivityControlsDialog {
  base::RecordAction(base::UserMetricsAction(
      "Signin_AccountSettings_GoogleActivityControlsClicked"));
  UINavigationController* settingsDetails =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->CreateWebAndAppSettingDetailsController(
              self.authService->GetAuthenticatedIdentity(), self);
  UIImage* closeIcon = [ChromeIcon closeIcon];
  SEL action = @selector(closeGoogleActivitySettings:);
  [settingsDetails.topViewController navigationItem].leftBarButtonItem =
      [ChromeIcon templateBarButtonItemWithImage:closeIcon
                                          target:self
                                          action:action];
  [self.navigationController presentViewController:settingsDetails
                                          animated:YES
                                        completion:nil];
}

- (void)openEncryptionDialog {
  browser_sync::ProfileSyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
  SettingsRootCollectionViewController* controllerToPush;
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  if (syncService->IsPassphraseRequired()) {
    controllerToPush = [[SyncEncryptionPassphraseCollectionViewController alloc]
        initWithBrowserState:self.browserState];
  } else {
    controllerToPush = [[SyncEncryptionCollectionViewController alloc]
        initWithBrowserState:self.browserState];
  }
  controllerToPush.dispatcher = self.dispatcher;
  [self.navigationController pushViewController:controllerToPush animated:YES];
}

- (void)openManageSyncedDataWebPage {
  GURL learnMoreUrl = google_util::AppendGoogleLocaleParam(
      GURL(kSyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:learnMoreUrl];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

#pragma mark - GoogleServicesSettingsViewControllerPresentationDelegate

- (void)googleServicesSettingsViewControllerDidRemove:
    (GoogleServicesSettingsViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate googleServicesSettingsCoordinatorDidRemove:self];
}

#pragma mark - ChromeIdentityBrowserOpener

- (void)openURL:(NSURL*)URL
              view:(UIView*)view
    viewController:(UIViewController*)viewController {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:net::GURLWithNSURL(URL)];
  [self.dispatcher closeSettingsUIAndOpenURL:command];
}

@end
