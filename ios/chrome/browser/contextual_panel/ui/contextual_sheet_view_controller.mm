// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/ui/contextual_sheet_view_controller.h"

#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Top margin for the resting place of a medium detent sheet.
const int kMediumDetentTopMargin = 300;

// Top margin for the resting place of a large detent sheet.
const int kLargeDetentTopMargin = 50;

// Threshold for where ending a swipe gesture opens the sheet to the large
// detent.
const int kLargeDetentTopThreshold = 150;

// Threshold for where ending a swipe gesture closes the sheet.
const int kCloseBottomThreshold = 250;

}  // namespace

@implementation ContextualSheetViewController {
  // Gesture recognizer used to expand and dismiss the sheet.
  UIPanGestureRecognizer* _panGestureRecognizer;

  // Constraint between the top of the sheet and the superview that changes
  // as the sheet expands.
  NSLayoutConstraint* _topConstraint;

  // Stores the initial value of the topConstraint when the pan gesture starts
  // for use in calculation.
  CGFloat _initialTopConstraintConstant;
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
    _topConstraint = nil;
    return;
  }

  // Position the view inside its parent.
  AddSameConstraintsToSides(
      self.view.superview, self.view,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);

  _topConstraint = [self.view.topAnchor
      constraintEqualToAnchor:self.view.superview.topAnchor];
  _topConstraint.constant = kMediumDetentTopMargin;
  _topConstraint.active = YES;
}

- (void)handlePanGesture:(UIPanGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateBegan) {
    _initialTopConstraintConstant = _topConstraint.constant;
  }

  CGFloat translation = [sender translationInView:self.view].y;

  _topConstraint.constant = _initialTopConstraintConstant + translation;

  if (sender.state == UIGestureRecognizerStateEnded) {
    if (_topConstraint.constant < kLargeDetentTopThreshold) {
      _topConstraint.constant = kLargeDetentTopMargin;
    } else if (_topConstraint.constant >
               self.view.superview.frame.size.height - kCloseBottomThreshold) {
      [self.contextualSheetHandler hideContextualSheet];
    } else {
      _topConstraint.constant = kMediumDetentTopMargin;
    }
  }
}

@end
