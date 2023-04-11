// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/tangible_sync/tangible_sync_screen_coordinator.h"

#import "components/sync/driver/sync_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TangibleSyncScreenCoordinator {
  // First run screen delegate.
  __weak id<FirstRunScreenDelegate> _delegate;
  // Coordinator to display the tangible sync view.
  TangibleSyncCoordinator* _tangibleSyncCoordinator;
}

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
  }
  return self;
}

- (void)start {
  [super start];
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(browserState);
  if (!authenticationService->GetPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    // Don't show sync screen if no logged-in user account.
    [_delegate screenWillFinishPresenting];
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
    // Don't show sync screen if sync is disabled.
    [_delegate screenWillFinishPresenting];
    return;
  }
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  AppState* appState = sceneState.appState;
  // This screen can only be used for First Run.
  DCHECK(appState.initStage == InitStageFirstRun);
  _tangibleSyncCoordinator = [[TangibleSyncCoordinator alloc]
      initFirstRunWithBaseNavigationController:self.baseNavigationController
                                       browser:self.browser];
  __weak __typeof(self) weakSelf = self;
  _tangibleSyncCoordinator.coordinatorCompleted = ^() {
    [weakSelf tangibleSyncCoordinatorCompleted];
  };
  [_tangibleSyncCoordinator start];
}

- (void)stop {
  [super stop];
  [_tangibleSyncCoordinator stop];
  _tangibleSyncCoordinator.coordinatorCompleted = nil;
  _tangibleSyncCoordinator = nil;
  _baseNavigationController = nil;
}

#pragma mark - Private

// Dismisses the current screen.
- (void)tangibleSyncCoordinatorCompleted {
  [_delegate screenWillFinishPresenting];
}

@end
