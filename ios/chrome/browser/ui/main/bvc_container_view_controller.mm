// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/bvc_container_view_controller.h"

#import <ostream>

#import "base/check_op.h"
#import "ios/chrome/browser/tabs/ui_bundled/tab_strip_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

@interface BVCContainerViewController ()

// Background behind toolbar and webview during animation to avoid seeing
// partial color streaks from background with different color between the
// different moving parts.
@property(nonatomic, strong) UIView* solidBackground;

@end

@implementation BVCContainerViewController

#pragma mark - Public

- (UIViewController*)currentBVC {
  return [self.childViewControllers firstObject];
}

- (void)setCurrentBVC:(UIViewController*)bvc {
  DCHECK(bvc);
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

  // Add the new active view controller.
  [self addChildViewController:bvc];
  // If the BVC's view has a transform, then its frame isn't accurate.
  // Instead, remove the transform, set the frame, then reapply the transform.
  CGAffineTransform oldTransform = bvc.view.transform;
  bvc.view.transform = CGAffineTransformIdentity;
  bvc.view.frame = self.view.bounds;
  bvc.view.transform = oldTransform;
  [self.view addSubview:bvc.view];
  [bvc didMoveToParentViewController:self];

  DCHECK(self.currentBVC == bvc);
}

#pragma mark - UIViewController methods

- (void)viewDidLoad {
  [super viewDidLoad];
}

- (void)presentViewController:(UIViewController*)viewControllerToPresent
                     animated:(BOOL)flag
                   completion:(void (^)())completion {
  // Force presentation to go through the current BVC, if possible, which does
  // some associated bookkeeping.
  UIViewController* viewController =
      self.currentBVC ? self.currentBVC : self.fallbackPresenterViewController;
  [viewController presentViewController:viewControllerToPresent
                               animated:flag
                             completion:completion];
}

- (void)dismissViewControllerAnimated:(BOOL)flag
                           completion:(void (^)())completion {
  // Force dismissal to go through the current BVC, if possible, which does some
  // associated bookkeeping.
  UIViewController* viewController =
      self.currentBVC ? self.currentBVC : self.fallbackPresenterViewController;
  [viewController dismissViewControllerAnimated:flag completion:completion];
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

@end
