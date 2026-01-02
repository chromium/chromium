// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/coordinator/assistant_sheet_coordinator.h"

#import "ios/chrome/browser/assistant/aim/coordinator/assistant_aim_coordinator.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_commands.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_sheet_child_coordinator.h"
#import "ios/chrome/browser/assistant/gemini/coordinator/assistant_gemini_coordinator.h"
#import "ios/chrome/browser/assistant/ui/assistant_bar_configuration.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_animator.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller.h"
#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller_delegate.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

@interface AssistantSheetCoordinator () <AssistantCommands,
                                         AssistantSheetViewControllerDelegate>
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

  // Resolve layout guide.
  GuideName* guideName = kAppBarGuide;
  LayoutGuideCenter* center = LayoutGuideCenterForBrowser(nil);
  _viewController.anchorView = [center referencedViewUnderName:guideName];

  // Initialize child coordinator based on mode.
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

  _childCoordinator.handler = self;
  [_childCoordinator start];

  [_viewController setChildViewController:_childCoordinator.viewController];
  // Configure the assistant bar using the child's configuration.
  [_viewController setBarConfiguration:_childCoordinator.barConfiguration];

  // Add the view controller as a child view controller.
  [self.baseViewController addChildViewController:_viewController];
  // Add the view to the hierarchy.
  [self.baseViewController.view addSubview:_viewController.view];
  [_viewController didMoveToParentViewController:self.baseViewController];

  // Force layout to determine optimal size before animating.
  [self.baseViewController.view layoutIfNeeded];

  // Animation: Expand and fade in.
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

#pragma mark - AssistantCommands

- (void)updateBarConfiguration:(AssistantBarConfiguration*)configuration {
  [_viewController setBarConfiguration:configuration];
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
