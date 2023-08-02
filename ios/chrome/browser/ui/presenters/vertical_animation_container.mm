// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/presenters/vertical_animation_container.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/presenters/contained_presenter_delegate.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
NSTimeInterval kAnimationDuration = 0.2;
}

@interface VerticalAnimationContainer ()
@property(nonatomic) NSArray<NSLayoutConstraint*>* dismissedConstraints;
@property(nonatomic) NSArray<NSLayoutConstraint*>* presentedConstraints;
@end

@implementation VerticalAnimationContainer

// Synthesize ContainedPresenter properties.
@synthesize baseViewController = _baseViewController;
@synthesize presentedViewController = _presentedViewController;
@synthesize delegate = _delegate;
// Synthesize private properties.
@synthesize dismissedConstraints = _dismissedConstraints;
@synthesize presentedConstraints = _presentedConstraints;

- (void)prepareForPresentation {
  DCHECK(self.presentedViewController);

  [self.baseViewController addChildViewController:self.presentedViewController];
  [self.baseViewController.view addSubview:self.presentedViewController.view];

  // Shorter names for more readable constraint code.
  UIView* container = self.baseViewController.view;
  UIView* contents = self.presentedViewController.view;

  // The contents view will be sized and positioned by constraints.
  contents.translatesAutoresizingMaskIntoConstraints = NO;

  // The horizontal position of the contents in the container also doesn't
  // change.
  [contents.centerXAnchor constraintEqualToAnchor:container.centerXAnchor]
      .active = YES;

  // When dismissed, the top of the contents is just below the bottom of the
  // container.
  self.dismissedConstraints = @[
    [contents.topAnchor constraintEqualToAnchor:container.bottomAnchor],
  ];

  // When presented, the bottom of the contents matches the bottom of the
  // container.
  self.presentedConstraints = @[
    [contents.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
  ];

  // The contents start off dismissed.
  [NSLayoutConstraint activateConstraints:self.dismissedConstraints];

  // Ensure the contents are actually positioned in the dismissed positions.
  [self.baseViewController.view layoutIfNeeded];

  [self.presentedViewController
      didMoveToParentViewController:self.baseViewController];
}

- (void)presentAnimated:(BOOL)animated {
  // An error if -prepareForPresentation hasn't been called yet.
  // Simple coherence test: the presented view controller's view has a
  // superview.
  DCHECK(self.presentedViewController.view.superview);

  // No-op if already presented.
  if (self.presentedConstraints[0].active)
    return;

  auto animations = ^{
    [NSLayoutConstraint deactivateConstraints:self.dismissedConstraints];
    [NSLayoutConstraint activateConstraints:self.presentedConstraints];
    [self.baseViewController.view layoutIfNeeded];
  };
  auto completion = ^(BOOL finished) {
    if ([self.delegate
            respondsToSelector:@selector(containedPresenterDidPresent:)]) {
      [self.delegate containedPresenterDidPresent:self];
    }
  };

  if (animated) {
    [UIView animateWithDuration:kAnimationDuration
                     animations:animations
                     completion:completion];
  } else {
    animations();
    completion(YES);
  }
}

- (void)dismissAnimated:(BOOL)animated {
  DCHECK(self.presentedViewController);
  // If animated, the base view controller must still be in the view hierarchy.
  DCHECK(!animated || self.baseViewController.view.superview);

  // No-op if already dismissed.
  if (self.dismissedConstraints[0].active)
    return;

  auto animations = ^{
    [NSLayoutConstraint deactivateConstraints:self.presentedConstraints];
    [NSLayoutConstraint activateConstraints:self.dismissedConstraints];
    [self.baseViewController.view layoutIfNeeded];
  };
  auto completion = ^(BOOL finished) {
    [self cleanUpAfterDismissal];
    if ([self.delegate
            respondsToSelector:@selector(containedPresenterDidDismiss:)]) {
      [self.delegate containedPresenterDidDismiss:self];
    }
  };

  if (animated) {
    // Trigger a layout pass before the animation, so that any pending updates
    // aren't animated along with the dismissal.
    [self.baseViewController.view layoutIfNeeded];

    [UIView animateWithDuration:kAnimationDuration
                     animations:animations
                     completion:completion];
  } else {
    // Just execute the completion block synchronously if the dismissal isn't
    // animated. `animations` isn't called because (a) -cleanupAfterDismissal
    // removes the presented view controller from the view hierarchy, and (b)
    // in some contexts a non-animated dismissal may occur when the base view
    // controller is no longer on screen, and the constraint activation in
    // `animations` will crash.
    completion(YES);
  }
}

#pragma mark - private

- (void)cleanUpAfterDismissal {
  if (self.presentedViewController.parentViewController !=
      self.baseViewController) {
    return;
  }
  [self.presentedViewController willMoveToParentViewController:nil];
  [self.presentedViewController.view removeFromSuperview];
  [self.presentedViewController removeFromParentViewController];
}

@end
