// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/history_sync/history_sync_screen_coordinator.h"

#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  _historySyncCoordinator = [[HistorySyncCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  __weak __typeof(self) weakSelf = self;
  _historySyncCoordinator.coordinatorCompleted = ^(bool success) {
    [weakSelf historySyncCoordinatorCompletedWithSuccess:success];
  };
  [_historySyncCoordinator start];
}

- (void)stop {
  [super stop];
  [_historySyncCoordinator stop];
  _historySyncCoordinator.coordinatorCompleted = nil;
  _historySyncCoordinator = nil;
  _baseNavigationController = nil;
}

- (void)dealloc {
  // TODO(crbug.com/1454777)
  DUMP_WILL_BE_CHECK(!_historySyncCoordinator);
}

#pragma mark - Private

// Dismisses the current screen, and stops the FRE if `success` is `false`.
- (void)historySyncCoordinatorCompletedWithSuccess:(bool)success {
  if (success) {
    [_delegate skipAllScreens];
  } else {
    [_delegate screenWillFinishPresenting];
  }
}

@end
