// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/coordinator/interactive_lens_promo_coordinator.h"

#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"

@implementation InteractiveLensPromoCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  // TODO(crbug.com/416480202): Present screen.
}

- (void)stop {
  self.firstRunDelegate = nil;
  [super stop];
}

@end
