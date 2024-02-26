// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_presentation_controller.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_group_view_controller.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/tab_groups_constants.h"

@implementation TabGroupPresentationController {
  TabGroupViewController* _viewController;
}

- (instancetype)initWithPresentedTabGroupViewController:
                    (TabGroupViewController*)tabGroupViewController
                               presentingViewController:
                                   (UIViewController*)presentingViewController {
  self = [super initWithPresentedViewController:tabGroupViewController
                       presentingViewController:presentingViewController];
  if (self) {
    _viewController = tabGroupViewController;
  }
  return self;
}

- (void)presentationTransitionWillBegin {
  [_viewController prepareForPresentation];

  __weak TabGroupViewController* viewController = _viewController;

  CGFloat nonGridElementDuration =
      kTabGroupPresentationDuration * kTabGroupBackgroundElementDurationFactor;
  CGFloat topElementDelay =
      kTabGroupPresentationDuration - nonGridElementDuration;

  [UIView animateWithDuration:nonGridElementDuration
                        delay:topElementDelay
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
                     [viewController animateTopElementsPresentation];
                   }
                   completion:nil];

  [UIView animateWithDuration:nonGridElementDuration
                        delay:0
                      options:UIViewAnimationCurveEaseIn
                   animations:^{
                     [viewController fadeBlurIn];
                   }
                   completion:nil];

  [UIView animateWithDuration:kTabGroupPresentationDuration
                        delay:0
                      options:UIViewAnimationCurveEaseInOut
                   animations:^{
                     [viewController animateGridPresentation];
                   }
                   completion:nil];
}

- (void)dismissalTransitionWillBegin {
  __weak TabGroupViewController* viewController = _viewController;

  [UIView animateWithDuration:kTabGroupDismissalDuration
                        delay:0
                      options:UIViewAnimationCurveEaseInOut
                   animations:^{
                     [viewController animateDismissal];
                   }
                   completion:nil];

  CGFloat backgroundDuration =
      kTabGroupDismissalDuration * kTabGroupBackgroundElementDurationFactor;
  [UIView animateWithDuration:backgroundDuration
                        delay:0
                      options:UIViewAnimationCurveEaseOut
                   animations:^{
                     [viewController fadeBlurOut];
                   }
                   completion:nil];
}

@end
