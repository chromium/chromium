// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"

#import <algorithm>

#import "base/check_op.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_intervention_view_controller.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_zero_state_background_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Small padding applied to all sides inside the container.
constexpr CGFloat kContainerPadding = 14.0;

// Standard insets applied around the worklog and the intervention inside the
// container.
constexpr NSDirectionalEdgeInsets kWorklogContainerInsets =
    NSDirectionalEdgeInsets{intelligence::actor::kSpacingLarge, 0,
                            intelligence::actor::kSpacingLarge, 0};
}  // namespace

@interface GeminiContainerViewController () <
    ActuationInterventionViewControllerDelegate,
    ActuationWorklogViewControllerDelegate>
@end

@implementation GeminiContainerViewController {
  // The child view controller wrapping the actual Gemini UI provided by the
  // provider.
  UIViewController* _geminiViewController;
  // The child view controller displaying compact actuation status.
  ActuationWorklogViewController* _worklogViewController;
  // The child view controller displaying the actuation intervention, below
  // `_worklogViewController`.
  ActuationInterventionViewController* _interventionViewController;
  // Wrapper view holding the Gemini UI and zero-state suggestions. This allows
  // to hide all of gemini UI without forcing a collapse with the default
  // handling of `hidden` by the stack view.
  UIView* _geminiContentView;
  // Stack view containing the zero-state and Gemini child views.
  UIStackView* _containerStack;
  // Background halo gradient view displayed when in zero-state.
  GeminiContainerZeroStateBackgroundView* _zeroStateBackgroundView;

  // Layout constraints pinning `_geminiContentView` to `self.view`.
  NSLayoutConstraint* _contentViewTopConstraint;
  NSLayoutConstraint* _contentViewLeadingConstraint;
  NSLayoutConstraint* _contentViewTrailingConstraint;
  NSLayoutConstraint* _contentViewBottomConstraint;
  // Measured heights of the worklog and the intervention.
  NSLayoutConstraint* _worklogHeightConstraint;
  NSLayoutConstraint* _interventionHeightConstraint;
  // Width at which `_interventionHeightConstraint` was last measured.
  CGFloat _interventionMeasurementWidth;
  // Pins `_geminiContentView` between the worklog and the intervention, instead
  // of `_contentViewTopConstraint` and `_contentViewBottomConstraint`, in
  // `ActuationWorklogDisplayModeExpanded`.
  NSArray<NSLayoutConstraint*>* _expandedContentViewConstraints;

  // Whether the container content has padding applied.
  BOOL _hasPadding;
  // Whether actuation is in progress.
  BOOL _actuationActive;
}

- (instancetype)initWithWorklogViewController:
                    (ActuationWorklogViewController*)worklogViewController
                   interventionViewController:
                       (ActuationInterventionViewController*)
                           interventionViewController {
  CHECK_EQ(worklogViewController == nil, interventionViewController == nil);
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _worklogViewController = worklogViewController;
    _interventionViewController = interventionViewController;
    _hasPadding = YES;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
  [defaultCenter addObserver:self
                    selector:@selector(keyboardWillShow:)
                        name:UIKeyboardWillShowNotification
                      object:nil];

  _geminiContentView = [[UIView alloc] init];
  _geminiContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_geminiContentView];

  CGFloat initialPadding = _hasPadding ? kContainerPadding : 0.0;
  _contentViewTopConstraint =
      [_geminiContentView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                                   constant:initialPadding];
  _contentViewLeadingConstraint = [_geminiContentView.leadingAnchor
      constraintEqualToAnchor:self.view.leadingAnchor
                     constant:initialPadding];
  _contentViewTrailingConstraint = [_geminiContentView.trailingAnchor
      constraintEqualToAnchor:self.view.trailingAnchor
                     constant:-initialPadding];
  _contentViewBottomConstraint = [_geminiContentView.bottomAnchor
      constraintEqualToAnchor:self.view.bottomAnchor
                     constant:-initialPadding];
  [NSLayoutConstraint activateConstraints:@[
    _contentViewTopConstraint,
    _contentViewLeadingConstraint,
    _contentViewTrailingConstraint,
    _contentViewBottomConstraint,
  ]];

  _containerStack = [[UIStackView alloc] init];
  _containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  _containerStack.axis = UILayoutConstraintAxisVertical;
  _containerStack.alignment = UIStackViewAlignmentFill;
  [_geminiContentView addSubview:_containerStack];
  AddSameConstraints(_containerStack, _geminiContentView);

  if (self.zeroStateViewController) {
    [self addZeroStateToContainer:_containerStack];
  }

  if (_geminiViewController) {
    [self addGeminiToContainer:_containerStack];
  }

  if (_worklogViewController) {
    [self addActuationSubviews];
  }
  // Applies the actuation state set before the view loaded.
  [self updateActuationLayout];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  // The sheet's width changes with its detent, which changes how the
  // intervention wraps. Measuring before the layout pass applies the new height
  // in the same pass, instead of one frame late.
  if (_worklogViewController &&
      [self interventionWidth] != _interventionMeasurementWidth) {
    [self updateInterventionHeight];
  }
}

#pragma mark - ActuationWorklogViewControllerDelegate

- (void)worklogViewControllerDidChangeHeight:
    (ActuationWorklogViewController*)viewController {
  [self updateActuationLayout];
  [self reportActuationHeight];
}

#pragma mark - ActuationInterventionViewControllerDelegate

- (void)interventionViewControllerDidChangeHeight:
    (ActuationInterventionViewController*)viewController {
  [self updateActuationLayout];
  [self reportActuationHeight];
}

#pragma mark - GeminiContainerConsumer

- (void)setGeminiViewController:(UIViewController*)geminiViewController {
  if (_geminiViewController == geminiViewController) {
    return;
  }
  if (_geminiViewController.parentViewController == self) {
    [_geminiViewController willMoveToParentViewController:nil];
    [_geminiViewController.view removeFromSuperview];
    [_geminiViewController removeFromParentViewController];
  }
  _geminiViewController = geminiViewController;
  if (_containerStack && _geminiViewController) {
    [self addGeminiToContainer:_containerStack];
  }
}

- (void)updateZeroStateVisibility:(BOOL)visible {
  self.zeroStateViewController.view.hidden = !visible;
  _zeroStateBackgroundView.hidden = !visible;
}

- (void)dismissKeyboard {
  [self.view endEditing:YES];
}

- (void)setWorklogDisplayMode:(ActuationWorklogDisplayMode)displayMode {
  if (_worklogViewController.displayMode == displayMode) {
    return;
  }
  _worklogViewController.displayMode = displayMode;
  [self updateActuationLayout];
}

- (void)setActuationActive:(BOOL)active {
  if (_actuationActive == active) {
    return;
  }
  _actuationActive = active;
  [self updateActuationLayout];
  if (!active) {
    return;
  }
  // The children may have reported their height before the mutator entered the
  // actuating state (`ActorService` observer order is not guaranteed), in which
  // case that report was ignored. Re-send it now that it will be applied.
  [self reportActuationHeight];
}

- (void)setHasPadding:(BOOL)hasPadding {
  if (_hasPadding == hasPadding) {
    return;
  }
  _hasPadding = hasPadding;
  [self updateLayoutConstraintsForPadding];
}

- (CGFloat)contentHeight {
  [self.view layoutIfNeeded];
  CGFloat padding = _hasPadding ? kContainerPadding : 0.0;
  CGFloat containerWidth = self.view.bounds.size.width;
  CGFloat contentWidth =
      containerWidth > 0 ? std::max(0.0, containerWidth - 2 * padding) : 0.0;
  CGSize targetSize =
      CGSizeMake(contentWidth, UILayoutFittingCompressedSize.height);
  CGFloat contentViewHeight =
      [_geminiContentView
            systemLayoutSizeFittingSize:targetSize
          withHorizontalFittingPriority:UILayoutPriorityRequired
                verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
          .height;
  return contentViewHeight == 0 ? 0 : contentViewHeight + 2 * padding;
}

// TODO(crbug.com/581337477): Push the minimized height alongside the compact
// height so the minimized detent stays in sync with header height changes.
- (CGFloat)actuationMinimizedDetentHeight {
  return
      [self containerHeightForActuationHeight:
                [_worklogViewController
                    heightForDisplayMode:ActuationWorklogDisplayModeMinimized]];
}

#pragma mark - Private

// Lays out the Gemini content, the worklog and the intervention for the
// actuation state. At the expanded mode, the Gemini content is between the
// worklog header and the intervention. Otherwise, it stays pinned to the
// container, hidden during actuation, since collapsing it would make the SDK
// lose its scroll position.
- (void)updateActuationLayout {
  if (!self.isViewLoaded || !_worklogViewController) {
    return;
  }
  ActuationWorklogDisplayMode mode = _worklogViewController.displayMode;
  BOOL expanded =
      _actuationActive && mode == ActuationWorklogDisplayModeExpanded;
  _geminiContentView.hidden = _actuationActive && !expanded;
  _worklogViewController.view.hidden = !_actuationActive;
  _interventionViewController.view.hidden =
      !_actuationActive || mode == ActuationWorklogDisplayModeMinimized;
  _worklogHeightConstraint.constant =
      [_worklogViewController heightForDisplayMode:mode];
  [self updateInterventionHeight];

  // Deactivates first to avoid transiently conflicting constraints.
  NSArray<NSLayoutConstraint*>* edgeConstraints =
      @[ _contentViewTopConstraint, _contentViewBottomConstraint ];
  if (expanded) {
    [NSLayoutConstraint deactivateConstraints:edgeConstraints];
    [NSLayoutConstraint activateConstraints:_expandedContentViewConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_expandedContentViewConstraints];
    [NSLayoutConstraint activateConstraints:edgeConstraints];
  }
}

// Updates content view layout constraints based on whether padding is applied.
- (void)updateLayoutConstraintsForPadding {
  if (!self.isViewLoaded) {
    return;
  }
  CGFloat padding = _hasPadding ? kContainerPadding : 0.0;
  _contentViewTopConstraint.constant = padding;
  _contentViewLeadingConstraint.constant = padding;
  _contentViewTrailingConstraint.constant = -padding;
  _contentViewBottomConstraint.constant = -padding;
}

// Returns the container height needed to fit `actuationHeight`, accounting for
// the insets applied above the worklog and below the intervention in
// `addActuationSubviews`.
// TODO(crbug.com/581337610): Move `kWorklogContainerInsets` into
// `ActuationWorklogViewController` so it reports padded heights directly.
- (CGFloat)containerHeightForActuationHeight:(CGFloat)actuationHeight {
  return actuationHeight + kWorklogContainerInsets.top +
         kWorklogContainerInsets.bottom;
}

// Reports the container height fitting the compact worklog and the
// intervention below it, used for the medium detent.
- (void)reportActuationHeight {
  if (!_worklogViewController) {
    return;
  }
  CGFloat actuationHeight =
      [_worklogViewController
          heightForDisplayMode:ActuationWorklogDisplayModeCompact] +
      [_interventionViewController
          fittingHeightForWidth:[self interventionWidth]];
  [self.mutator containerDidChangeActuationHeight:
                    [self containerHeightForActuationHeight:actuationHeight]];
}

// Returns the width of the intervention, which spans `self.view` within
// `kWorklogContainerInsets`.
- (CGFloat)interventionWidth {
  return CGRectGetWidth(self.view.bounds) - kWorklogContainerInsets.leading -
         kWorklogContainerInsets.trailing;
}

// Fits `_interventionHeightConstraint` to the intervention at its width.
- (void)updateInterventionHeight {
  _interventionMeasurementWidth = [self interventionWidth];
  _interventionHeightConstraint.constant = [_interventionViewController
      fittingHeightForWidth:_interventionMeasurementWidth];
}

// Adds the zero-state view controller to `containerStack`.
- (void)addZeroStateToContainer:(UIStackView*)containerStack {
  _zeroStateBackgroundView =
      [[GeminiContainerZeroStateBackgroundView alloc] init];
  _zeroStateBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
  _zeroStateBackgroundView.hidden = self.zeroStateViewController.view.hidden;
  [self.view insertSubview:_zeroStateBackgroundView
              belowSubview:_geminiContentView];
  AddSameConstraints(_zeroStateBackgroundView, self.view);

  [self addChildViewController:self.zeroStateViewController];
  [containerStack addArrangedSubview:self.zeroStateViewController.view];

  [self.zeroStateViewController didMoveToParentViewController:self];
}

// Adds the Gemini view controller to `containerStack`.
- (void)addGeminiToContainer:(UIStackView*)containerStack {
  [self addChildViewController:_geminiViewController];
  [containerStack addArrangedSubview:_geminiViewController.view];

  [_geminiViewController didMoveToParentViewController:self];
}

// Adds the worklog at the top and the intervention at the bottom of
// `self.view`, with heights measured by the container.
- (void)addActuationSubviews {
  [self addChildViewController:_worklogViewController];
  UIView* worklogView = _worklogViewController.view;
  worklogView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:worklogView];
  AddSameConstraintsToSidesWithInsets(
      worklogView, self.view, LayoutSides::kTop | LayoutSides::kHorizontal,
      kWorklogContainerInsets);
  // Starts at the real height, as 0 conflicts with the worklog's content.
  CGFloat worklogHeight = [_worklogViewController
      heightForDisplayMode:_worklogViewController.displayMode];
  _worklogHeightConstraint =
      [worklogView.heightAnchor constraintEqualToConstant:worklogHeight];
  _worklogHeightConstraint.active = YES;
  [_worklogViewController didMoveToParentViewController:self];

  [self addChildViewController:_interventionViewController];
  UIView* interventionView = _interventionViewController.view;
  interventionView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:interventionView];
  AddSameConstraintsToSidesWithInsets(interventionView, self.view,
                                      LayoutSides::kHorizontal,
                                      kWorklogContainerInsets);
  _interventionHeightConstraint =
      [interventionView.heightAnchor constraintEqualToConstant:0];
  // Optional, so that the intervention slides below the worklog and is clipped
  // when the sheet is too short for both.
  NSLayoutConstraint* interventionBottomConstraint = [self.view.bottomAnchor
      constraintEqualToAnchor:interventionView.bottomAnchor
                     constant:kWorklogContainerInsets.bottom];
  interventionBottomConstraint.priority = UILayoutPriorityRequired - 1;
  [NSLayoutConstraint activateConstraints:@[
    _interventionHeightConstraint,
    interventionBottomConstraint,
    // Never overlaps the worklog.
    [interventionView.topAnchor
        constraintGreaterThanOrEqualToAnchor:worklogView.bottomAnchor],
  ]];
  [_interventionViewController didMoveToParentViewController:self];

  _expandedContentViewConstraints = @[
    [_geminiContentView.topAnchor
        constraintEqualToAnchor:worklogView.bottomAnchor],
    [_geminiContentView.bottomAnchor
        constraintEqualToAnchor:interventionView.topAnchor],
  ];

  // Set last, as loading the children's views reports their heights.
  _worklogViewController.delegate = self;
  _interventionViewController.delegate = self;
}

// Called right before the keyboard is shown.
- (void)keyboardWillShow:(NSNotification*)notification {
  // Only proceed if the keyboard appeared because the view inside this
  // container or its subviews are the first responder.
  if (!GetFirstResponderSubview(self.view)) {
    return;
  }

  NSDictionary* userInfo = notification.userInfo;
  NSTimeInterval duration =
      [userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue];
  UIViewAnimationCurve curve = static_cast<UIViewAnimationCurve>(
      [userInfo[UIKeyboardAnimationCurveUserInfoKey] integerValue]);
  [self.mutator containerKeyboardDidShowWithDuration:duration curve:curve];
}

@end
