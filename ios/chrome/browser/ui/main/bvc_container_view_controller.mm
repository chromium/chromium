// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"

#include <ostream>

#include "base/check_op.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_vertical_pan_handler.h"
#import "ios/chrome/browser/ui/thumb_strip/thumb_strip_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BVCContainerViewController

@synthesize thumbStripPanHandler = _thumbStripPanHandler;

#pragma mark - public property implementation

- (UIViewController*)currentBVC {
  return [self.childViewControllers firstObject];
}

- (void)setCurrentBVC:(UIViewController*)bvc {
  // When the thumb strip is enabled, the BVC container stays around all the
  // time. When on a tab grid page with no tabs or the recent tab page, the
  // currentBVC will be set to nil.
  DCHECK(bvc || ShowThumbStripInTraitCollection(self.traitCollection));
  if (self.currentBVC == bvc) {
    return;
  }

  // Remove the current bvc, if any.
  if (self.currentBVC) {
    [self.currentBVC willMoveToParentViewController:nil];
    [self.currentBVC.view removeFromSuperview];
    [self.currentBVC removeFromParentViewController];
  }

  DCHECK_EQ(nil, self.currentBVC);
  DCHECK_EQ(0U, self.view.subviews.count);

  // Add the new active view controller.
  if (bvc) {
    [self addChildViewController:bvc];
    // If the BVC's view has a transform, then its frame isn't accurate.
    // Instead, remove the transform, set the frame, then reapply the transform.
    CGAffineTransform oldTransform = bvc.view.transform;
    bvc.view.transform = CGAffineTransformIdentity;
    bvc.view.frame = self.view.bounds;
    bvc.view.transform = oldTransform;
    [self.view addSubview:bvc.view];
    [bvc didMoveToParentViewController:self];

    if (ShowThumbStripInTraitCollection(self.traitCollection)) {
      // The background needs to be clear to allow the thumb strip to be seen
      // during the enter/exit thumb strip animation.
      self.currentBVC.view.backgroundColor = [UIColor clearColor];
    }
  }

  DCHECK(self.currentBVC == bvc);
}

#pragma mark - UIViewController methods

- (void)presentViewController:(UIViewController*)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^)())completion {
  // Force presentation to go through the current BVC, which does some
  // associated bookkeeping.
  DCHECK(self.currentBVC);
  [self.currentBVC presentViewController:viewControllerToPresent
                                animated:flag
                              completion:completion];
}

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)())completion {
  // Force dismissal to go through the current BVC, which does some associated
  // bookkeeping.
  DCHECK(self.currentBVC);
  [self.currentBVC dismissViewControllerAnimated:flag completion:completion];
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.currentBVC;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.currentBVC;
}

- (BOOL)shouldAutorotate {
  return self.currentBVC ? [self.currentBVC shouldAutorotate]
                         : [super shouldAutorotate];
}

#pragma mark - ViewRevealingAnimatee

- (void)willAnimateViewRevealFromState:(ViewRevealState)currentViewRevealState
                               toState:(ViewRevealState)nextViewRevealState {
  // No-op.
}

- (void)animateViewReveal:(ViewRevealState)nextViewRevealState {
  switch (nextViewRevealState) {
    case ViewRevealState::Hidden:
      self.view.transform = CGAffineTransformIdentity;
      break;
    case ViewRevealState::Peeked:
      self.view.transform = CGAffineTransformMakeTranslation(
          0, self.thumbStripPanHandler.peekedHeight);
      break;
    case ViewRevealState::Revealed:
      self.view.transform = CGAffineTransformMakeTranslation(
          0, self.thumbStripPanHandler.revealedHeight);
      break;
  }
}

- (void)didAnimateViewReveal:(ViewRevealState)viewRevealState {
  // No-op.
}

@end
