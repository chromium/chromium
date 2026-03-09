// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"

#import <algorithm>
#import <map>
#import <optional>

#import "base/check.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view.h"
#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_container_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The height assigned to a detent that isn't in the list.
constexpr NSInteger kInvalidDetentHeight = -1;

// Constants used for the container resizing animation.
constexpr CGFloat kSpringDuration = 0.3;
constexpr CGFloat kSpringDamping = 0.85;
constexpr CGFloat kMomentumProjectionSeconds = 0.2;

// The height of the top area that responds to the pan gesture.
constexpr CGFloat kGestureTopAreaHeight = 44.0;

}  // namespace

@interface AssistantContainerViewController () <UIGestureRecognizerDelegate>
@end

@implementation AssistantContainerViewController {
  // Layout constraints for the container.
  NSLayoutConstraint* _heightConstraint;
  NSLayoutConstraint* _leadingConstraint;
  NSLayoutConstraint* _trailingConstraint;
  NSLayoutConstraint* _bottomConstraint;

  // Background dimming view for transitions to large detent.
  UIView* _dimmingView;

  // The view that holds the child view controller.
  AssistantContainerView* _assistantContainerView;

  // State storage for configuration before view load.
  UIViewController* _childViewController;

  // Gesture recognizer for resizing the container.
  UIPanGestureRecognizer* _headerPanGesture;
  // The height of the container when the gesture started.
  CGFloat _initialConstraintHeight;
  // Whether the view has appeared.
  BOOL _hasAppeared;

  // Cached map of calculated heights for the active detents.
  std::map<AssistantContainerDetent, NSInteger> _detentHeights;

  // Tracks the active detent to prevent redundant delegate callbacks and layout
  // loops.
  std::optional<AssistantContainerDetent> _activeDetent;
}

- (instancetype)initWithViewController:(UIViewController*)viewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _childViewController = viewController;
    _detents = {
        AssistantContainerDetent::kMinimized,
        AssistantContainerDetent::kMedium,
        AssistantContainerDetent::kLarge,
    };
  }
  return self;
}

- (void)loadView {
  // Use a ChromeOverlayContainerView as the root view. Its bounds are static,
  // which prevents excessive layout passes in the parent view when resizing
  // the Assistant container.
  self.view = [[ChromeOverlayContainerView alloc] init];

  [self setupDimmingView];

  _assistantContainerView = [[AssistantContainerView alloc] init];
  _assistantContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_assistantContainerView];
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
  CGFloat initialHeight =
      MAX(_detentHeights[self.detents.front()], self.minimizedDetentHeight);
  _heightConstraint = [_assistantContainerView.heightAnchor
      constraintEqualToConstant:initialHeight];
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
  [self updateDetentHeights];
  [self updateHeightConstraint];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        if (strongSelf->_hasAppeared) {
          [strongSelf updateHeightConstraint];
        }
      }
                      completion:nil];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  _hasAppeared = YES;
}

#pragma mark - Public

- (void)animateToDetent:(AssistantContainerDetent)detentIdentifier
               duration:(NSTimeInterval)duration
                  curve:(UIViewAnimationCurve)curve {
  std::vector<AssistantContainerDetent> currentDetents = self.detents;
  auto it =
      std::find(currentDetents.begin(), currentDetents.end(), detentIdentifier);
  if (it == currentDetents.end()) {
    return;
  }

  NSInteger maxHeight = [self effectiveMaxHeight];
  NSInteger minHeight = [self effectiveMinHeight];
  NSInteger targetHeight =
      std::clamp(_detentHeights[detentIdentifier], minHeight, maxHeight);

  _heightConstraint.constant = targetHeight;
  CGFloat targetPercentage = [self expandPercentageForHeight:targetHeight];

  if ([self.delegate
          respondsToSelector:@selector(assistantContainer:
                                 animateAlongsideTransitionToPercentage:)]) {
    [self.delegate assistantContainer:self
        animateAlongsideTransitionToPercentage:targetPercentage];
  }

  [self notifyDelegateOfDetentChangeIfNeeded:detentIdentifier];

  // The shift converts an animation curve to animation options.
  // `UIViewAnimationOptionBeginFromCurrentState` ensures that if an animation
  // is already running, the new animation smoothly interrupts it from its
  // current position rather than snapping to the end state.
  UIViewAnimationOptions options =
      curve << 16 | UIViewAnimationOptionBeginFromCurrentState;

  self.isAnimating = YES;

  if (duration <= 0) {
    [self executeAlongsideAnimationWithPercentage:targetPercentage];
    [self didCompleteDetentAnimationWithDetent:detentIdentifier];
    return;
  }

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:duration
      delay:0
      options:options
      animations:^{
        [weakSelf executeAlongsideAnimationWithPercentage:targetPercentage];
      }
      completion:^(BOOL finished) {
        [weakSelf didCompleteDetentAnimationWithDetent:detentIdentifier];
      }];
}

#pragma mark - Properties

- (void)setIsAnimating:(BOOL)isAnimating {
  if (_isAnimating == isAnimating) {
    return;
  }
  _isAnimating = isAnimating;
  [self updatePanGestureEnabledState];
}

- (void)setDetents:(std::vector<AssistantContainerDetent>)detents {
  CHECK(!detents.empty());
  _detents = std::move(detents);
  std::sort(_detents.begin(), _detents.end(),
            [](AssistantContainerDetent a, AssistantContainerDetent b) {
              return a < b;
            });
  [self updateDetentHeights];
  [self updatePanGestureEnabledState];
  [self.view setNeedsLayout];
}

- (void)setMinimizedDetentHeight:(NSInteger)minimizedDetentHeight {
  _minimizedDetentHeight = minimizedDetentHeight;
  [self updateDetentHeights];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  if (gestureRecognizer != _headerPanGesture) {
    return YES;
  }
  CGPoint location = [touch locationInView:_assistantContainerView];
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

// Configures and adds the background dimming view.
- (void)setupDimmingView {
  _dimmingView = [[UIView alloc] init];
  _dimmingView.translatesAutoresizingMaskIntoConstraints = NO;
  _dimmingView.backgroundColor = UIColor.blackColor;
  _dimmingView.alpha = 0.0;
  [self.view addSubview:_dimmingView];
  AddSameConstraints(_dimmingView, self.view);
}

// Dynamically updates the bounding constraints and border radius based on
// scale.
- (void)updateContainerStylingForHeight:(CGFloat)height {
  CGFloat minimizedHeight =
      _detentHeights[AssistantContainerDetent::kMinimized];
  CGFloat mediumHeight = _detentHeights[AssistantContainerDetent::kMedium];
  CGFloat largeHeight = _detentHeights[AssistantContainerDetent::kLarge];

  ContainerMorphingConstraints constraints = CalculateMorphingConstraints(
      height, minimizedHeight, mediumHeight, largeHeight);

  _heightConstraint.constant = constraints.actual_height;
  _leadingConstraint.constant = constraints.side_margin;
  _trailingConstraint.constant = -constraints.side_margin;
  _bottomConstraint.constant = -constraints.bottom_margin;
  [_assistantContainerView updateCornerRadius:constraints.corner_radius
                                maskedCorners:constraints.masked_corners];
  _dimmingView.alpha = constraints.background_dimming_alpha;
}

// Notifies the delegate of a detent change if it differs from the previously
// notified active detent.
- (void)notifyDelegateOfDetentChangeIfNeeded:
    (AssistantContainerDetent)newDetent {
  if (!_activeDetent.has_value() || _activeDetent.value() != newDetent) {
    _activeDetent = newDetent;
    if ([self.delegate respondsToSelector:@selector(assistantContainer:
                                                       didChangeDetent:)]) {
      [self.delegate assistantContainer:self didChangeDetent:newDetent];
    }
  }
}

// Adds gesture recognizers to the view.
- (void)setUpGestures {
  _headerPanGesture = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePanGesture:)];
  _headerPanGesture.delegate = self;
  [_assistantContainerView addGestureRecognizer:_headerPanGesture];
  [self updatePanGestureEnabledState];
}

// Called when the animation to a detent completes.
- (void)didCompleteDetentAnimationWithDetent:(AssistantContainerDetent)detent {
  self.isAnimating = NO;
}

// Executes the layout pass and notifies the delegate of the transition.
- (void)executeAlongsideAnimationWithPercentage:(CGFloat)percentage {
  [self updateContainerStylingForHeight:_heightConstraint.constant];
  [self.view layoutIfNeeded];

  if ([self.delegate
          respondsToSelector:@selector(assistantContainer:
                                 animateAlongsideTransitionToPercentage:)]) {
    [self.delegate assistantContainer:self
        animateAlongsideTransitionToPercentage:percentage];
  }
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
  return MIN(_detentHeights[self.detents.front()], absoluteMax);
}

// Calculates the effective maximum height based on detents.
- (NSInteger)effectiveMaxHeight {
  NSInteger absoluteMax = [self absoluteMaxHeight];
  return MIN(_detentHeights[self.detents.back()], absoluteMax);
}

// Converts a physical pixel height mathematically into an expansion percentage.
- (CGFloat)expandPercentageForHeight:(CGFloat)height {
  CGFloat minHeight = [self effectiveMinHeight];
  CGFloat maxHeight = [self effectiveMaxHeight];
  if (maxHeight <= minHeight) {
    return 0.0;
  }
  return (height - minHeight) / (maxHeight - minHeight);
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
    NSInteger diff = minHeight - newHeight;
    newHeight = minHeight - RubberBandDistance(diff, minHeight);
  } else if (newHeight > maxHeight) {
    NSInteger diff = newHeight - maxHeight;
    newHeight = maxHeight + RubberBandDistance(diff, maxHeight);
  }

  _heightConstraint.constant = newHeight;
  [self updateContainerStylingForHeight:newHeight];

  CGFloat percentage = [self expandPercentageForHeight:newHeight];
  if ([self.delegate respondsToSelector:@selector(assistantContainer:
                                            didUpdateExpandPercentage:)]) {
    [self.delegate assistantContainer:self
            didUpdateExpandPercentage:percentage];
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
  [self updateContainerStylingForHeight:targetHeight];

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

  NSInteger bestDetentValue = 0;
  NSInteger minDistance = NSIntegerMax;

  // Project height based on velocity to simulate momentum.
  NSInteger projectedHeight =
      round(currentHeight - (velocity.y * kMomentumProjectionSeconds));

  for (AssistantContainerDetent detent : self.detents) {
    NSInteger val = _detentHeights[detent];
    // Clamp detent value to safe limits.
    val = std::clamp(val, minHeight, maxHeight);

    NSInteger diff = ABS(projectedHeight - val);
    if (diff < minDistance) {
      minDistance = diff;
      bestDetentValue = val;
    }
  }
  return bestDetentValue;
}

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

  // If detents are available, use them to determine the target height.
  // We snap to the nearest detent.
  NSInteger currentHeight = round(_heightConstraint.constant);
  NSInteger nearestDetentValue = 0;
  NSInteger minDistance = NSIntegerMax;
  AssistantContainerDetent matchedDetent = self.detents.front();

  for (AssistantContainerDetent detent : self.detents) {
    NSInteger val = _detentHeights[detent];
    val = MAX(minHeight, MIN(val, maxHeight));

    NSInteger diff = ABS(currentHeight - val);
    if (diff < minDistance) {
      minDistance = diff;
      nearestDetentValue = val;
      matchedDetent = detent;
    }
  }

  if (round(_heightConstraint.constant) != nearestDetentValue) {
    _heightConstraint.constant = nearestDetentValue;
    [self updateContainerStylingForHeight:nearestDetentValue];
    // Animate only if visible.
    if (_hasAppeared && !self.isAnimating) {
      [self animateLayoutIfNeededWithInitialVelocity:0];
    }
  }

  [self notifyDelegateOfDetentChangeIfNeeded:matchedDetent];
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

  return round(bottomY - safeAreaTop);
}

// Lays out the view anchored to the guide/view within the parent view.
- (void)layoutInParentView:(UIView*)parentView {
  if (!parentView) {
    return;
  }

  _leadingConstraint.active = NO;
  _trailingConstraint.active = NO;
  _bottomConstraint.active = NO;

  NSLayoutYAxisAnchor* bottomAnchor = nil;
  if (self.anchorView) {
    bottomAnchor = self.anchorToBottom ? self.anchorView.bottomAnchor
                                       : self.anchorView.topAnchor;
  }

  if (!bottomAnchor) {
    bottomAnchor = parentView.safeAreaLayoutGuide.bottomAnchor;
  }

  // Pin the wrapper to the parent view.
  [NSLayoutConstraint activateConstraints:@[
    [self.view.topAnchor constraintEqualToAnchor:parentView.topAnchor],
    [self.view.leadingAnchor constraintEqualToAnchor:parentView.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:parentView.trailingAnchor],
    [self.view.bottomAnchor constraintEqualToAnchor:bottomAnchor],
  ]];

  // Pin the container inside the wrapper (these constraints mutate during
  // morphing).
  _leadingConstraint = [_assistantContainerView.leadingAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor];
  _trailingConstraint = [_assistantContainerView.trailingAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor];

  // Anchor to bottom of the wrapper.
  _bottomConstraint = [_assistantContainerView.bottomAnchor
      constraintEqualToAnchor:self.view.bottomAnchor];

  _leadingConstraint.active = YES;
  _trailingConstraint.active = YES;
  _bottomConstraint.active = YES;

  [self updateDetentHeights];

  // Update its value with the initial height based on detents.
  _heightConstraint.constant =
      MAX(_detentHeights[self.detents.front()], self.minimizedDetentHeight);
  [self updateContainerStylingForHeight:_heightConstraint.constant];
}

// Animates layout changes with standard spring parameters.
- (void)animateLayoutIfNeededWithInitialVelocity:(CGFloat)velocity {
  CGFloat targetHeight = _heightConstraint.constant;
  CGFloat targetPercentage = [self expandPercentageForHeight:targetHeight];

  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kSpringDuration
                        delay:0
       usingSpringWithDamping:kSpringDamping
        initialSpringVelocity:velocity
                      options:UIViewAnimationOptionCurveEaseOut |
                              UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     [weakSelf executeAlongsideAnimationWithPercentage:
                                   targetPercentage];
                   }
                   completion:nil];
}

// Recomputes and caches the heights for all active detents.
- (void)updateDetentHeights {
  _detentHeights[AssistantContainerDetent::kMinimized] = kInvalidDetentHeight;
  _detentHeights[AssistantContainerDetent::kMedium] = kInvalidDetentHeight;
  _detentHeights[AssistantContainerDetent::kLarge] = kInvalidDetentHeight;

  for (AssistantContainerDetent detent : self.detents) {
    switch (detent) {
      case AssistantContainerDetent::kLarge:
        _detentHeights[detent] = [self absoluteMaxHeight];
        break;
      case AssistantContainerDetent::kMedium:
        _detentHeights[detent] = [self absoluteMaxHeight] / 2;
        break;
      case AssistantContainerDetent::kMinimized:
        _detentHeights[detent] = self.minimizedDetentHeight;
        break;
    }
  }
}

@end
