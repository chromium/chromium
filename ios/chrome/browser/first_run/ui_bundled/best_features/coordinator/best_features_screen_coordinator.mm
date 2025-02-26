// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/best_features/coordinator/best_features_screen_coordinator.h"

#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

@implementation BestFeaturesScreenCoordinator {
  // First run screen delegate.
  __weak id<FirstRunScreenDelegate> _delegate;
}
@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        delegate:(id<FirstRunScreenDelegate>)
                                                     delegate {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _baseNavigationController = navigationController;
    _delegate = delegate;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
}

- (void)stop {
  _delegate = nil;
  [super stop];
}

@end
