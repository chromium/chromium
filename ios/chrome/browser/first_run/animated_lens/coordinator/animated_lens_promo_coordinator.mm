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
  // `YES` if presented during the First Run Experience.
  BOOL _firstRun;
}

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                        firstRun:(BOOL)firstRun {
  if ((self = [super initWithBaseViewController:navigationController
                                        browser:browser])) {
    _baseNavigationController = navigationController;
    _firstRun = firstRun;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];

  if (_firstRun) {
    base::UmaHistogramEnumeration(first_run::kFirstRunStageHistogram,
                                  first_run::kAnimatedLensPromoStart);
  }

  _viewController = [[AnimatedLensPromoViewController alloc] init];
  _viewController.delegate = self;
  _viewController.shouldHideBanner = YES;
  _viewController.modalInPresentation = _firstRun;
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
  if (_firstRun) {
    base::UmaHistogramEnumeration(
        first_run::kFirstRunStageHistogram,
        first_run::kAnimatedLensPromoCompletionWithAction);
  }
  [self.firstRunDelegate firstRunScreenCoordinatorWantsToBeStopped:self];
}

@end
