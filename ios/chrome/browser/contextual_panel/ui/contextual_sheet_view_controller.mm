// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/ui/contextual_sheet_view_controller.h"

#import "base/metrics/histogram_functions.h"
#import "ios/chrome/browser/contextual_panel/ui/contextual_panel_view_constants.h"
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

// Duration for the animation of the sheet's height.
const CGFloat kHeightAnimationDuration = 0.3;

// Radius of the 2 top corners on the sheet.
const CGFloat kTopCornerRadius = 10;

}  // namespace

@interface ContextualSheetViewController () <UIGestureRecognizerDelegate>

@end

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

  // Constraints for the width of the sheet. The second constraint constrains
  // the sheet to a portion of its parent's width and is used in compact height.
  NSLayoutConstraint* _widthConstraint;
  NSLayoutConstraint* _compactHeightWidthConstraint;
}

- (void)viewDidLoad {
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  _panGestureRecognizer = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePanGesture:)];
  _panGestureRecognizer.delegate = self;
  [self.view addGestureRecognizer:_panGestureRecognizer];

  self.view.layer.cornerRadius = kTopCornerRadius;
  self.view.layer.maskedCorners =
      kCALayerMinXMinYCorner | kCALayerMaxXMinYCorner;
  self.view.clipsToBounds = YES;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleKeyboardWillShow:)
             name:UIKeyboardWillShowNotification
           object:nil];
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

  _compactHeightWidthConstraint = [self.view.widthAnchor
      constraintEqualToAnchor:self.view.superview.widthAnchor
                   multiplier:0.66];
  _compactHeightWidthConstraint.active =
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

  [self
      animateHeightConstraintToConstant:[self
                                            restingHeightWithSheetVelocity:0]];

  _widthConstraint.active =
      self.traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact;
  _compactHeightWidthConstraint.active =
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
// then don't allow expansion to large detent. Compact height devices only ever
// show the large detent, so it should always be allowed there.
- (BOOL)shouldAllowLargeDetent {
  return self.traitCollection.verticalSizeClass ==
             UIUserInterfaceSizeClassCompact ||
         [self mediumDetentHeight] == kDefaultMediumDetentHeight;
}

// Returns the height the sheet should rest at if released at the current
// position and velocity. Velocity should be positive if the sheet is moving
// up/getting larger. Returns 0 to indicate the sheet should be closed.
- (CGFloat)restingHeightWithSheetVelocity:(CGFloat)velocity {
  CGFloat superviewHeight = self.view.superview.frame.size.height;
  CGFloat currentSheetHeight = _heightConstraint.constant;

  // Estimate the final resting point pretending the sheet will decelerate over
  // 1 second. This is just a simple heuristic for what should feel reasonable
  // to the user. It is not taken from any UIKit deceleration code. Then the
  // resulting formula comes from the physics formula for distance traveled with
  // constant acceleration and known initial velocity and time.
  CGFloat estimatedFinalRestingHeight = currentSheetHeight + 0.5 * velocity;

  NSMutableArray<NSNumber*>* detents =
      [[NSMutableArray alloc] initWithArray:@[ @0 ]];
  // Only regular height devices support medium detent.
  if (self.traitCollection.verticalSizeClass !=
      UIUserInterfaceSizeClassCompact) {
    [detents addObject:[NSNumber numberWithDouble:[self mediumDetentHeight]]];
  }
  if ([self shouldAllowLargeDetent]) {
    [detents addObject:[NSNumber numberWithDouble:superviewHeight -
                                                  kLargeDetentTopMargin]];
  }

  // Find the detents the current height is between.
  if (currentSheetHeight < [detents[0] doubleValue]) {
    return [detents[0] doubleValue];
  }

  for (NSUInteger index = 0; index < detents.count - 1; index++) {
    if (currentSheetHeight < [detents[index + 1] doubleValue]) {
      // If the estimated resting height is less than halfway to the next
      // detent, then the resting height is the current detent. Otherwise, it's
      // the next detent.
      if (estimatedFinalRestingHeight <
          ([detents[index] doubleValue] + [detents[index + 1] doubleValue]) /
              2) {
        return [detents[index] doubleValue];
      } else {
        return [detents[index + 1] doubleValue];
      }
    }
  }

  // Detents is initialized with 0, so there's always at least one option.
  return [detents[detents.count - 1] doubleValue];
}

- (void)handlePanGesture:(UIPanGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateBegan) {
    _initialHeightConstraintConstant = _heightConstraint.constant;
  }

  CGFloat translation = [sender translationInView:self.view].y;
  // According to the gesture recognizer, positive velocity means gesture moving
  // down the screen (towards positive y), but it's easier to think about
  // positive velocity meaning sheet getting taller.
  CGFloat sheetVelocity = -[sender velocityInView:self.view].y;

  _heightConstraint.constant = _initialHeightConstraintConstant - translation;

  if (sender.state == UIGestureRecognizerStateEnded) {
    CGFloat newHeight = [self restingHeightWithSheetVelocity:sheetVelocity];
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

  CGFloat newHeight = [self restingHeightWithSheetVelocity:0];
  // This should not close the sheet if the current height is short and the new
  // contentHeight is tall.
  if (newHeight > 0) {
    _heightConstraint.constant = newHeight;
  }
}

#pragma mark - Keyboard notifications

- (void)handleKeyboardWillShow:(NSNotification*)notification {
  base::UmaHistogramEnumeration("IOS.ContextualPanel.DismissedReason",
                                ContextualPanelDismissedReason::KeyboardOpened);
  [self.contextualSheetHandler closeContextualSheet];
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRequireFailureOfGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Require gestures in the panel's content to fail before expanding the panel
  // itself. SwiftUI only allows setting the name field on their gesture
  // recognizers in iOS 18, so use a workaround for identifying SwiftUI gesture
  // recognizers in earlier iOS versions.

  // SwiftUI only allowed setting a name on a gesture recognizer in iOS 18, so
  // this check is necessary for cases where the app is built on an earlier SDK
  // but is running on iOS 18, but can be removed once the build target is
  // updated.
  BOOL isSwiftUIOniOS18 = [NSStringFromClass([otherGestureRecognizer class])
      isEqualToString:@"SwiftUI.UIKitResponderGestureRecognizer"];
  if ([NSStringFromClass([otherGestureRecognizer class])
          isEqualToString:@"SwiftUI.UIKitGestureRecognizer"] ||
      isSwiftUIOniOS18 ||
      [otherGestureRecognizer.name
          isEqualToString:kPanelContentGestureRecognizerName]) {
    return YES;
  }
  return NO;
}

@end
