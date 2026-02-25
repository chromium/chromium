// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"

#import "ios/chrome/browser/assistant/ui/assistant_container_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Margin for the container content relative to the screen edges.
constexpr CGFloat kContainerMargin = 5.0;
constexpr CGFloat kMinContainerHeight = 60.0;

// Constants used for the container resizing animation.
constexpr CGFloat kRubberBandCoefficient = 8.0;
constexpr CGFloat kFlingVelocityThreshold = 1000.0;
constexpr CGFloat kSpringDuration = 0.3;
constexpr CGFloat kSpringDamping = 0.85;

}  // namespace

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
  CGFloat preferredHeight = [_assistantContainerView preferredHeight];
  CGFloat initialHeight = MAX(preferredHeight, kMinContainerHeight);
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

#pragma mark - Private

// Adds gesture recognizers to the view.
- (void)setUpGestures {
  _headerPanGesture = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePanGesture:)];
  [_assistantContainerView.headerView addGestureRecognizer:_headerPanGesture];
}

// Handles the pan gesture on the header to resize the container.
- (void)handlePanGesture:(UIPanGestureRecognizer*)gesture {
  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }

  // Calculate limits.
  CGFloat maxHeight = [self maxHeight];
  CGFloat minHeight = kMinContainerHeight;

  if (gesture.state == UIGestureRecognizerStateBegan) {
    _initialConstraintHeight = _heightConstraint.constant;
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    CGPoint translation = [gesture translationInView:superview];
    CGFloat newHeight = _initialConstraintHeight - translation.y;

    // Damped elastic effect when dragging beyond limits.
    // Logarithmic decay for a "stiffer" feel as you pull further.
    if (newHeight < minHeight) {
      CGFloat diff = minHeight - newHeight;
      // log(1 + diff) ensures continuous function starting at 0.
      newHeight = minHeight - (kRubberBandCoefficient * log(1.0 + diff));
    } else if (newHeight > maxHeight) {
      CGFloat diff = newHeight - maxHeight;
      newHeight = maxHeight + (kRubberBandCoefficient * log(1.0 + diff));
    }

    _heightConstraint.constant = newHeight;
    _hasUserResized = YES;

  } else if (gesture.state == UIGestureRecognizerStateEnded ||
             gesture.state == UIGestureRecognizerStateCancelled) {
    CGPoint velocity = [gesture velocityInView:superview];

    // Fling Down: animate to Min Height (Content Size).
    if (velocity.y > kFlingVelocityThreshold) {
      _heightConstraint.constant = minHeight;
      _hasUserResized = YES;
    }
    // Fling Up: animate to Max Height.
    else if (velocity.y < -kFlingVelocityThreshold) {
      _heightConstraint.constant = maxHeight;
      _hasUserResized = YES;
    } else {
      // Low Velocity: stay at current height (already there), or snap back if
      // out of bounds.
      CGFloat current = _heightConstraint.constant;
      if (current < minHeight) {
        _heightConstraint.constant = minHeight;
      } else if (current > maxHeight) {
        _heightConstraint.constant = maxHeight;
      }
    }

    CGFloat targetHeight = _heightConstraint.constant;
    // Current height from visual frame (approximate start of animation).
    CGFloat currentHeight = self.view.frame.size.height;
    CGFloat distance = targetHeight - currentHeight;
    CGFloat springVelocity = 0.0;

    // According to the gesture recognizer, positive velocity means gesture
    // moving down the screen (towards positive y), but it's easier to think
    // about positive velocity meaning container getting taller.
    CGFloat containerVelocity = -velocity.y;

    if (ABS(distance) > 1.0) {
      springVelocity = containerVelocity / distance;
    }

    // Animate the snap.
    [self animateLayoutIfNeededWithInitialVelocity:springVelocity];
  }
}

// Calculates the maximum allowable height for the container, respecting the
// safe area.
- (CGFloat)maxHeight {
  UIView* superview = self.view.superview;
  if (!superview) {
    return 0.0;
  }

  // We use the view's frame max Y because the container is anchored to a view
  // (e.g. toolbar) that is above the screen bottom.
  CGFloat bottomY = CGRectGetMaxY(self.view.frame);
  CGFloat safeAreaTop = superview.safeAreaInsets.top;

  return bottomY - safeAreaTop - kContainerMargin;
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
    CGFloat preferredHeight = [_assistantContainerView preferredHeight];
    CGFloat initialHeight = MAX(preferredHeight, kMinContainerHeight);
    _heightConstraint.constant = initialHeight;
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

// Updates the height constraint based on preferred content size and detents.
- (void)updateHeightConstraint {
  // If we are currently dragging, DO NOT interfere with the constraint.
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
  CGFloat maxHeight = [self maxHeight];

  CGFloat preferredHeight = [_assistantContainerView preferredHeight];
  CGFloat minHeight = kMinContainerHeight;

  // If user has never resized, we default to preferred height (min logic).
  // But strictly speaking, the initial state IS the preferred height.
  // The constraint logic should ensure:
  // 1. Constraint Height >= Preferred Height (Min).
  // 2. Constraint Height <= Max Height.

  // If the user explicitly resized to be LARGER, we preserve that preference
  // as long as it fits within the new bounds.
  CGFloat target = 0.0;
  if (_hasUserResized) {
    // Re-clamp current user height with NEW bounds.
    // (e.g. content might have grown => minHeight grew => push up user height).
    CGFloat current = _heightConstraint.constant;
    target = MAX(minHeight, MIN(current, maxHeight));
  } else {
    target = MIN(preferredHeight, maxHeight);
  }

  // Ensure we never break the min height limit.
  target = MAX(target, kMinContainerHeight);
  if (ABS(_heightConstraint.constant - target) > 0.1) {
    // Animate only if visible, auto-sizing, and idle.
    if (_hasAppeared && !_hasUserResized && !self.isAnimating) {
      _heightConstraint.constant = target;
      [self animateLayoutIfNeededWithInitialVelocity:0];
    } else {
      _heightConstraint.constant = target;
    }
  }
}

@end
