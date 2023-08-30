// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/consent_auditor/consent_auditor_factory.h"
#import "ios/chrome/browser/first_run/first_run_metrics.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/elements/activity_overlay_coordinator.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_mediator.h"
#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_mediator_delegate.h"
#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_view_controller.h"
#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_view_controller_delegate.h"
#import "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"

namespace {

constexpr signin_metrics::AccessPoint kTangibleSyncAccessPoint =
    signin_metrics::AccessPoint::ACCESS_POINT_START_PAGE;

}  // namespace

@interface TangibleSyncCoordinator () <AuthenticationFlowDelegate,
                                       TangibleSyncMediatorDelegate,
                                       TangibleSyncViewControllerDelegate>
@end

@implementation TangibleSyncCoordinator {
  // Tangible mediator.
  TangibleSyncMediator* _mediator;
  // Tangible view controller.
  TangibleSyncViewController* _viewController;
  // Whether the user requested the advanced settings when starting the sync.
  BOOL _advancedSettingsRequested;
  // Coordinator that handles the advanced settings sign-in UI.
  SigninCoordinator* _advancedSettingsSigninCoordinator;
  // This array contains the exaustive list of string ids displayed on the
  // sync tangible screen. This list is recorded when the screen is confirmed.
  NSMutableArray* _consentStringIDs;
  // `YES` if coordinator used during the first run.
  BOOL _firstRun;
  ActivityOverlayCoordinator* _activityOverlayCoordinator;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        firstRun:(BOOL)firstRun {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
    _baseNavigationController = navigationController;
    _consentStringIDs = [NSMutableArray array];
    _firstRun = firstRun;
  }
  return self;
}

- (void)start {
  [super start];
  _viewController = [[TangibleSyncViewController alloc] init];
  _viewController.delegate = self;
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  ChromeAccountManagerService* chromeAccountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  _mediator = [[TangibleSyncMediator alloc]
      initWithAuthenticationService:authenticationService
        chromeAccountManagerService:chromeAccountManagerService
                     consentAuditor:ConsentAuditorFactory::GetForBrowserState(
                                        browserState)
                    identityManager:IdentityManagerFactory::GetForBrowserState(
                                        browserState)
                        syncService:SyncServiceFactory::GetForBrowserState(
                                        browserState)
                   syncSetupService:SyncSetupServiceFactory::GetForBrowserState(
                                        browserState)
              unifiedConsentService:UnifiedConsentServiceFactory::
                                        GetForBrowserState(browserState)
                        accessPoint:kTangibleSyncAccessPoint];
  _mediator.consumer = _viewController;
  _mediator.delegate = self;
  if (_firstRun) {
    _viewController.modalInPresentation = YES;
    base::UmaHistogramEnumeration("FirstRun.Stage",
                                  first_run::kTangibleSyncScreenStart);
  }
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _viewController ]
                                           animated:animated];
}

- (void)stop {
  [super stop];
  [_mediator disconnect];
  _mediator = nil;
  _viewController = nil;
}

#pragma mark - AuthenticationFlowDelegate

- (void)didPresentDialog {
  [self setUIEnabled:YES];
}

- (void)didDismissDialog {
  [self setUIEnabled:NO];
}

#pragma mark - TangibleSyncMediatorDelegate

- (void)tangibleSyncMediatorDidSuccessfulyFinishSignin:
    (TangibleSyncMediator*)mediator {
  if (_advancedSettingsRequested) {
    // TODO(crbug.com/1256784): Log a UserActions histogram to track the touch
    // interactions on the advanced settings button.
    [self showAdvancedSettings];
  } else {
    if (_firstRun) {
      base::UmaHistogramEnumeration(
          "FirstRun.Stage", first_run::kTangibleSyncScreenCompletionWithSync);
    }
    DCHECK(self.coordinatorCompleted);
    self.coordinatorCompleted();
    self.coordinatorCompleted = nil;
  }
}

- (void)tangibleSyncMediatorUserRemoved:(TangibleSyncMediator*)mediator {
  DCHECK(self.coordinatorCompleted);
  self.coordinatorCompleted();
  self.coordinatorCompleted = nil;
}

- (void)tangibleSyncMediator:(TangibleSyncMediator*)mediator
                   UIEnabled:(BOOL)UIEnabled {
  [self setUIEnabled:UIEnabled];
}

#pragma mark - TangibleSyncViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self startSyncOrAdvancedSettings:NO];
}

- (void)didTapSecondaryActionButton {
  if (_firstRun) {
    base::UmaHistogramEnumeration(
        "FirstRun.Stage", first_run::kTangibleSyncScreenCompletionWithoutSync);
  }
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState());
  // Call StopAndClear() to clear the encryption passphrase, in case the
  // user entered it before canceling the sync opt-in flow, and also to set
  // sync as requested.
  syncService->StopAndClear();
  DCHECK(self.coordinatorCompleted);
  self.coordinatorCompleted();
  self.coordinatorCompleted = nil;
}

- (void)didTapURLInDisclaimer:(NSURL*)URL {
  // Currently there is only one link to show sync settings in the disclaimer.
  [self startSyncOrAdvancedSettings:YES];
}

// Adds consent string ID.
- (void)addConsentStringID:(const int)stringID {
  [_consentStringIDs addObject:[NSNumber numberWithInt:stringID]];
}

#pragma mark - Private

// Starts sign-in flow. If `advancedSettings` is `NO`, sync is started with the
// sign-in.
- (void)startSyncOrAdvancedSettings:(BOOL)advancedSettings {
  _advancedSettingsRequested = advancedSettings;

  id<SystemIdentity> identity =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          ->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  PostSignInAction postSignInAction = advancedSettings
                                          ? PostSignInAction::kNone
                                          : PostSignInAction::kCommitSync;
  AuthenticationFlow* authenticationFlow =
      [[AuthenticationFlow alloc] initWithBrowser:self.browser
                                         identity:identity
                                      accessPoint:kTangibleSyncAccessPoint
                                 postSignInAction:postSignInAction
                         presentingViewController:_viewController];
  authenticationFlow.dispatcher = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), BrowsingDataCommands);
  authenticationFlow.delegate = self;

  [_mediator startSyncWithConfirmationID:_viewController.activateSyncButtonID
                              consentIDs:_consentStringIDs
                      authenticationFlow:authenticationFlow
       advancedSyncSettingsLinkWasTapped:advancedSettings];
}

// Shows the advanced sync settings.
- (void)showAdvancedSettings {
  DCHECK(!_advancedSettingsSigninCoordinator);

  const IdentitySigninState signinState =
      IdentitySigninStateSignedInWithSyncDisabled;

  _advancedSettingsSigninCoordinator = [SigninCoordinator
      advancedSettingsSigninCoordinatorWithBaseViewController:_viewController
                                                      browser:self.browser
                                                  signinState:signinState];
  __weak __typeof(self) weakSelf = self;
  _advancedSettingsSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult advancedSigninResult,
        SigninCompletionInfo* signinCompletionInfo) {
        [weakSelf onAdvancedSettingsFinished];
      };
  [_advancedSettingsSigninCoordinator start];
}

// Stops `_advancedSettingsSigninCoordinator`.
- (void)onAdvancedSettingsFinished {
  DCHECK(_advancedSettingsSigninCoordinator);
  [_advancedSettingsSigninCoordinator stop];
  _advancedSettingsSigninCoordinator = nil;
}

// Adds an overlay to block the UI if `UIEnabled` is `YES`, otherwise, removes
// the overlay.
- (void)setUIEnabled:(BOOL)UIEnabled {
  if (UIEnabled) {
    DCHECK(_activityOverlayCoordinator);
    [_activityOverlayCoordinator stop];
    _activityOverlayCoordinator = nil;
  } else {
    DCHECK(!_activityOverlayCoordinator);
    _activityOverlayCoordinator = [[ActivityOverlayCoordinator alloc]
        initWithBaseViewController:_viewController
                           browser:self.browser];
    [_activityOverlayCoordinator start];
  }
}

@end
