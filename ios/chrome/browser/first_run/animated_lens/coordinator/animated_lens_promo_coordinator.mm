// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/animated_lens/coordinator/animated_lens_promo_coordinator.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/first_run/animated_lens/ui/animated_lens_promo_view_controller.h"
#import "ios/chrome/browser/first_run/model/first_run_metrics.h"
#import "ios/chrome/browser/first_run/public/first_run_screen_delegate.h"

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

  base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                first_run::kAnimatedLensPromoStart);

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
  base::UmaHistogramEnumeration(
      first_run::kFirstRunStageHistogram,
      first_run::kAnimatedLensPromoCompletionWithAction);
  [self.firstRunDelegate screenWillFinishPresenting];
}

@end
