// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/coordinator/assistant_sheet_coordinator.h"

#import "ios/chrome/browser/assistant/aim/coordinator/assistant_aim_coordinator.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_sheet_child_coordinator.h"
#import "ios/chrome/browser/assistant/gemini/coordinator/assistant_gemini_coordinator.h"
#import "ios/chrome/browser/assistant/ui/assistant_navbar_configuration.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_animator.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller_delegate.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

@interface AssistantSheetCoordinator () <AssistantSheetViewControllerDelegate>
@end

@implementation AssistantSheetCoordinator {
  AssistantSheetViewController* _viewController;
  AssistantSheetChildCoordinator* _childCoordinator;
  AssistantSheetAnimator* _animator;
}

- (void)start {
  if (_viewController) {
    return;
  }

  _viewController = [[AssistantSheetViewController alloc] init];
  _viewController.delegate = self;

  // Resolve Layout Guide.
  GuideName* guideName = kAppBarGuide;
  LayoutGuideCenter* center = LayoutGuideCenterForBrowser(nil);
  _viewController.anchorView = [center referencedViewUnderName:guideName];

  // Initialize Child Coordinator based on Mode.
  switch (self.mode) {
    case AssistantSheetModeAI:
      _childCoordinator = [[AssistantAIMCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser];
      break;
    case AssistantSheetModeGemini:
      _childCoordinator = [[AssistantGeminiCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser];
      break;
  }

  [_childCoordinator start];
  [_viewController setChildViewController:_childCoordinator.viewController];
  // Configure Navigation Bar using the child's configuration.
  [_viewController
      setNavigationBarConfiguration:_childCoordinator.navbarConfiguration];

  // Add the view controller as a child view controller.
  [self.baseViewController addChildViewController:_viewController];
  // Add the view to the hierarchy.
  [self.baseViewController.view addSubview:_viewController.view];
  [_viewController didMoveToParentViewController:self.baseViewController];

  // Force layout to determine optimal size before animating.
  [self.baseViewController.view layoutIfNeeded];

  // Animation: Expand and Fade In.
  _animator = [[AssistantSheetAnimator alloc] init];
  [_animator animatePresentation:_viewController completion:nil];
}

- (void)stop {
  [_childCoordinator stop];
  _childCoordinator = nil;

  // Remove the view controller from the hierarchy. This is required to revert
  // the steps taken in start.
  [_viewController willMoveToParentViewController:nil];
  [_viewController.view removeFromSuperview];
  [_viewController removeFromParentViewController];

  _viewController = nil;
  _animator = nil;
}

#pragma mark - AssistantSheetViewControllerDelegate

- (void)assistantSheetViewControllerDidTapClose:
    (AssistantSheetViewController*)viewController {
  __weak __typeof(self) weakSelf = self;
  [_animator animateDismissal:_viewController
                   completion:^{
                     [weakSelf dismissalAnimationCompletion];
                   }];
}

#pragma mark - Private

// Called when the dismissal animation completes.
- (void)dismissalAnimationCompletion {
  [self stop];
}

@end
