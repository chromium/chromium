// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_coordinator.h"

#import "base/metrics/histogram_functions.h"
#include "components/sync/driver/sync_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/consent_auditor_factory.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/enterprise/user_policy_signout/user_policy_signout_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator_delegate.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_view_controller.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SyncScreenCoordinator () <PolicyWatcherBrowserAgentObserving,
                                     SyncScreenMediatorDelegate,
                                     SyncScreenViewControllerDelegate,
                                     UserPolicySignoutCoordinatorDelegate> {
  // Observer for the sign-out policy changes.
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
}

// Sync screen view controller.
@property(nonatomic, strong) SyncScreenViewController* viewController;
// Sync screen mediator.
@property(nonatomic, strong) SyncScreenMediator* mediator;

@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;

// The coordinator that manages the prompt for when the user is signed out due
// to policy.
@property(nonatomic, strong)
    UserPolicySignoutCoordinator* policySignoutPromptCoordinator;

// The consent string ids of texts on the sync screen.
@property(nonatomic, assign, readonly) NSMutableArray* consentStringIDs;

// Whether the user requested the advanced settings when starting the sync.
@property(nonatomic, assign) BOOL advancedSettingsRequested;

@end

@implementation SyncScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
    _baseNavigationController = navigationController;
    _delegate = delegate;
    _policyWatcherObserverBridge =
        std::make_unique<PolicyWatcherBrowserAgentObserverBridge>(self);
  }
  return self;
}

- (void)start {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);

  PolicyWatcherBrowserAgent::FromBrowser(self.browser)
      ->AddObserver(_policyWatcherObserverBridge.get());

  if (!authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // Don't show sync screen if no logged-in user account.
    [self.delegate willFinishPresenting];
    return;
  }

  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(browserState);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);

  BOOL shouldSkipSyncScreen =
      syncService->GetDisableReasons().Has(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      syncSetupService->IsFirstSetupComplete();
  if (shouldSkipSyncScreen) {
    [self.delegate willFinishPresenting];
    return;
  }

  self.viewController = [[SyncScreenViewController alloc] init];
  self.viewController.delegate = self;
  self.viewController.syncTypesRestricted =
      HasManagedSyncDataType(browserState);
  // Setup mediator.
  self.mediator = [[SyncScreenMediator alloc]
      initWithAuthenticationService:authenticationService
                    identityManager:IdentityManagerFactory::GetForBrowserState(
                                        browserState)
              accountManagerService:ChromeAccountManagerServiceFactory::
                                        GetForBrowserState(browserState)
                     consentAuditor:ConsentAuditorFactory::GetForBrowserState(
                                        browserState)
                   syncSetupService:syncSetupService
              unifiedConsentService:UnifiedConsentServiceFactory::
                                        GetForBrowserState(browserState)
                        syncService:SyncServiceFactory::GetForBrowserState(
                                        self.browser->GetBrowserState())];

  self.mediator.delegate = self;
  self.mediator.consumer = self.viewController;

  base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kSyncScreenStart);

  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
  self.viewController.modalInPresentation = YES;
}

- (void)stop {
  PolicyWatcherBrowserAgent::FromBrowser(self.browser)
      ->RemoveObserver(_policyWatcherObserverBridge.get());

  self.delegate = nil;
  self.viewController = nil;
  [self.mediator disconnect];
  self.mediator = nil;
  [self.policySignoutPromptCoordinator stop];
  self.policySignoutPromptCoordinator = nil;
}

#pragma mark - SyncScreenViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self startSyncOrAdvancedSettings:NO];
}

- (void)didTapSecondaryActionButton {
  base::UmaHistogramEnumeration("FirstRun.Stage",
                                first_run::kSyncScreenCompletionWithoutSync);
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  // Call StopAndClear() to clear the encryption passphrase, in case the
  // user entered it before canceling the sync opt-in flow, and also to set
  // sync as requested.
  syncService->StopAndClear();
  [self.delegate willFinishPresenting];
}

- (void)showSyncSettings {
  [self startSyncOrAdvancedSettings:YES];
}

- (void)addConsentStringID:(const int)stringID {
  [self.consentStringIDs addObject:[NSNumber numberWithInt:stringID]];
}

#pragma mark - SyncScreenMediatorDelegate

- (void)syncScreenMediatorDidSuccessfulyFinishSignin:
    (SyncScreenMediator*)mediator {
  if (self.advancedSettingsRequested) {
    base::UmaHistogramEnumeration(
        "FirstRun.Stage", first_run::kSyncScreenCompletionWithSyncSettings);
    id<ApplicationCommands> handler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    [handler showAdvancedSigninSettingsFromViewController:
                 self.baseNavigationController];
  } else {
    base::UmaHistogramEnumeration("FirstRun.Stage",
                                  first_run::kSyncScreenCompletionWithSync);
    [self.delegate willFinishPresenting];
  }
}

- (void)userRemoved {
  [self.delegate willFinishPresenting];
}

#pragma mark - PolicyWatcherBrowserAgentObserving

- (void)policyWatcherBrowserAgentNotifySignInDisabled:
    (PolicyWatcherBrowserAgent*)policyWatcher {
  self.policySignoutPromptCoordinator = [[UserPolicySignoutCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser];
  self.policySignoutPromptCoordinator.delegate = self;
  [self.policySignoutPromptCoordinator start];
}

#pragma mark - UserPolicySignoutCoordinatorDelegate

- (void)hidePolicySignoutPromptForLearnMore:(BOOL)learnMore {
  [self dismissSignedOutModalAndSkipScreens:learnMore];
}

- (void)userPolicySignoutDidDismiss {
  [self dismissSignedOutModalAndSkipScreens:NO];
}

#pragma mark - Private

// Dismisses the Signed Out modal if it is still present and |skipScreens|.
- (void)dismissSignedOutModalAndSkipScreens:(BOOL)skipScreens {
  [self.policySignoutPromptCoordinator stop];
  self.policySignoutPromptCoordinator = nil;
  [self.delegate skipAll];
}

// Starts syncing or opens |advancedSettings|.
- (void)startSyncOrAdvancedSettings:(BOOL)advancedSettings {
  self.advancedSettingsRequested = advancedSettings;
  int confirmationID = advancedSettings
                           ? self.viewController.openSettingsStringID
                           : self.viewController.activateSyncButtonID;

  ChromeIdentity* identity =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  PostSignInAction postSignInAction = advancedSettings
                                          ? POST_SIGNIN_ACTION_NONE
                                          : POST_SIGNIN_ACTION_COMMIT_SYNC;
  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:identity
                                  shouldClearData:SHOULD_CLEAR_DATA_MERGE_DATA
                                 postSignInAction:postSignInAction
                         presentingViewController:self.viewController];
  authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);
  authenticationFlow.delegate = self.viewController;

  [self.mediator startSyncWithConfirmationID:confirmationID
                                  consentIDs:self.consentStringIDs
                          authenticationFlow:authenticationFlow
           advancedSyncSettingsLinkWasTapped:advancedSettings];
}

@end
