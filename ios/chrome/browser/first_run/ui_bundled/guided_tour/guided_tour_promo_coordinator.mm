// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/guided_tour/guided_tour_promo_coordinator.h"

#import "ios/chrome/browser/first_run/ui_bundled/guided_tour/guided_tour_promo_view_controller.h"

@interface GuidedTourPromoCoordinator () <PromoStyleViewControllerDelegate>

@end

@implementation GuidedTourPromoCoordinator {
  GuidedTourPromoViewController* _viewController;
}

- (void)start {
  _viewController = [[GuidedTourPromoViewController alloc] init];
  _viewController.delegate = self;
  UISheetPresentationController* sheet =
      _viewController.sheetPresentationController;
  sheet.detents = @[ [UISheetPresentationControllerDetent mediumDetent] ];
  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stopWithCompletion:(ProceduralBlock)completion {
  [_viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:completion];
  _viewController = nil;
}

- (void)didTapPrimaryActionButton {
  [self.delegate startGuidedTour];
}

- (void)didTapSecondaryActionButton {
  [self.delegate dismissGuidedTourPromo];
}

@end
