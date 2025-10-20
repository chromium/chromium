// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator.h"

#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_coordinator_delegate.h"

@implementation SyncedSetUpCoordinator {
  // Parameters relevant to understanding the app startup, used to determine
  // how the Synced Set Up flow should be presented.
  AppStartupParameters* _startupParameters;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                         startupParameters:
                             (AppStartupParameters*)startupParameters {
  if ((self = [super initWithBaseViewController:viewController
                                        browser:browser])) {
    _startupParameters = startupParameters;
  }
  return self;
}

@end
