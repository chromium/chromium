// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/coordinator/interactive_lens_promo_coordinator.h"

#import "base/check.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_screen_delegate.h"
#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/interactive_lens_overlay_promo_view_controller.h"

@interface InteractiveLensPromoCoordinator () <InteractiveLensPromoDelegate>
@end

@implementation InteractiveLensPromoCoordinator {
  // View controller for the Interactive Lens promo screen.
  InteractiveLensOverlayPromoViewController* _promoViewController;
  // Container view controller for the Lens view.
  UIViewController* _lensContainerViewController;
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
  // TODO(crbug.com/416480202): Trigger the Lens UI presentation with
  // `_lensContainerViewController` as the base view controller before
  // presenting `_promoViewController`.
  _lensContainerViewController = [[UIViewController alloc] init];
  _promoViewController = [[InteractiveLensOverlayPromoViewController alloc]
      initWithLensView:_lensContainerViewController.view];
  _promoViewController.delegate = self;
  _promoViewController.modalInPresentation = YES;
  BOOL animated = self.baseNavigationController.topViewController != nil;
  [self.baseNavigationController setViewControllers:@[ _promoViewController ]
                                           animated:animated];
}

- (void)stop {
  self.firstRunDelegate = nil;
  _lensContainerViewController = nil;
  _promoViewController = nil;
  [super stop];
}

#pragma mark - InteractiveLensPromoDelegate

- (void)didTapContinueButton {
  CHECK(self.firstRunDelegate);
  [self.firstRunDelegate screenWillFinishPresenting];
}

@end
