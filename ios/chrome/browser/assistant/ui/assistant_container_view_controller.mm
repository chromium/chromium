// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_container_view_controller.h"

#import <algorithm>
#import <map>
#import <optional>

#import "base/check.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_accessibility_manager.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_layout_utils.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_view.h"
#import "ios/chrome/browser/assistant/ui/assistant_grabber_button.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/chrome_overlay_window/chrome_overlay_container_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The height assigned to a detent that isn't in the list.
constexpr NSInteger kInvalidDetentHeight = -1;

// The maximum width of the sheet container on iPad devices.
constexpr CGFloat kAssistantSheetMaxWidth = 700.0;
// The multiplier for the width of the sheet container relative to its parent.
constexpr CGFloat kAssistantSheetWidthMultiplier = 2.0 / 3.0;

// The absolute minimum padding between the top of the container and the top of
// the screen if no safe area insets exist (e.g. iPad full screen, iPhone
// landscape).
constexpr CGFloat kMinTopPadding = 12.0;

// Default percentage for the medium detent.
constexpr NSInteger kDefaultMediumDetentPercentage = 50;

// Returns the height for the medium detent, taking into account the
// experimental setting percentage.
NSInteger GetMediumDetentHeight(NSInteger absoluteMax) {
  NSInteger percentage =
      GetAssistantMediumDetentPercentage() ?: kDefaultMediumDetentPercentage;
  return absoluteMax * (percentage / 100.0);
}

}  // namespace

@interface AssistantContainerViewController () <
    LayoutStateObserver,
    UIGestureRecognizerDelegate,
    AssistantContainerAccessibilityManagerDelegate>
@end

@implementation AssistantContainerViewController {
  // The view that holds the child view controller.
  AssistantContainerView* _assistantContainerView;
  // Background dimming view for transitions to large detent.
  UIView* _dimmingView;

  // Layout constraints for the container.
  NSLayoutConstraint* _heightConstraint;
  NSLayoutConstraint* _leadingConstraint;
  NSLayoutConstraint* _trailingConstraint;
  NSLayoutConstraint* _outerBottomConstraint;
  NSLayoutConstraint* _innerBottomConstraint;

  // Layout constraints for width-restricted contexts (iPad/Landscape).
  NSArray<NSLayoutConstraint*>* _widthRestrictedConstraints;
  // Constraints pinning the container to the parent view for side panel.
  NSArray<NSLayoutConstraint*>* _sidePanelConstraints;

  // State storage for configuration before view load.
  UIViewController* _childViewController;
  // Cached map of calculated heights for the active detents.
  std::map<AssistantContainerDetent, NSInteger> _detentHeights;
  // Tracks the active detent to prevent redundant delegate callbacks and layout
  // loops.
  std::optional<AssistantContainerDetent> _activeDetent;
  // The height of the container when the gesture started.
  CGFloat _initialConstraintHeight;
  // Tracks whether the active gesture is currently dragging the container.
  BOOL _isDraggingContainer;
  // Whether the view has appeared.
  BOOL _hasAppeared;

  // Gesture recognizer for resizing the container.
  UIPanGestureRecognizer* _headerPanGesture;

  // An array of unowned scroll views, modeled through provider blocks that
  // capture weak references to the views in question.
  NSMutableArray<UIScrollView* (^)()>* _disabledScrollViews;

  // Manages accessibility properties and actions.
  AssistantContainerAccessibilityManager* _accessibilityManager;
}

@synthesize isAnimating = _isAnimating;

- (instancetype)initWithViewController:(UIViewController*)viewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _childViewController = viewController;
    _disabledScrollViews = [[NSMutableArray alloc] init];
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
  UIView* view = self.view;
  view.translatesAutoresizingMaskIntoConstraints = NO;

  [self setUpGestures];

  [self
      registerForTraitChanges:
          @[ UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class ]
                   withAction:@selector(onTraitChange)];

  // Apply pending configuration.
  if (_childViewController) {
    [self addChildViewController:_childViewController];
    _childViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    [_assistantContainerView.contentView addSubview:_childViewController.view];
    AddSameConstraints(_childViewController.view,
                       _assistantContainerView.contentView);
    [_childViewController didMoveToParentViewController:self];
  }

  [self updateAccessibilityIdentifier];

  // Create and activate the height constraint.
  CGFloat initialHeight =
      MAX(_detentHeights[self.detents.front()], self.minimizedDetentHeight);
  _heightConstraint = [_assistantContainerView.heightAnchor
      constraintEqualToConstant:initialHeight];
  _heightConstraint.active = YES;

  // Pin the container inside the wrapper, these constraints mutate during
  // morphing.
  _leadingConstraint = [_assistantContainerView.leadingAnchor
      constraintEqualToAnchor:view.safeAreaLayoutGuide.leadingAnchor];
  _trailingConstraint = [_assistantContainerView.trailingAnchor
      constraintEqualToAnchor:view.safeAreaLayoutGuide.trailingAnchor];

  NSLayoutConstraint* proportionalWidthConstraint =
      [_assistantContainerView.widthAnchor
          constraintEqualToAnchor:view.widthAnchor
                       multiplier:kAssistantSheetWidthMultiplier];
  proportionalWidthConstraint.priority = UILayoutPriorityRequired - 1;

  // Set up width-restricted constraints, inactive by default.
  _widthRestrictedConstraints = @[
    [_assistantContainerView.centerXAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.centerXAnchor],
    proportionalWidthConstraint,
    [_assistantContainerView.widthAnchor
        constraintLessThanOrEqualToConstant:kAssistantSheetMaxWidth]
  ];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (_innerBottomConstraint) {
    [NSLayoutConstraint deactivateConstraints:@[
      _innerBottomConstraint, _outerBottomConstraint
    ]];
    _innerBottomConstraint = nil;
    _outerBottomConstraint = nil;
  }
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

  AssistantContainerDetent detentBeforeRotation = _activeDetent.value();

  __weak __typeof(self) weakSelf = self;
  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> context) {
        // Do nothing here to avoid snapping during transition.
      }
      completion:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [weakSelf completeOrientationTransitionWithDetent:detentBeforeRotation];
      }];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  _hasAppeared = YES;

  // Focus or announce the grabber button on entry.
  if (_assistantContainerView.grabberButton) {
    if (self.announceArrivalOnly) {
      NSString* message = l10n_util::GetNSString(
          IDS_IOS_ASSISTANT_SHEET_GRABBER_ACCESSIBILITY_LABEL);
      UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                      message);
      return;
    }
    UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                    _assistantContainerView.grabberButton);
  }
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

  NSInteger targetHeight = _detentHeights[detentIdentifier];

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

- (void)setLayoutState:(LayoutState*)layoutState {
  if (_layoutState == layoutState) {
    return;
  }
  [_layoutState removeObserver:self];
  _layoutState = layoutState;
  [_layoutState addObserver:self];

  if (_layoutState) {
    [self updatePresentationContextForSupportedState:
              _layoutState.containedLayoutSupported];
  }
}

- (void)setPresentationContext:
    (AssistantPresentationContext)presentationContext {
  if (_presentationContext == presentationContext) {
    return;
  }
  _presentationContext = presentationContext;

  if ([self.delegate respondsToSelector:@selector(assistantContainer:
                                                    didChangeContext:)]) {
    [self.delegate assistantContainer:self
                     didChangeContext:presentationContext];
  }

  if (presentationContext == AssistantPresentationContext::kSheet &&
      _activeDetent.has_value()) {
    if ([self.delegate respondsToSelector:@selector(assistantContainer:
                                                       didChangeDetent:)]) {
      [self.delegate assistantContainer:self
                        didChangeDetent:_activeDetent.value()];
    }
  }

  [self applyLayoutForPresentationContext];
}

- (void)setDelegate:(id<AssistantContainerDelegate>)delegate {
  if (_delegate == delegate) {
    return;
  }
  _delegate = delegate;

  if (_heightConstraint &&
      [_delegate respondsToSelector:@selector(assistantContainer:
                                        didUpdateExpandPercentage:)]) {
    CGFloat percentage =
        [self expandPercentageForHeight:_heightConstraint.constant];
    [_delegate assistantContainer:self didUpdateExpandPercentage:percentage];
  }
}

- (void)setDetents:(std::vector<AssistantContainerDetent>)detents {
  CHECK(!detents.empty());
  _detents = std::move(detents);
  std::sort(_detents.begin(), _detents.end(),
            [](AssistantContainerDetent a, AssistantContainerDetent b) {
              return a < b;
            });
  [self updateDetentHeights];
  [self updateInteractionEnabledState];
  [self.view setNeedsLayout];

  // Accessibility properties for sheet resizing are not relevant in panel mode.
  if (self.presentationContext == AssistantPresentationContext::kPanel) {
    return;
  }

  if (!_activeDetent.has_value()) {
    return;
  }

  [_accessibilityManager
      updateAccessibilityPropertiesWithCurrentDetent:_activeDetent.value()
                                    availableDetents:self.detents];
}

- (void)setMinimizedDetentHeight:(NSInteger)minimizedDetentHeight {
  _minimizedDetentHeight = minimizedDetentHeight;
  [self updateDetentHeights];
}

- (void)setAnchorView:(UIView*)anchorView {
  if (_anchorView == anchorView) {
    return;
  }
  _anchorView = anchorView;
  [self updateHeightConstraint];
}

#pragma mark - AssistantContainerAnimatable

- (void)setIsAnimating:(BOOL)isAnimating {
  if (_isAnimating == isAnimating) {
    return;
  }
  _isAnimating = isAnimating;
  [self updateInteractionEnabledState];
}

- (UIView*)dimmingView {
  return _dimmingView;
}

- (UIView*)assistantContainerView {
  return _assistantContainerView;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  if (gestureRecognizer != _headerPanGesture) {
    return YES;
  }

  if ([otherGestureRecognizer.view isKindOfClass:[UIScrollView class]]) {
    UIScrollView* scrollView =
        static_cast<UIScrollView*>(otherGestureRecognizer.view);

    // If the scroll view is not part of the assistant content, pause it.
    BOOL inAssistantContent = [scrollView isDescendantOfView:self.view];
    if (!inAssistantContent) {
      [self pauseScrollView:scrollView];
      return YES;
    }

    if ([self.delegate
            respondsToSelector:@selector(assistantContainer:
                                      shouldPauseScrollView:forGesture:)]) {
      if ([self.delegate assistantContainer:self
                      shouldPauseScrollView:scrollView
                                 forGesture:otherGestureRecognizer]) {
        [self pauseScrollView:scrollView];
        return YES;
      }
    }
  }

  return NO;
}

#pragma mark - Private

// Completes the orientation transition by animating to the active detent.
- (void)completeOrientationTransitionWithDetent:
    (AssistantContainerDetent)detent {
  [self animateToDetent:detent
               duration:kAssistantSheetSpringDuration
                  curve:UIViewAnimationCurveEaseInOut];
}

// Resumes all the previously paused scroll views.
- (void)resumeAllScrollViews {
  for (UIScrollView* (^scrollViewProvider)() in _disabledScrollViews) {
    UIScrollView* scrollView = scrollViewProvider();
    scrollView.scrollEnabled = YES;
  }
  [_disabledScrollViews removeAllObjects];
}

// Pauses scrolling on the given scroll view.
- (void)pauseScrollView:(UIScrollView*)scrollView {
  if (scrollView.scrollEnabled) {
    __weak UIScrollView* weakScrollView = scrollView;
    [_disabledScrollViews addObject:^{
      return weakScrollView;
    }];
    scrollView.scrollEnabled = NO;
  }
}

// Configures and adds the background dimming view.
- (void)setupDimmingView {
  _dimmingView = [[UIView alloc] init];
  _dimmingView.translatesAutoresizingMaskIntoConstraints = NO;
  _dimmingView.backgroundColor = UIColor.blackColor;
  _dimmingView.alpha = 0.0;

  UITapGestureRecognizer* tapGesture = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleDimmingViewTap:)];
  [_dimmingView addGestureRecognizer:tapGesture];

  [self.view addSubview:_dimmingView];
  AddSameConstraints(_dimmingView, self.view);
}

// Dynamically updates the bounding constraints and border radius based on
// scale.
- (void)updateContainerStylingForHeight:(CGFloat)height {
  if (_presentationContext == AssistantPresentationContext::kPanel) {
    // The SceneViewController handles its UI layout in Panel mode.
    [_assistantContainerView updateTopCornerRadius:0 bottomCornerRadius:0];
    return;
  }

  CGFloat minimizedHeight =
      _detentHeights[AssistantContainerDetent::kMinimized];
  CGFloat mediumHeight = _detentHeights[AssistantContainerDetent::kMedium];
  CGFloat largeHeight = _detentHeights[AssistantContainerDetent::kLarge];

  ContainerMorphingConstraints constraints = CalculateMorphingConstraints(
      height, minimizedHeight, mediumHeight, largeHeight);

  if (IsRegularXRegularSizeClass(self.traitCollection)) {
    // iPad floating sheet always has 4 rounded corners and a bottom margin.
    constraints.top_corner_radius = kMorphingBaseCornerRadius;
    constraints.bottom_corner_radius = kMorphingBaseCornerRadius;
    constraints.bottom_margin = kMorphingBaseMargin;
  }

  _heightConstraint.constant = constraints.actual_height;
  _leadingConstraint.constant = constraints.side_margin;
  _trailingConstraint.constant = -constraints.side_margin;
  _innerBottomConstraint.constant = -constraints.bottom_margin;
  [_assistantContainerView
      updateTopCornerRadius:constraints.top_corner_radius
         bottomCornerRadius:constraints.bottom_corner_radius];
  _dimmingView.alpha = constraints.background_dimming_alpha;
}

// Updates the accessibility identifier of the container view based on the
// current detent.
- (void)updateAccessibilityIdentifier {
  if (!_assistantContainerView) {
    return;
  }
  AssistantContainerDetent detent =
      _activeDetent.value_or(self.detents.front());
  switch (detent) {
    case AssistantContainerDetent::kMinimized:
      _assistantContainerView.accessibilityIdentifier =
          kAssistantContainerDetentMinimizedIdentifier;
      break;
    case AssistantContainerDetent::kMedium:
      _assistantContainerView.accessibilityIdentifier =
          kAssistantContainerDetentMediumIdentifier;
      break;
    case AssistantContainerDetent::kLarge:
      _assistantContainerView.accessibilityIdentifier =
          kAssistantContainerDetentLargeIdentifier;
      break;
  }
}

// Notifies the delegate of a detent change if it differs from the previously
// notified active detent.
- (void)notifyDelegateOfDetentChangeIfNeeded:
    (AssistantContainerDetent)newDetent {
  if (_activeDetent != newDetent) {
    _activeDetent = newDetent;
    [self updateAccessibilityIdentifier];

    if ([self.delegate respondsToSelector:@selector(assistantContainer:
                                                       didChangeDetent:)]) {
      [self.delegate assistantContainer:self didChangeDetent:newDetent];
    }

    if (self.presentationContext == AssistantPresentationContext::kPanel) {
      return;
    }

    [_accessibilityManager
        updateAccessibilityPropertiesWithCurrentDetent:newDetent
                                      availableDetents:self.detents];

    // Announce the new state without losing VoiceOver focus.
    NSString* valueString =
        _assistantContainerView.grabberButton.accessibilityValue;
    UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                    valueString);
  }
}

// Adds gesture recognizers to the view.
- (void)setUpGestures {
  // Pan gesture for resizing the container.
  _headerPanGesture = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePanGesture:)];
  _headerPanGesture.delegate = self;
  [_assistantContainerView addGestureRecognizer:_headerPanGesture];

  // Configure the grabber button action for toggling container size.
  [_assistantContainerView.grabberButton
             addTarget:self
                action:@selector(handleGrabberButtonTapped:)
      forControlEvents:UIControlEventTouchUpInside];

  _accessibilityManager = [[AssistantContainerAccessibilityManager alloc]
      initWithGrabberButton:_assistantContainerView.grabberButton
                   delegate:self];
  _assistantContainerView.grabberButton.accessibilityDelegate =
      _accessibilityManager;

  [self updateInteractionEnabledState];
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

// Updates the interaction enabled state based on animation and detents.
- (void)updateInteractionEnabledState {
  // Prevent interactions from interfering with the animation.
  if (self.isAnimating) {
    _headerPanGesture.enabled = NO;
    _assistantContainerView.grabberButton.enabled = NO;
    return;
  }

  _headerPanGesture.enabled = YES;
  _assistantContainerView.grabberButton.enabled = YES;
}

// Handles the tap on the grabber button to cycle through detents.
- (void)handleGrabberButtonTapped:(UIButton*)sender {
  if (self.isAnimating) {
    return;
  }

  std::vector<AssistantContainerDetent> currentDetents = self.detents;

  AssistantContainerDetent currentDetent =
      _activeDetent.value_or(currentDetents.front());
  auto it =
      std::find(currentDetents.begin(), currentDetents.end(), currentDetent);

  size_t nextIndex = 0;
  if (it != currentDetents.end()) {
    size_t currentIndex = std::distance(currentDetents.begin(), it);
    nextIndex = (currentIndex + 1) % currentDetents.size();
  }

  AssistantContainerDetent targetDetent = currentDetents[nextIndex];

  [self animateToDetent:targetDetent
               duration:kAssistantSheetSpringDuration
                  curve:UIViewAnimationCurveEaseInOut];
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

// Handles the tap gesture on the dimming view.
- (void)handleDimmingViewTap:(UITapGestureRecognizer*)gesture {
  if (_activeDetent != AssistantContainerDetent::kLarge) {
    return;
  }

  AssistantContainerDetent detent = self.detents.front();
  if (detent == AssistantContainerDetent::kLarge) {
    return;
  }

  [self animateToDetent:detent
               duration:kAssistantSheetSpringDuration
                  curve:UIViewAnimationCurveEaseInOut];
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
  CGFloat minHeight = self.minimizedDetentHeight;
  CGFloat maxHeight = [self absoluteMaxHeight];
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

  // Transition to container dragging.
  if (!_isDraggingContainer) {
    _isDraggingContainer = YES;
    [gesture setTranslation:CGPointZero inView:superview];
    _initialConstraintHeight = _heightConstraint.constant;
    newHeight = round(_initialConstraintHeight);
  }

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

  // Lock interaction and prevent height recalculations immediately.
  self.isAnimating = YES;

  [self resumeAllScrollViews];
  _isDraggingContainer = NO;

  UIView* superview = self.view.superview;
  CGPoint velocity = [gesture velocityInView:superview];

  // Calculate target height based on gesture end state.
  CGFloat currentHeight = _heightConstraint.constant;
  AssistantContainerDetent targetDetent =
      [self targetDetentForCurrentHeight:currentHeight velocity:velocity];
  NSInteger targetHeight = _detentHeights[targetDetent];

  _heightConstraint.constant = targetHeight;
  [self updateContainerStylingForHeight:targetHeight];

  [self notifyDelegateOfDetentChangeIfNeeded:targetDetent];

  [self animateSnapToTargetHeight:targetHeight gestureVelocity:velocity];
}

// Calculates spring velocity and triggers the snap animation.
- (void)animateSnapToTargetHeight:(NSInteger)targetHeight
                  gestureVelocity:(CGPoint)velocity {
  CGFloat currentFrameHeight = self.view.frame.size.height;
  CGFloat distance = targetHeight - currentFrameHeight;

  // If the distance is very small, skip the animation to prevent UIKit from
  // skipping the completion block and leaving isAnimating stuck at YES.
  if (ABS(distance) <= 1.0) {
    [self.view layoutIfNeeded];
    self.isAnimating = NO;
    return;
  }

  CGFloat springVelocity = 0.0;

  // Invert velocity so positive values indicate upward expansion.
  CGFloat containerVelocity = -velocity.y;

  // If the velocity direction is opposite to the distance direction (e.g.,
  // moving away from the target during an overshoot), set springVelocity to 0
  // to ensure a smooth settle without wild bouncing.
  if ((distance > 0 && containerVelocity < 0) ||
      (distance < 0 && containerVelocity > 0)) {
    springVelocity = 0;
  } else if (ABS(distance) > 1.0) {
    springVelocity = containerVelocity / distance;
  }

  // Animate the snap.
  [self animateLayoutIfNeededWithInitialVelocity:springVelocity];
}

// Calculates the target height based on the current height and velocity of the
// gesture.
- (AssistantContainerDetent)targetDetentForCurrentHeight:(CGFloat)currentHeight
                                                velocity:(CGPoint)velocity {
  NSInteger maxHeight = [self effectiveMaxHeight];
  NSInteger minHeight = [self effectiveMinHeight];

  AssistantContainerDetent bestDetent = self.detents.front();
  NSInteger minDistance = NSIntegerMax;

  // Project height based on velocity to simulate momentum.
  NSInteger projectedHeight = round(
      currentHeight - (velocity.y * kAssistantSheetMomentumProjectionSeconds));

  for (AssistantContainerDetent detent : self.detents) {
    NSInteger val = _detentHeights[detent];
    // Clamp detent value to safe limits.
    val = std::clamp(val, minHeight, maxHeight);

    NSInteger diff = ABS(projectedHeight - val);
    if (diff < minDistance) {
      minDistance = diff;
      bestDetent = detent;
    }
  }
  return bestDetent;
}

- (void)updateHeightConstraint {
  // If we are currently dragging, do not interfere with the constraint.
  if (_headerPanGesture.state == UIGestureRecognizerStateBegan ||
      _headerPanGesture.state == UIGestureRecognizerStateChanged ||
      _headerPanGesture.state == UIGestureRecognizerStateEnded ||
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
  CGFloat maxAvailableHeight = self.view.frame.size.height;
  CGFloat safeAreaTop = self.view.safeAreaInsets.top;

  // Fallback to window safe area if view safe area is not yet available.
  if (safeAreaTop == 0 && self.view.window) {
    safeAreaTop = self.view.window.safeAreaInsets.top;
  }

  CGFloat topInset = MAX(safeAreaTop, kMinTopPadding);

  return round(maxAvailableHeight - topInset);
}

// Lays out the view anchored to the guide/view within the parent view.
- (void)layoutInParentView:(UIView*)parentView {
  if (!parentView) {
    return;
  }

  NSLayoutYAxisAnchor* bottomAnchor = nil;
  if (self.anchorView) {
    bottomAnchor = self.anchorView.topAnchor;
  }

  if (!bottomAnchor) {
    bottomAnchor = parentView.safeAreaLayoutGuide.bottomAnchor;
  }

  UIView* view = self.view;

  // Anchor the container directly to the dynamic bottom anchor.
  if (!_innerBottomConstraint) {
    _innerBottomConstraint = [_assistantContainerView.bottomAnchor
        constraintEqualToAnchor:view.bottomAnchor];
    _outerBottomConstraint =
        [view.bottomAnchor constraintEqualToAnchor:bottomAnchor];

    [NSLayoutConstraint activateConstraints:@[
      _outerBottomConstraint, _innerBottomConstraint
    ]];
  }

  // Trigger initial adaptive layout once the view is successfully in the
  // hierarchy.
  [self applyLayoutForPresentationContext];
  [self
      updatePresentationContextForSupportedState:self.layoutState
                                                     .containedLayoutSupported];
}

// Updates the presentation context based on the layout state.
- (void)updatePresentationContextForSupportedState:(BOOL)supported {
  if (!self.view.window) {
    return;
  }

  AssistantPresentationContext targetContext =
      supported ? AssistantPresentationContext::kPanel
                : AssistantPresentationContext::kSheet;

  if (_presentationContext != targetContext) {
    self.presentationContext = targetContext;
  }
}

// Called when system traits change.
- (void)onTraitChange {
  if (_presentationContext == AssistantPresentationContext::kSheet) {
    // Re-evaluate sheet constraints on pure trait changes.
    [self applySheetLayoutConstraints];
  }
}

#pragma mark - LayoutStateObserver

- (void)layoutState:(LayoutState*)layoutState
    didChangeContainedLayoutSupported:(BOOL)supported {
  [self updatePresentationContextForSupportedState:supported];
}

// Configures the constraints for the panel layout.
- (void)applyPanelLayoutConstraints {
  UIView* view = self.view;

  [NSLayoutConstraint deactivateConstraints:_widthRestrictedConstraints];
  [NSLayoutConstraint deactivateConstraints:@[
    _leadingConstraint, _trailingConstraint, _innerBottomConstraint,
    _outerBottomConstraint, _heightConstraint
  ]];

  if (!_sidePanelConstraints) {
    _sidePanelConstraints = @[
      [_assistantContainerView.leadingAnchor
          constraintEqualToAnchor:view.safeAreaLayoutGuide.leadingAnchor],
      [_assistantContainerView.trailingAnchor
          constraintEqualToAnchor:view.safeAreaLayoutGuide.trailingAnchor],
      [_assistantContainerView.topAnchor
          constraintEqualToAnchor:view.safeAreaLayoutGuide.topAnchor],
      [_assistantContainerView.bottomAnchor
          constraintEqualToAnchor:view.safeAreaLayoutGuide.bottomAnchor]
    ];
  }
  [NSLayoutConstraint activateConstraints:_sidePanelConstraints];

  _headerPanGesture.enabled = NO;
  _dimmingView.hidden = YES;
  _assistantContainerView.grabberButton.hidden = YES;
  _assistantContainerView.accessibilityViewIsModal = NO;
}

// Applies the constraints and view states for the current presentation context.
- (void)applyLayoutForPresentationContext {
  if (_presentationContext == AssistantPresentationContext::kPanel) {
    [self applyPanelLayoutConstraints];
    return;
  }

  // Sheet layout.
  [self applySheetLayoutConstraints];
}

// Configures the constraints for the sheet layout.
- (void)applySheetLayoutConstraints {
  UIView* view = self.view;

  [NSLayoutConstraint deactivateConstraints:_sidePanelConstraints];
  [NSLayoutConstraint activateConstraints:@[
    _outerBottomConstraint, _innerBottomConstraint, _heightConstraint
  ]];

  _headerPanGesture.enabled = YES;
  _dimmingView.hidden = NO;
  _assistantContainerView.grabberButton.hidden = NO;
  _assistantContainerView.accessibilityViewIsModal = YES;

  if (IsRegularXRegularSizeClass(self.traitCollection) ||
      IsIPhoneLandscapeLayout(self.traitCollection)) {
    [NSLayoutConstraint
        deactivateConstraints:@[ _leadingConstraint, _trailingConstraint ]];
    [NSLayoutConstraint activateConstraints:_widthRestrictedConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_widthRestrictedConstraints];
    [NSLayoutConstraint
        activateConstraints:@[ _leadingConstraint, _trailingConstraint ]];
  }

  // By only calling setNeedsLayout, we are batching all constraint changes. We
  // allow the height calculations to finish, and UIKit will then perform a
  // single layout pass at the end.
  [view setNeedsLayout];

  [self updateDetentHeights];

  if (_activeDetent.has_value()) {
    _heightConstraint.constant = _detentHeights[_activeDetent.value()];
  }

  [self updateHeightConstraint];
  [self updateContainerStylingForHeight:_heightConstraint.constant];

  if (_hasAppeared && !self.isAnimating) {
    [self animateLayoutIfNeededWithInitialVelocity:0];
  }
}

// Animates layout changes with standard spring parameters.
- (void)animateLayoutIfNeededWithInitialVelocity:(CGFloat)velocity {
  CGFloat targetHeight = _heightConstraint.constant;
  CGFloat targetPercentage = [self expandPercentageForHeight:targetHeight];

  self.isAnimating = YES;

  __weak __typeof(self) weakSelf = self;

  [UIView animateWithDuration:kAssistantSheetSpringDuration
      delay:0
      usingSpringWithDamping:kAssistantSheetSpringDamping
      initialSpringVelocity:velocity
      options:UIViewAnimationOptionCurveEaseOut |
              UIViewAnimationOptionBeginFromCurrentState
      animations:^{
        [weakSelf executeAlongsideAnimationWithPercentage:targetPercentage];
      }
      completion:^(BOOL finished) {
        weakSelf.isAnimating = NO;
      }];
}

// Recomputes and caches the heights for all active detents.
- (void)updateDetentHeights {
  _detentHeights[AssistantContainerDetent::kMinimized] = kInvalidDetentHeight;
  _detentHeights[AssistantContainerDetent::kMedium] = kInvalidDetentHeight;
  _detentHeights[AssistantContainerDetent::kLarge] = kInvalidDetentHeight;

  NSInteger absoluteMax = [self absoluteMaxHeight];
  for (AssistantContainerDetent detent : self.detents) {
    switch (detent) {
      case AssistantContainerDetent::kLarge:
        _detentHeights[detent] = absoluteMax;
        break;
      case AssistantContainerDetent::kMedium:
        _detentHeights[detent] = GetMediumDetentHeight(absoluteMax);
        break;
      case AssistantContainerDetent::kMinimized:
        _detentHeights[detent] = self.minimizedDetentHeight;
        break;
    }
  }
}

#pragma mark - AssistantContainerAccessibilityManagerDelegate

- (void)accessibilityManagerDidRequestDetentChange:
    (AssistantContainerDetent)detent {
  [self animateToDetent:detent
               duration:kAssistantSheetSpringDuration
                  curve:UIViewAnimationCurveEaseInOut];
}

@end
