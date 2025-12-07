// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/coordinator/reader_mode_blur_overlay_coordinator.h"

#import "ios/chrome/browser/reader_mode/ui/reader_mode_blur_overlay_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation ReaderModeBlurOverlayCoordinator {
  // View controller that displays the blur overlay.
  ReaderModeBlurOverlayViewController* _viewController;
}

- (void)start {
  [self startWithCompletion:nil];
}

- (void)startWithCompletion:(ProceduralBlock)completion {
  _viewController = [[ReaderModeBlurOverlayViewController alloc] init];
  [self.baseViewController addChildViewController:_viewController];
  [self.baseViewController.view addSubview:_viewController.view];
  [_viewController didMoveToParentViewController:self.baseViewController];
  _viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.baseViewController.view, _viewController.view);
  [_viewController animateInWithCompletion:completion];
}

- (void)stop {
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];
  _viewController = nil;
}

@end
