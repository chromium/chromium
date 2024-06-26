// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/ui/contextual_sheet_view_controller.h"

#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Height for the resting place of a medium detent sheet.
const int kMediumDetentHeight = 450;

// Top margin for the resting place of a large detent sheet.
const int kLargeDetentTopMargin = 50;

// Threshold for where ending a swipe gesture opens the sheet to the large
// detent.
const int kLargeDetentTopThreshold = 150;

// Threshold for where ending a swipe gesture closes the sheet.
const int kCloseBottomThreshold = 250;

// Duration for the animation of the sheet's height.
const CGFloat kHeightAnimationDuration = 0.2;

}  // namespace

@implementation ContextualSheetViewController {
  // Gesture recognizer used to expand and dismiss the sheet.
  UIPanGestureRecognizer* _panGestureRecognizer;

  // Constraint for the height of the sheet that changes
  // as the sheet expands.
  NSLayoutConstraint* _heightConstraint;

  // Stores the initial value of the heightConstraint when the pan gesture
  // starts for use in calculation.
  CGFloat _initialHeightConstraintConstant;
}

- (void)viewDidLoad {
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  _panGestureRecognizer = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePanGesture:)];
  [self.view addGestureRecognizer:_panGestureRecognizer];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  if (!parent) {
    _heightConstraint = nil;
    return;
  }

  // Position the view inside its parent.
  AddSameConstraintsToSides(
      self.view.superview, self.view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);

  _heightConstraint =
      [self.view.heightAnchor constraintEqualToConstant:kMediumDetentHeight];
  _heightConstraint.active = YES;
}

- (void)handlePanGesture:(UIPanGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateBegan) {
    _initialHeightConstraintConstant = _heightConstraint.constant;
  }

  CGFloat translation = [sender translationInView:self.view].y;

  _heightConstraint.constant = _initialHeightConstraintConstant - translation;

  CGFloat superviewHeight = self.view.superview.frame.size.height;

  if (sender.state == UIGestureRecognizerStateEnded) {
    if (superviewHeight - _heightConstraint.constant <
        kLargeDetentTopThreshold) {
      [self animateHeightConstraintToConstant:superviewHeight -
                                              kLargeDetentTopMargin];
    } else if (_heightConstraint.constant < kCloseBottomThreshold) {
      [self.contextualSheetHandler closeContextualSheet];
    } else {
      [self animateHeightConstraintToConstant:kMediumDetentHeight];
    }
  }
}

- (void)animateAppearance {
  _heightConstraint.constant = 0;
  // Make sure the view is laid out offscreen to prepare for the animation in.
  [self.view.superview layoutIfNeeded];

  [self animateHeightConstraintToConstant:kMediumDetentHeight];
}

- (void)animateHeightConstraintToConstant:(CGFloat)constant {
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kHeightAnimationDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseInOut
                   animations:^{
                     [weakSelf
                         blockForAnimatingHeightConstraintToConstant:constant];
                   }
                   completion:nil];
}

- (void)blockForAnimatingHeightConstraintToConstant:(CGFloat)constant {
  _heightConstraint.constant = constant;
  [self.view.superview layoutIfNeeded];
}

@end
