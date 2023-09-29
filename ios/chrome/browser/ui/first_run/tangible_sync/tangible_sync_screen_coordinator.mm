// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/tangible_sync/tangible_sync_screen_coordinator.h"

#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/tangible_sync/tangible_sync_coordinator.h"

@implementation TangibleSyncScreenCoordinator {
  // First run screen delegate.
  __weak id<FirstRunScreenDelegate> _delegate;
  // Coordinator to display the tangible sync view.
  TangibleSyncCoordinator* _tangibleSyncCoordinator;
  BOOL _firstRun;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        firstRun:(BOOL)firstRun
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
    _firstRun = firstRun;
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
  syncer::SyncService* syncService =
      SyncServiceFactory::GetForBrowserState(browserState);

  BOOL shouldSkipSyncScreen =
      syncService->HasDisableReason(
          syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY) ||
      syncService->GetUserSettings()->IsInitialSyncFeatureSetupComplete();
  if (shouldSkipSyncScreen) {
    // Don't show sync screen if sync is disabled.
    [_delegate screenWillFinishPresenting];
    return;
  }
  _tangibleSyncCoordinator = [[TangibleSyncCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                              firstRun:_firstRun];
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
  _delegate = nil;
}

#pragma mark - Private

// Dismisses the current screen.
- (void)tangibleSyncCoordinatorCompleted {
  [_delegate screenWillFinishPresenting];
}

@end
