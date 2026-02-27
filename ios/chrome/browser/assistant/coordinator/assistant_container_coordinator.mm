// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/coordinator/assistant_container_coordinator.h"

#import "base/notreached.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_animator.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

@implementation AssistantContainerCoordinator {
  // The view controller for the assistant container.
  AssistantContainerViewController* _containerViewController;
  // The content view controller to be displayed inside the container.
  UIViewController* _contentViewController;
  AssistantContainerAnimator* _animator;
  __weak id<AssistantContainerDelegate> _delegate;
  // Whether a dismissal is currently in progress.
  BOOL _dismissalInProgress;
  // Completion block to be executed after dismissal.
  ProceduralBlock _dismissalCompletion;
  // The available detents for the container.
  NSArray<AssistantContainerDetent*>* _detents;
}

- (void)start {
  [self.browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(AssistantContainerCommands)];
}

- (void)stop {
  NOTREACHED() << "Use stopAnimated:completion: instead.";
}

- (void)stopAnimated:(BOOL)animated completion:(ProceduralBlock)completion {
  [self.browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  [self dismissAssistantContainerAnimated:animated completion:completion];
}

#pragma mark - AssistantContainerCommands

- (void)showAssistantContainerWithContent:(UIViewController*)viewController
                                 delegate:
                                     (id<AssistantContainerDelegate>)delegate {
  if (_containerViewController) {
    // Already presented.
    return;
  }

  _contentViewController = viewController;
  _delegate = delegate;

  _containerViewController = [[AssistantContainerViewController alloc]
      initWithViewController:_contentViewController];
  _containerViewController.delegate = _delegate;
  if (_detents) {
    _containerViewController.detents = _detents;
  }

  // Resolve layout guide.
  GuideName* guideName = kAppBarGuide;
  LayoutGuideCenter* center = LayoutGuideCenterForBrowser(nil);
  _containerViewController.anchorView = [center referencedViewUnderName:guideName];

  // Add the view controller as a child view controller.
  [self.baseViewController addChildViewController:_containerViewController];

  // Add the view to the hierarchy.
  [self.baseViewController.view addSubview:_containerViewController.view];
  [_containerViewController didMoveToParentViewController:self.baseViewController];

  // Force layout to determine optimal size before animating.
  [self.baseViewController.view layoutIfNeeded];

  if ([_delegate respondsToSelector:@selector(assistantContainer:
                                              willAppearAnimated:)]) {
    [_delegate assistantContainer:_containerViewController willAppearAnimated:YES];
  }

  _animator = [[AssistantContainerAnimator alloc] init];

  __weak __typeof(self) weakSelf = self;
  [_animator animatePresentation:_containerViewController
                      completion:^{
                        [weakSelf didCompletePresentationAnimation];
                      }];
}

- (void)setAssistantContainerDetents:
    (NSArray<AssistantContainerDetent*>*)detents {
  _detents = detents;
  [_containerViewController setDetents:detents];
}

- (void)dismissAssistantContainerAnimated:(BOOL)animated
                               completion:(ProceduralBlock)completion {
  if (!_containerViewController) {
    if (completion) {
      completion();
    }
    return;
  }

  // If a dismissal is already in progress, update the completion block.
  // If the new request is non-animated, force immediate dismissal.
  if (_dismissalInProgress) {
    if (completion) {
      _dismissalCompletion = completion;
    }
    if (!animated) {
      [_containerViewController.view.layer removeAllAnimations];
      [_containerViewController.view removeFromSuperview];
      [self didCompleteDismissalAnimationAnimated:NO];
    }
    return;
  }

  _dismissalInProgress = YES;
  if (completion) {
    _dismissalCompletion = completion;
  }

  if ([_delegate respondsToSelector:@selector(assistantContainer:
                                           willDisappearAnimated:)]) {
    [_delegate assistantContainer:_containerViewController
            willDisappearAnimated:animated];
  }

  if (animated) {
    __weak __typeof(self) weakSelf = self;
    [_animator animateDismissal:_containerViewController
                     completion:^{
                       [weakSelf didCompleteDismissalAnimationAnimated:YES];
                     }];
  } else {
    [_containerViewController.view removeFromSuperview];
    [self didCompleteDismissalAnimationAnimated:NO];
  }
}

#pragma mark - Private

// Called when the presentation animation completes.
- (void)didCompletePresentationAnimation {
  if ([_delegate respondsToSelector:@selector(assistantContainer:
                                               didAppearAnimated:)]) {
    [_delegate assistantContainer:_containerViewController didAppearAnimated:YES];
  }
}

// Called when the dismissal animation completes.
- (void)didCompleteDismissalAnimationAnimated:(BOOL)animated {
  // If the dismissal is not in progress, it means it has already been completed
  // (e.g. by a subsequent non-animated dismissal).
  if (!_dismissalInProgress) {
    return;
  }

  if ([_delegate respondsToSelector:@selector(assistantContainer:
                                            didDisappearAnimated:)]) {
    [_delegate assistantContainer:_containerViewController
             didDisappearAnimated:animated];
  }

  _dismissalInProgress = NO;

  // Cleanup view controller and state.
  [_containerViewController willMoveToParentViewController:nil];
  [_containerViewController.view removeFromSuperview];
  [_containerViewController removeFromParentViewController];
  [_containerViewController didMoveToParentViewController:nil];
  _containerViewController = nil;

  _animator = nil;
  _contentViewController = nil;
  _delegate = nil;
  _detents = nil;

  if (_dismissalCompletion) {
    ProceduralBlock completion = _dismissalCompletion;
    _dismissalCompletion = nil;
    completion();
  }
}

@end
