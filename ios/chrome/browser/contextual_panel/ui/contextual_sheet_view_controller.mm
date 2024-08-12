// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/ui/contextual_sheet_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/contextual_panel/ui/trait_collection_change_delegate.h"
#import "ios/chrome/browser/contextual_panel/utils/contextual_panel_metrics.h"
#import "ios/chrome/browser/shared/public/commands/contextual_sheet_commands.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Height for the default resting place of a medium detent sheet.
const int kDefaultMediumDetentHeight = 450;

// Top margin for the resting place of a large detent sheet.
const int kLargeDetentTopMargin = 50;

// Threshold for where ending a swipe gesture opens the sheet to the large
// detent.
const int kLargeDetentTopThreshold = 150;

// Duration for the animation of the sheet's height.
const CGFloat kHeightAnimationDuration = 0.3;

// Radius of the 2 top corners on the sheet.
const CGFloat kTopCornerRadius = 10;

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

  // The height of the sheet's content.
  CGFloat _contentHeight;

  // Constraints for the width of the sheet. The second constraint constraints
  // the sheet to half its parent's width and is used in compact height.
  NSLayoutConstraint* _widthConstraint;
  NSLayoutConstraint* _halfWidthConstraint;
}

- (void)viewDidLoad {
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  _panGestureRecognizer = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePanGesture:)];
  [self.view addGestureRecognizer:_panGestureRecognizer];

  self.view.layer.cornerRadius = kTopCornerRadius;
  self.view.layer.maskedCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
  self.view.clipsToBounds = YES;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  if (!parent) {
    _heightConstraint = nil;
    return;
  }

  // Position the view inside its parent.
  [NSLayoutConstraint activateConstraints:@[
    [self.view.bottomAnchor
        constraintEqualToAnchor:self.view.superview.bottomAnchor],
    [self.view.centerXAnchor
        constraintEqualToAnchor:self.view.superview.centerXAnchor],
  ]];

  _widthConstraint = [self.view.widthAnchor
      constraintEqualToAnchor:self.view.superview.widthAnchor];
  _widthConstraint.active =
      self.traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact;

  _halfWidthConstraint = [self.view.widthAnchor
      constraintEqualToAnchor:self.view.superview.widthAnchor
                   multiplier:0.5];
  _halfWidthConstraint.active =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;

  CGFloat initialHeight =
      (self.traitCollection.verticalSizeClass ==
       UIUserInterfaceSizeClassCompact)
          ? self.view.superview.frame.size.height - kLargeDetentTopMargin
          : [self mediumDetentHeight];
  _heightConstraint =
      [self.view.heightAnchor constraintEqualToConstant:initialHeight];
  _heightConstraint.active = YES;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  [self.traitCollectionDelegate traitCollectionDidChangeForViewController:self];

  // It's possible that changing the trait collection removes this view from
  // the view hierarchy, so only do the remaining updates if it's still here.
  if (!self.view.superview) {
    return;
  }

  [self animateHeightConstraintToConstant:[self restingHeight]];

  _widthConstraint.active =
      self.traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact;
  _halfWidthConstraint.active =
      self.traitCollection.verticalSizeClass == UIUserInterfaceSizeClassCompact;
}

- (BOOL)accessibilityPerformEscape {
  [self closeSheet];
  return YES;
}

// Returns the calculated detent of the medium height sheet. If the content
// height is less than the default medium detent, use that instead of the
// default.
- (CGFloat)mediumDetentHeight {
  if (_contentHeight <= 0) {
    return kDefaultMediumDetentHeight;
  }
  return MIN(_contentHeight, kDefaultMediumDetentHeight);
}

// If the sheet is short because the medium detent's size is lower than default,
// then don't allow expansion to large detent.
- (BOOL)shouldAllowLargeDetent {
  return [self mediumDetentHeight] == kDefaultMediumDetentHeight;
}

// Returns the height the sheet should rest at if released at the current
// position. Returns 0 to indicate the sheet should be closed.
- (CGFloat)restingHeight {
  CGFloat superviewHeight = self.view.superview.frame.size.height;

  // Compact height devices only use the large detent.
  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    CGFloat height = superviewHeight - kLargeDetentTopMargin;
    CGFloat closeThreshold = height / 2;
    if (_heightConstraint.constant < closeThreshold) {
      return 0;
    } else {
      return height;
    }
  }

  // TODO(crbug.com/349856760): Use half the medium detent as the threshold for
  // now.
  CGFloat closeThreshold = [self mediumDetentHeight] / 2;

  if ([self shouldAllowLargeDetent] &&
      superviewHeight - _heightConstraint.constant < kLargeDetentTopThreshold) {
    return superviewHeight - kLargeDetentTopMargin;
  } else if (_heightConstraint.constant < closeThreshold) {
    return 0;
  } else {
    return [self mediumDetentHeight];
  }
}

- (void)handlePanGesture:(UIPanGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateBegan) {
    _initialHeightConstraintConstant = _heightConstraint.constant;
  }

  CGFloat translation = [sender translationInView:self.view].y;

  _heightConstraint.constant = _initialHeightConstraintConstant - translation;

  if (sender.state == UIGestureRecognizerStateEnded) {
    CGFloat newHeight = [self restingHeight];
    if (newHeight == 0) {
      [self closeSheet];
    } else {
      [self animateHeightConstraintToConstant:newHeight];
    }
  }
}

- (void)animateAppearance {
  CGFloat initialHeight = _heightConstraint.constant;

  _heightConstraint.constant = 0;
  // Make sure the view is laid out offscreen to prepare for the animation in.
  [self.view.superview layoutIfNeeded];

  [self animateHeightConstraintToConstant:initialHeight];
}

- (void)animateHeightConstraintToConstant:(CGFloat)constant {
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kHeightAnimationDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseOut
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

- (void)closeSheet {
  base::UmaHistogramEnumeration("IOS.ContextualPanel.DismissedReason",
                                ContextualPanelDismissedReason::UserDismissed);
  [self.contextualSheetHandler closeContextualSheet];
}

#pragma mark - ContextualSheetDisplayController

- (void)setContentHeight:(CGFloat)height {
  _contentHeight = height;

  CGFloat newHeight = [self restingHeight];
  // This should not close the sheet if the current height is short and the new
  // contentHeight is tall.
  if (newHeight > 0) {
    _heightConstraint.constant = newHeight;
  }
}

@end
