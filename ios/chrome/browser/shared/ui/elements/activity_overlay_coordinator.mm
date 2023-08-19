// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/activity_overlay_coordinator.h"

#import "ios/chrome/browser/shared/ui/elements/activity_overlay_view_controller.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@interface ActivityOverlayCoordinator ()
// View controller that displays an activity indicator.
@property(nonatomic, strong) UIViewController* activityOverlayViewController;
@end

@implementation ActivityOverlayCoordinator

@synthesize activityOverlayViewController = _activityOverlayViewController;

- (void)start {
  if (self.activityOverlayViewController) {
    return;
  }
  self.activityOverlayViewController =
      [[ActivityOverlayViewController alloc] initWithNibName:nil bundle:nil];
  [self.baseViewController
      addChildViewController:self.activityOverlayViewController];
  [self.baseViewController.view
      addSubview:self.activityOverlayViewController.view];
  [self.activityOverlayViewController
      didMoveToParentViewController:self.baseViewController];
  UIView* baseView = self.baseViewController.view;
  UIView* activityOverlayView = self.activityOverlayViewController.view;
  AddSameCenterConstraints(baseView, activityOverlayView);
  [NSLayoutConstraint activateConstraints:@[
    [baseView.heightAnchor
        constraintEqualToAnchor:activityOverlayView.heightAnchor],
    [baseView.widthAnchor
        constraintEqualToAnchor:activityOverlayView.widthAnchor],
  ]];
}

- (void)stop {
  if (!self.activityOverlayViewController) {
    return;
  }
  [self.activityOverlayViewController willMoveToParentViewController:nil];
  [self.activityOverlayViewController.view removeFromSuperview];
  [self.activityOverlayViewController removeFromParentViewController];
  self.activityOverlayViewController = nil;
}

@end
