// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"

#import <algorithm>

#import "base/check.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Margin for the container content relative to the screen edges.
constexpr CGFloat kContainerMargin = 5.0;
// Used as the fallback height when no detents are provided.
constexpr CGFloat kMinContainerHeight = 60.0;
// Constants used for the container resizing animation.
constexpr CGFloat kRubberBandCoefficient = 8.0;
constexpr CGFloat kFlingVelocityThreshold = 1000.0;
constexpr CGFloat kSpringDuration = 0.3;
constexpr CGFloat kSpringDamping = 0.85;
constexpr CGFloat kMomentumProjectionSeconds = 0.2;

// The height of the top area that responds to the pan gesture.
constexpr CGFloat kGestureTopAreaHeight = 44.0;

}  // namespace

@interface AssistantContainerViewController () <UIGestureRecognizerDelegate>
@end

@implementation AssistantContainerViewController {
  NSLayoutConstraint* _heightConstraint;
  AssistantContainerView* _assistantContainerView;

  // State storage for configuration before view load.
  UIViewController* _childViewController;

  // Gesture recognizer for resizing the container.
  UIPanGestureRecognizer* _headerPanGesture;
  // The height of the container when the gesture started.
  CGFloat _initialConstraintHeight;
  // Whether the user has manually resized the container.
  BOOL _hasUserResized;
  // Whether the view has appeared.
  BOOL _hasAppeared;
}

- (instancetype)initWithViewController:(UIViewController*)viewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _childViewController = viewController;
  }
  return self;
}

- (void)loadView {
  _assistantContainerView = [[AssistantContainerView alloc] init];
  self.view = _assistantContainerView;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  [self setUpGestures];

  // Apply pending configuration.
  if (_childViewController) {
    [self addChildViewController:_childViewController];
    _childViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    [_assistantContainerView.contentView addSubview:_childViewController.view];
    AddSameConstraints(_childViewController.view,
                       _assistantContainerView.contentView);
    [_childViewController didMoveToParentViewController:self];
  }

  // Create and activate the height constraint.
  CGFloat initialHeight = kMinContainerHeight;
  if (self.detents.count == 1) {
    initialHeight = MAX(self.detents.firstObject.value, kMinContainerHeight);
  } else {
    CGFloat preferredHeight = [_assistantContainerView preferredHeight];
    initialHeight = MAX(preferredHeight, kMinContainerHeight);
  }
  _heightConstraint =
      [self.view.heightAnchor constraintEqualToConstant:initialHeight];
  _heightConstraint.active = YES;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (parent) {
    [self layoutInParentView:parent.view];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateHeightConstraint];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  _hasAppeared = YES;
}

#pragma mark - Properties

- (void)setIsAnimating:(BOOL)isAnimating {
  if (_isAnimating == isAnimating) {
    return;
  }
  _isAnimating = isAnimating;
  [self updatePanGestureEnabledState];
}

- (void)setDetents:(NSArray<AssistantContainerDetent*>*)detents {
  _detents = [detents copy];
  [self updatePanGestureEnabledState];
  [self.view setNeedsLayout];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  if (gestureRecognizer != _headerPanGesture) {
    return YES;
  }
  CGPoint location = [touch locationInView:self.view];
  // Restrict the pan gesture to the top area.
  return location.y <= kGestureTopAreaHeight;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldBeRequiredToFailByGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  if (gestureRecognizer == _headerPanGesture) {
    // Ensures the container's resizing pan gesture has absolute priority over
    // other gestures, like an embedded UIScrollView's content pan.
    return
        [otherGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]];
  }
  return NO;
}

#pragma mark - Private

// Adds gesture recognizers to the view.
- (void)setUpGestures {
  _headerPanGesture = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePanGesture:)];
  _headerPanGesture.delegate = self;
  [self.view addGestureRecognizer:_headerPanGesture];
  [self updatePanGestureEnabledState];
}

// Updates the pan gesture enabled state based on animation and detents.
- (void)updatePanGestureEnabledState {
  // Prevent the gesture recognizer from interfering with the animation.
  if (self.isAnimating) {
    _headerPanGesture.enabled = NO;
    return;
  }

  _headerPanGesture.enabled = YES;
}

// Handles the pan gesture on the header to resize the container.
- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture {
  if (gesture != _headerPanGesture) {
    return;
  }

  if ([self.delegate respondsToSelector:@selector(assistantContainer:
                                            shouldInterceptPanGesture:)]) {
    if ([self.delegate assistantContainer:self
                shouldInterceptPanGesture:gesture]) {
      return;
    }
  }

  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }

  if (gesture.state == UIGestureRecognizerStateBegan) {
    [self handlePanGestureBegan:gesture];
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    [self handlePanGestureChanged:gesture];
  } else if (gesture.state == UIGestureRecognizerStateEnded ||
             gesture.state == UIGestureRecognizerStateCancelled) {
    [self handlePanGestureEnded:gesture];
  }
}

// Handles the state when the pan gesture begins.
- (void)handlePanGestureBegan:(UIPanGestureRecognizer*)gesture {
  CHECK(gesture == _headerPanGesture);
  _initialConstraintHeight = _heightConstraint.constant;
}

// Calculates the effective minimum height based on detents.
- (NSInteger)effectiveMinHeight {
  NSInteger absoluteMax = [self absoluteMaxHeight];

  if (self.detents.count == 0) {
    return round(kMinContainerHeight);
  }

  NSInteger minVal = NSIntegerMax;
  for (AssistantContainerDetent* detent in self.detents) {
    if (detent.value < minVal) {
      minVal = detent.value;
    }
  }
  return MIN(minVal, absoluteMax);
}

// Calculates the effective maximum height based on detents and safe area.
- (NSInteger)effectiveMaxHeight {
  NSInteger absoluteMax = [self absoluteMaxHeight];

  if (self.detents.count == 0) {
    return absoluteMax;
  }

  NSInteger maxVal = 0;
  for (AssistantContainerDetent* detent in self.detents) {
    if (detent.value > maxVal) {
      maxVal = detent.value;
    }
  }
  return MIN(absoluteMax, maxVal);
}

// Handles the state when the pan gesture changes (drags).
- (void)handlePanGestureChanged:(UIPanGestureRecognizer*)gesture {
  CHECK(gesture == _headerPanGesture);

  UIView* superview = self.view.superview;
  CGPoint translation = [gesture translationInView:superview];
  NSInteger newHeight = round(_initialConstraintHeight - translation.y);

  if (round(_heightConstraint.constant) == newHeight) {
    return;
  }

  NSInteger maxHeight = [self effectiveMaxHeight];
  NSInteger minHeight = [self effectiveMinHeight];

  // Apply logarithmic decay for a "stiffer" feel beyond limits.
  if (newHeight < minHeight) {
    CGFloat diff = minHeight - newHeight;
    newHeight = minHeight - (kRubberBandCoefficient * log(1.0 + diff));
  } else if (newHeight > maxHeight) {
    CGFloat diff = newHeight - maxHeight;
    newHeight = maxHeight + (kRubberBandCoefficient * log(1.0 + diff));
  }

  _heightConstraint.constant = newHeight;
  _hasUserResized = YES;
  if ([self.delegate respondsToSelector:@selector(assistantContainer:
                                                     didUpdateHeight:)]) {
    [self.delegate assistantContainer:self didUpdateHeight:newHeight];
  }
}

// Handles the state when the pan gesture ends or is cancelled.
- (void)handlePanGestureEnded:(UIPanGestureRecognizer*)gesture {
  CHECK(gesture == _headerPanGesture);

  UIView* superview = self.view.superview;
  CGPoint velocity = [gesture velocityInView:superview];

  // Calculate target height based on gesture end state.
  CGFloat currentHeight = _heightConstraint.constant;
  NSInteger targetHeight = [self targetHeightForCurrentHeight:currentHeight
                                                     velocity:velocity];

  _heightConstraint.constant = targetHeight;
  _hasUserResized = YES;

  // Current height from visual frame (approximate start of animation).
  CGFloat currentFrameHeight = self.view.frame.size.height;
  CGFloat distance = targetHeight - currentFrameHeight;
  CGFloat springVelocity = 0.0;

  // Invert velocity so positive values indicate upward expansion.
  CGFloat containerVelocity = -velocity.y;

  if (ABS(distance) > 1.0) {
    springVelocity = containerVelocity / distance;
  }

  // Animate the snap.
  [self animateLayoutIfNeededWithInitialVelocity:springVelocity];
}

// Calculates the target height based on the current height and velocity of the
// gesture.
- (NSInteger)targetHeightForCurrentHeight:(CGFloat)currentHeight
                                 velocity:(CGPoint)velocity {
  NSInteger maxHeight = [self effectiveMaxHeight];
  NSInteger minHeight = [self effectiveMinHeight];

  // If detents are available, use them to determine the target height.
  if (self.detents.count > 0) {
    // Find min and max detent values.
    NSInteger minDetentValue = minHeight;
    NSInteger maxDetentValue = maxHeight;

    // Logic for low velocity.
    // If the user stops dragging with little momentum, and the view is within
    // the valid range [minDetent, maxDetent], we let it stay there.
    // If it's outside that range, we snap to the nearest valid detent.
    if (ABS(velocity.y) <= kFlingVelocityThreshold) {
      if (currentHeight >= minDetentValue && currentHeight <= maxDetentValue) {
        return currentHeight;
      }
    }

    // High velocity (fling) or out of bounds.
    // Snap to the most appropriate detent.
    NSInteger bestDetentValue = 0;
    NSInteger minDistance = NSIntegerMax;

    // Project height based on velocity to simulate momentum.
    NSInteger projectedHeight =
        round(currentHeight - (velocity.y * kMomentumProjectionSeconds));

    for (AssistantContainerDetent* detent in self.detents) {
      NSInteger val = detent.value;
      // Clamp detent value to safe limits.
      val = std::clamp(val, minHeight, maxHeight);

      NSInteger diff = ABS(projectedHeight - val);
      if (diff < minDistance) {
        minDistance = diff;
        bestDetentValue = val;
      }
    }
    CHECK(bestDetentValue);
    return bestDetentValue;
  }

  // Fallback behavior (no detents): Fling to min/max.
  if (velocity.y > kFlingVelocityThreshold) {
    return minHeight;
  } else if (velocity.y < -kFlingVelocityThreshold) {
    return maxHeight;
  } else {
    // Snap to nearest limit.
    if (ABS(currentHeight - minHeight) < ABS(currentHeight - maxHeight)) {
      return minHeight;
    } else {
      return maxHeight;
    }
  }
}

// Updates the height constraint based on preferred content size and detents.
- (void)updateHeightConstraint {
  // If we are currently dragging, do not interfere with the constraint.
  if (_headerPanGesture.state == UIGestureRecognizerStateBegan ||
      _headerPanGesture.state == UIGestureRecognizerStateChanged ||
      self.isAnimating) {
    return;
  }

  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }

  // Calculate limits consistent with gesture logic.
  NSInteger maxHeight = [self effectiveMaxHeight];
  NSInteger minHeight = [self effectiveMinHeight];

  NSInteger preferredHeight = round([_assistantContainerView preferredHeight]);

  // If detents are available, use them to determine the target height.
  // We snap to the nearest detent.
  if (self.detents.count > 0) {
    NSInteger currentHeight = round(_heightConstraint.constant);
    NSInteger nearestDetentValue = 0;
    NSInteger minDistance = NSIntegerMax;
    AssistantContainerDetent* matchedDetent = nil;

    for (AssistantContainerDetent* detent in self.detents) {
      NSInteger val = detent.value;
      val = MAX(minHeight, MIN(val, maxHeight));

      NSInteger diff = ABS(currentHeight - val);
      if (diff < minDistance) {
        minDistance = diff;
        nearestDetentValue = val;
        matchedDetent = detent;
      }
    }

    if (matchedDetent) {
      if (round(_heightConstraint.constant) != nearestDetentValue) {
        _heightConstraint.constant = nearestDetentValue;
        // Animate only if visible.
        if (_hasAppeared && !self.isAnimating) {
          [self animateLayoutIfNeededWithInitialVelocity:0];
        }
      }
      if ([self.delegate respondsToSelector:@selector(assistantContainer:
                                                         didChangeDetent:)]) {
        [self.delegate assistantContainer:self didChangeDetent:matchedDetent];
      }
      return;
    }
  }
  CHECK(!self.detents.count);

  // Fallback to default logic if no detents.
  // Use the user's explicit size if available, clamped to new bounds.
  // Otherwise, default to the preferred height.
  NSInteger target = 0;
  if (_hasUserResized) {
    // Re-clamp current user height with new bounds.
    // (e.g. content might have grown => minHeight grew => push up user height).
    NSInteger current = round(_heightConstraint.constant);
    target = MAX(minHeight, MIN(current, maxHeight));
  } else {
    target = MIN(preferredHeight, maxHeight);
  }

  // Ensure we never break the min height limit.
  target = MAX(target, minHeight);

  if (round(_heightConstraint.constant) != target) {
    _heightConstraint.constant = target;
    // Animate only if visible, auto-sizing, and idle.
    if (_hasAppeared && !_hasUserResized && !self.isAnimating) {
      [self animateLayoutIfNeededWithInitialVelocity:0];
    }
  }
}

// Calculates the maximum allowable height for the container, respecting the
// safe area.
- (NSInteger)absoluteMaxHeight {
  UIView* superview = self.view.superview;
  if (!superview) {
    return 0;
  }

  // We use the view's frame max Y because the container is anchored to a view
  // (e.g. toolbar) that is above the screen bottom.
  CGFloat bottomY = CGRectGetMaxY(self.view.frame);
  CGFloat safeAreaTop = superview.safeAreaInsets.top;

  return round(bottomY - safeAreaTop - kContainerMargin);
}

// Lays out the view anchored to the guide/view within the parent view.
- (void)layoutInParentView:(UIView*)parentView {
  if (!parentView) {
    return;
  }

  NSLayoutYAxisAnchor* bottomAnchor = nil;
  if (self.anchorView) {
    bottomAnchor = self.anchorToBottom ? self.anchorView.bottomAnchor
                                       : self.anchorView.topAnchor;
  }

  if (!bottomAnchor) {
    bottomAnchor = parentView.safeAreaLayoutGuide.bottomAnchor;
  }

  AddSameConstraintsToSidesWithInsets(
      self.view, parentView, LayoutSides::kLeading | LayoutSides::kTrailing,
      NSDirectionalEdgeInsetsMake(0, kContainerMargin, 0, kContainerMargin));

  // Anchor to bottom.
  [self.view.bottomAnchor constraintEqualToAnchor:bottomAnchor
                                         constant:-kContainerMargin]
      .active = YES;

  // Update its value if the user hasn't resized it (e.g. content changed).
  if (!_hasUserResized) {
    if (self.detents.count == 1) {
      _heightConstraint.constant = self.detents.firstObject.value;
    } else {
      CGFloat preferredHeight = [_assistantContainerView preferredHeight];
      CGFloat initialHeight = MAX(preferredHeight, kMinContainerHeight);
      _heightConstraint.constant = initialHeight;
    }
  }
}

// Animates layout changes with standard spring parameters.
- (void)animateLayoutIfNeededWithInitialVelocity:(CGFloat)velocity {
  [UIView animateWithDuration:kSpringDuration
                        delay:0
       usingSpringWithDamping:kSpringDamping
        initialSpringVelocity:velocity
                      options:UIViewAnimationOptionCurveEaseOut |
                              UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     [self.view.superview layoutIfNeeded];
                   }
                   completion:nil];
}

@end
