// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/animated_lens/coordinator/animated_lens_promo_coordinator.h"

#import "ios/chrome/browser/first_run/ui_bundled/animated_lens/ui/animated_lens_promo_view_controller.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"

@implementation AnimatedLensPromoCoordinator {
  // Animated Lens Promo view controller.
  AnimatedLensPromoViewController* _viewController;
}

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
  _viewController = [[AnimatedLensPromoViewController alloc] init];
  _viewController.delegate = self;
  _viewController.shouldHideBanner = YES;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _viewController ]
                                           animated:animated];
}

- (void)stop {
  _viewController = nil;
  _viewController.delegate = nil;
  self.firstRunDelegate = nil;
  [super stop];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self.firstRunDelegate screenWillFinishPresenting];
}

@end
