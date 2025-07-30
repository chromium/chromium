// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/first_run/ui_bundled/interactive_lens/ui/lens_overlay_promo_container_view_controller.h"

namespace {
// Corner radius for the top two corners of the Lens view.
const CGFloat kLensViewCornerRadius = 45.0;
}  // namespace

@interface LensOverlayPromoContainerViewController () <
    UIGestureRecognizerDelegate>
@end

@implementation LensOverlayPromoContainerViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.layer.cornerRadius = kLensViewCornerRadius;
  self.view.layer.masksToBounds = YES;
  self.view.layer.maskedCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;

  // Add gesture recognizers to the Lens view to detect interaction.
  UITapGestureRecognizer* tapGesture = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleLensInteraction:)];
  UIPanGestureRecognizer* panGesture = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleLensInteraction:)];
  panGesture.cancelsTouchesInView = NO;
  tapGesture.delegate = self;
  panGesture.delegate = self;
  [self.view addGestureRecognizer:tapGesture];
  [self.view addGestureRecognizer:panGesture];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

#pragma mark - Private

// Informs the delegate that the user has interacted with the Lens view.
- (void)handleLensInteraction:(UIGestureRecognizer*)gestureRecognizer {
  if ([gestureRecognizer isKindOfClass:[UITapGestureRecognizer class]]) {
    // A tap event starts and ends simultaneously.
    [self.delegate
        lensOverlayPromoContainerViewControllerDidBeginInteraction:self];
    [self.delegate
        lensOverlayPromoContainerViewControllerDidEndInteraction:self];
    return;
  }

  if ([gestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]]) {
    switch (gestureRecognizer.state) {
      case UIGestureRecognizerStateBegan:
        [self.delegate
            lensOverlayPromoContainerViewControllerDidBeginInteraction:self];
        break;
      case UIGestureRecognizerStateEnded:
      case UIGestureRecognizerStateCancelled:
      case UIGestureRecognizerStateFailed:
        [self.delegate
            lensOverlayPromoContainerViewControllerDidEndInteraction:self];
        break;
      case UIGestureRecognizerStatePossible:
      case UIGestureRecognizerStateChanged:
        break;
    }
  }
}

@end
