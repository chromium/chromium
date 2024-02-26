// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_transition_delegate.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_presentation_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_transition_animator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_view_controller.h"

@implementation TabGroupTransitionDelegate {
  TabGroupViewController* _tabGroupViewController;
}

- (instancetype)initWithTabGroupViewController:
    (TabGroupViewController*)tabGroupViewController {
  self = [super init];
  if (self) {
    _tabGroupViewController = tabGroupViewController;
  }
  return self;
}

- (UIPresentationController*)
    presentationControllerForPresentedViewController:
        (UIViewController*)presented
                            presentingViewController:
                                (UIViewController*)presenting
                                sourceViewController:(UIViewController*)source {
  TabGroupPresentationController* presentationController =
      [[TabGroupPresentationController alloc]
          initWithPresentedTabGroupViewController:_tabGroupViewController
                         presentingViewController:presenting];
  return presentationController;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForPresentedController:(UIViewController*)presented
                         presentingController:(UIViewController*)presenting
                             sourceController:(UIViewController*)source {
  TabGroupTransitionAnimator* animator =
      [[TabGroupTransitionAnimator alloc] init];
  animator.appearing = YES;
  return animator;
}

- (id<UIViewControllerAnimatedTransitioning>)
    animationControllerForDismissedController:(UIViewController*)dismissed {
  TabGroupTransitionAnimator* animator =
      [[TabGroupTransitionAnimator alloc] init];
  animator.appearing = NO;
  return animator;
}

@end
