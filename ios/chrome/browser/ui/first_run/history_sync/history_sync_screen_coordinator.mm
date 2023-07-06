// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/history_sync/history_sync_screen_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface HistorySyncScreenCoordinator () <HistorySyncCoordinatorDelegate>
@end

@implementation HistorySyncScreenCoordinator {
  __weak id<FirstRunScreenDelegate> _delegate;
  BOOL _firstRun;
  // Coordinator to display the history sync view.
  HistorySyncCoordinator* _historySyncCoordinator;
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
  _historySyncCoordinator = [[HistorySyncCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                              delegate:self
                              firstRun:YES];
  [_historySyncCoordinator start];
}

- (void)stop {
  [super stop];
  [_historySyncCoordinator stop];
  _historySyncCoordinator = nil;
  _baseNavigationController = nil;
}

- (void)dealloc {
  // TODO(crbug.com/1454777)
  DUMP_WILL_BE_CHECK(!_historySyncCoordinator);
}

#pragma mark - HistorySyncCoordinatorDelegate

// Dismisses the current screen.
- (void)closeHistorySyncCoordinator:
    (HistorySyncCoordinator*)historySyncCoordinator {
  [_delegate screenWillFinishPresenting];
}

@end
