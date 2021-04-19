// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/sync_screen_coordinator.h"

#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/first_run/sync_screen_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SyncScreenCoordinator () <SyncScreenViewControllerDelegate>

// Sync screen view controller.
@property(nonatomic, strong) SyncScreenViewController* viewController;

@end

@implementation SyncScreenCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  // TODO(crbug.com/1189840): Check if sync screen need to be shown.
  // if not:
  // [self.delegate willFinishPresenting]
  // if yes:
  self.viewController = [[SyncScreenViewController alloc] init];
  // TODO(crbug.com/1189840): once the view controller's delegate is unified
  // with the base FirstRunScreenViewController delegate, change this back to
  // self.viewController.delegate = self;
  self.viewController.delegate2 = self;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  self.delegate = nil;
  self.viewController = nil;
}

#pragma mark - SyncScreenViewControllerDelegate

- (void)continueWithSync {
  // TODO(crbug.com/1189840): record sync status.
  [self.delegate willFinishPresenting];
}

- (void)continueWithoutSync {
  // TODO(crbug.com/1189840): record sync status.
  [self.delegate willFinishPresenting];
}

- (void)showSyncSettings {
  // TODO(crbug.com/1189840): show settings UI.
}

@end
