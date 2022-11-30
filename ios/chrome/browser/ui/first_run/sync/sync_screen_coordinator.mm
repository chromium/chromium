// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/consent_auditor/consent_auditor_factory.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_prompt/enterprise_prompt_coordinator.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browsing_data_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator_delegate.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_view_controller.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#import "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SyncScreenCoordinator () <EnterprisePromptCoordinatorDelegate,
                                     PolicyWatcherBrowserAgentObserving,
                                     SyncScreenMediatorDelegate,
                                     SyncScreenViewControllerDelegate> {
  // Observer for the sign-out policy changes.
  std::unique_ptr<PolicyWatcherBrowserAgentObserverBridge>
      _policyWatcherObserverBridge;
}

// Sync screen view controller.
@property(nonatomic, strong) SyncScreenViewController* viewController;
// Sync screen mediator.
@property(nonatomic, strong) SyncScreenMediator* mediator;

@property(nonatomic, weak) id<FirstRunScreenDelegate> delegate;

// The coordinator that manages enterprise prompts.
@property(nonatomic, strong)
    EnterprisePromptCoordinator* enterprisePromptCoordinator;

// The consent string ids of texts on the sync screen.
@property(nonatomic, strong, readonly) NSMutableArray* consentStringIDs;

// Whether the user requested the advanced settings when starting the sync.
@property(nonatomic, assign) BOOL advancedSettingsRequested;

// Coordinator that handles the advanced settings sign-in UI.
@property(nonatomic, strong)
    SigninCoordinator* advancedSettingsSigninCoordinator;

// YES if this coordinator is currently used in the First Run context.
@property(nonatomic, assign) BOOL firstRun;

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
    _consentStringIDs = [NSMutableArray array];
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
    [self.delegate screenWillFinishPresenting];
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
    [self.delegate screenWillFinishPresenting];
    return;
  }

  // Determine if it is currently in the First Run context.
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  AppState* appState = sceneState.appState;
  self.firstRun = appState.initStage == InitStageFirstRun;

  self.viewController = [[SyncScreenViewController alloc] init];
  self.viewController.delegate = self;
  PrefService* prefService = browserState->GetPrefs();
  self.viewController.syncTypesRestricted = HasManagedSyncDataType(prefService);
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

  if (self.firstRun) {
    base::UmaHistogramEnumeration("FirstRun.Stage",
                                  first_run::kSyncScreenStart);
  }

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
  [self.enterprisePromptCoordinator stop];
  self.enterprisePromptCoordinator = nil;
  // If advancedSettingsSigninCoordinator wasn't dismissed yet (which can
  // happen when closing the scene), try to call -interruptWithAction: to
  // properly cleanup the coordinator.
  SigninCoordinator* signinCoordiantor = self.advancedSettingsSigninCoordinator;
  [self.advancedSettingsSigninCoordinator
      interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
               completion:^() {
                 [signinCoordiantor stop];
               }];
  [self.advancedSettingsSigninCoordinator stop];
  self.advancedSettingsSigninCoordinator = nil;
}

#pragma mark - SyncScreenViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self startSyncOrAdvancedSettings:NO];
}

- (void)didTapSecondaryActionButton {
  if (self.firstRun) {
    base::UmaHistogramEnumeration("FirstRun.Stage",
                                  first_run::kSyncScreenCompletionWithoutSync);
  }
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  // Call StopAndClear() to clear the encryption passphrase, in case the
  // user entered it before canceling the sync opt-in flow, and also to set
  // sync as requested.
  syncService->StopAndClear();
  [self.delegate screenWillFinishPresenting];
}

- (void)didTapURLInDisclaimer:(NSURL*)URL {
  // Currently there is only one link to show sync settings in the disclaimer.
  [self startSyncOrAdvancedSettings:YES];
}

- (void)addConsentStringID:(const int)stringID {
  [self.consentStringIDs addObject:[NSNumber numberWithInt:stringID]];
}

- (void)logScrollButtonVisible:(BOOL)scrollButtonVisible {
  if (!self.firstRun) {
    return;
  }
  RecordFirstRunScrollButtonVisibilityMetrics(
      first_run::FirstRunScreenType::kSyncScreenWithoutIdentityPicker,
      scrollButtonVisible);
}

#pragma mark - SyncScreenMediatorDelegate

- (void)syncScreenMediatorDidSuccessfulyFinishSignin:
    (SyncScreenMediator*)mediator {
  if (self.advancedSettingsRequested) {
    // TODO(crbug.com/1256784): Log a UserActions histogram to track the touch
    // interactions on the advanced settings button.
    [self showAdvancedSettings];
  } else {
    if (self.firstRun) {
      base::UmaHistogramEnumeration("FirstRun.Stage",
                                    first_run::kSyncScreenCompletionWithSync);
    }
    [self.delegate screenWillFinishPresenting];
  }
}

- (void)userRemoved {
  [self.delegate screenWillFinishPresenting];
}

#pragma mark - PolicyWatcherBrowserAgentObserving

- (void)policyWatcherBrowserAgentNotifySignInDisabled:
    (PolicyWatcherBrowserAgent*)policyWatcher {
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf openEnterprisePromptDialog];
  };
  if (!self.advancedSettingsSigninCoordinator) {
    completion();
    return;
  }
  [self.advancedSettingsSigninCoordinator
      interruptWithAction:SigninCoordinatorInterruptActionDismissWithAnimation
               completion:completion];
}

#pragma mark - EnterprisePromptCoordinatorDelegate

- (void)hideEnterprisePrompForLearnMore:(BOOL)learnMore {
  [self dismissSignedOutModalAndSkipScreens:learnMore];
}

#pragma mark - InterruptibleChromeCoordinator

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  if (self.advancedSettingsSigninCoordinator) {
    [self.advancedSettingsSigninCoordinator interruptWithAction:action
                                                     completion:completion];
  } else {
    if (completion) {
      completion();
    }
  }
}

#pragma mark - Private

// Dismisses the Signed Out modal if it is still present and `skipScreens`.
- (void)dismissSignedOutModalAndSkipScreens:(BOOL)skipScreens {
  [self.enterprisePromptCoordinator stop];
  self.enterprisePromptCoordinator = nil;
  [self.delegate skipAllScreens];
}

// Starts syncing or opens `advancedSettings`.
- (void)startSyncOrAdvancedSettings:(BOOL)advancedSettings {
  self.advancedSettingsRequested = advancedSettings;

  id<SystemIdentity> identity =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  PostSignInAction postSignInAction = advancedSettings
                                          ? POST_SIGNIN_ACTION_NONE
                                          : POST_SIGNIN_ACTION_COMMIT_SYNC;
  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:identity
                                 postSignInAction:postSignInAction
                         presentingViewController:self.viewController];
  authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);
  authenticationFlow.delegate = self.viewController;

  [self.mediator
            startSyncWithConfirmationID:self.viewController.activateSyncButtonID
                             consentIDs:self.consentStringIDs
                     authenticationFlow:authenticationFlow
      advancedSyncSettingsLinkWasTapped:advancedSettings];
}

// Shows the advanced sync settings.
- (void)showAdvancedSettings {
  DCHECK(!self.advancedSettingsSigninCoordinator);

  const IdentitySigninState signinState =
      IdentitySigninStateSignedInWithSyncDisabled;

  self.advancedSettingsSigninCoordinator = [SigninCoordinator
      advancedSettingsSigninCoordinatorWithBaseViewController:
          self.viewController
                                                      browser:self.browser
                                                  signinState:signinState];
  __weak __typeof(self) weakSelf = self;
  self.advancedSettingsSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult advancedSigninResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf onAdvancedSettingsFinished];
      };
  [self.advancedSettingsSigninCoordinator start];
}

- (void)onAdvancedSettingsFinished {
  DCHECK(self.advancedSettingsSigninCoordinator);

  [self.advancedSettingsSigninCoordinator stop];
  self.advancedSettingsSigninCoordinator = nil;
}

// Opens EnterprisePromptCoordinator.
- (void)openEnterprisePromptDialog {
  self.enterprisePromptCoordinator = [[EnterprisePromptCoordinator alloc]
      initWithBaseViewController:self.viewController
                         browser:self.browser
                      promptType:EnterprisePromptTypeForceSignOut];
  self.enterprisePromptCoordinator.delegate = self;
  [self.enterprisePromptCoordinator start];
}

@end
