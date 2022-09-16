// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/tangible_sync/tangible_sync_screen_coordinator.h"

#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
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
  _tangibleSyncCoordinator = [[TangibleSyncCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  __weak __typeof(self) weakSelf = self;
  _tangibleSyncCoordinator.coordinatorCompleted = ^(bool success) {
    [weakSelf tangibleSyncCoordinatorCompletedWithSuccess:success];
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

// Dismisses the current screen, and stops the FRE if `success` is `false`.
- (void)tangibleSyncCoordinatorCompletedWithSuccess:(bool)success {
  if (success) {
    [_delegate skipAllScreens];
  } else {
    [_delegate screenWillFinishPresenting];
  }
}

@end
