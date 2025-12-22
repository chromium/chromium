// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/coordinator/assistant_sheet_coordinator.h"

#import "ios/chrome/browser/assistant/ui/assistant_sheet_animator.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

@implementation AssistantSheetCoordinator {
  AssistantSheetViewController* _viewController;
  AssistantSheetAnimator* _animator;
}

- (void)start {
  if (_viewController) {
    return;
  }

  _viewController = [[AssistantSheetViewController alloc] init];

  // Resolve Layout Guide.
  GuideName* guideName = kDiamondBottomAppBarGuide;
  LayoutGuideCenter* center = LayoutGuideCenterForBrowser(nil);
  _viewController.anchorView = [center referencedViewUnderName:guideName];

  // Add the view controller as a child view controller.
  [self.baseViewController addChildViewController:_viewController];
  // Add the view to the hierarchy.
  [self.baseViewController.view addSubview:_viewController.view];
  [_viewController didMoveToParentViewController:self.baseViewController];

  // Force layout to determine optimal size before animating.
  [self.baseViewController.view layoutIfNeeded];

  // Animation: Expand and Fade In.
  _animator = [[AssistantSheetAnimator alloc] init];
  [_animator animatePresentation:_viewController.view completion:nil];
}

- (void)stop {
  // Remove the view controller from the hierarchy. This is required to revert
  // the steps taken in start.
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];

  _viewController = nil;
  _animator = nil;
}

@end
