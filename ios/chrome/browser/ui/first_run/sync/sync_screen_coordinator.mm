// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync/sync_screen_coordinator.h"

#import "base/metrics/histogram_functions.h"
#include "ios/chrome/browser/first_run/first_run_metrics.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent.h"
#import "ios/chrome/browser/policy/policy_watcher_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/consent_auditor_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_policy_signout_coordinator.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_mediator.h"
#import "ios/chrome/browser/ui/first_run/sync/sync_screen_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SyncScreenCoordinator () <PolicyWatcherBrowserAgentObserving,
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

  if (!authenticationService->GetAuthenticatedIdentity()) {
    // Don't show sync screen if no logged-in user account.
    return [self.delegate willFinishPresenting];
  }

  self.viewController = [[SyncScreenViewController alloc] init];
  self.viewController.delegate = self;

  // Setup mediator.
  self.mediator = [[SyncScreenMediator alloc]
      initWithAuthenticationService:authenticationService
                    identityManager:IdentityManagerFactory::GetForBrowserState(
                                        browserState)
                     consentAuditor:ConsentAuditorFactory::GetForBrowserState(
                                        browserState)
                   syncSetupService:SyncSetupServiceFactory::GetForBrowserState(
                                        browserState)];

  base::UmaHistogramEnumeration("FirstRun.Stage", first_run::kSyncScreenStart);

  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ self.viewController ]
                                           animated:animated];
  if (@available(iOS 13, *)) {
    self.viewController.modalInPresentation = YES;
  }
}

- (void)stop {
  PolicyWatcherBrowserAgent::FromBrowser(self.browser)
      ->RemoveObserver(_policyWatcherObserverBridge.get());

  self.delegate = nil;
  self.viewController = nil;
  self.mediator = nil;
  [self.policySignoutPromptCoordinator stop];
  self.policySignoutPromptCoordinator = nil;
}

#pragma mark - SyncScreenViewControllerDelegate

- (void)didTapPrimaryActionButton {
  base::UmaHistogramEnumeration("FirstRun.Stage",
                                first_run::kSyncScreenCompletionWithSync);
  [self.mediator startSyncWithConfirmationID:
                     IDS_IOS_FIRST_RUN_SYNC_SCREEN_PRIMARY_ACTION
                                  consentIDs:self.consentStringIDs
           advancedSyncSettingsLinkWasTapped:NO];
  [self.delegate willFinishPresenting];
}

- (void)didTapSecondaryActionButton {
  base::UmaHistogramEnumeration("FirstRun.Stage",
                                first_run::kSyncScreenCompletionWithoutSync);
  [self.delegate willFinishPresenting];
}

- (void)showSyncSettings {
  base::UmaHistogramEnumeration(
      "FirstRun.Stage", first_run::kSyncScreenCompletionWithSyncSettings);
  [self.mediator startSyncWithConfirmationID:
                     IDS_IOS_FIRST_RUN_SYNC_SCREEN_ADVANCE_SETTINGS
                                  consentIDs:self.consentStringIDs
           advancedSyncSettingsLinkWasTapped:YES];
  [self.delegate skipAllAndShowSyncSettings];
}

- (void)addConsentStringID:(const int)stringID {
  [self.consentStringIDs addObject:[NSNumber numberWithInt:stringID]];
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

@end
