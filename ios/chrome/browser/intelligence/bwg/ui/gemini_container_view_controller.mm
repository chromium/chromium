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
  // Wrapper holding the worklog and the intervention, hidden by the container
  // since the worklog manages its own view's `hidden`.
  UIView* _actuationContentView;
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
  [self updateActuationContentVisibility];
}

#pragma mark - ActuationWorklogViewControllerDelegate

- (void)worklogViewControllerDidChangeHeight:
    (ActuationWorklogViewController*)viewController {
  [self reportActuationHeight];
}

#pragma mark - ActuationInterventionViewControllerDelegate

- (void)interventionViewControllerDidChangeHeight:
    (ActuationInterventionViewController*)viewController {
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
  [self updateActuationContentVisibility];
}

- (void)setActuationActive:(BOOL)active {
  if (_actuationActive == active) {
    return;
  }
  _actuationActive = active;
  [self updateActuationContentVisibility];
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
      [self containerHeightForWorklogHeight:
                [_worklogViewController
                    heightForDisplayMode:ActuationWorklogDisplayModeMinimized]];
}

#pragma mark - Private

// Shows either the Gemini content or the worklog. During actuation, the worklog
// is shown, except at its expanded mode where the Gemini content and its inline
// worklog replace the full worklog. The intervention is shown with the worklog,
// except while minimized.
// TODO(crbug.com/587910518): Keep the worklog header and interventions visible
// around the Gemini content at the expanded mode.
- (void)updateActuationContentVisibility {
  BOOL showGeminiContent =
      !_actuationActive ||
      _worklogViewController.displayMode == ActuationWorklogDisplayModeExpanded;
  _geminiContentView.hidden = !showGeminiContent;
  _actuationContentView.hidden = showGeminiContent;
  _interventionViewController.view.hidden =
      _worklogViewController.displayMode ==
      ActuationWorklogDisplayModeMinimized;
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

// Returns the container height needed to fit `worklogHeight`, accounting for
// the insets applied around the worklog in `addActuationSubviews`.
// TODO(crbug.com/581337610): Move `kWorklogContainerInsets` into
// `ActuationWorklogViewController` so it reports padded heights directly.
- (CGFloat)containerHeightForWorklogHeight:(CGFloat)worklogHeight {
  return worklogHeight + kWorklogContainerInsets.top +
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
      [_interventionViewController fittingHeight];
  [self.mutator containerDidChangeActuationHeight:
                    [self containerHeightForWorklogHeight:actuationHeight]];
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

// Adds the actuation worklog to the view hierarchy, with the intervention drawn
// over its bottom, below the worklog's content.
- (void)addActuationSubviews {
  _actuationContentView = [[UIView alloc] init];
  _actuationContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_actuationContentView];
  AddSameConstraintsWithInsets(_actuationContentView, self.view,
                               kWorklogContainerInsets);

  _worklogViewController.delegate = self;
  [self addChildViewController:_worklogViewController];
  UIView* worklogView = _worklogViewController.view;
  worklogView.translatesAutoresizingMaskIntoConstraints = NO;
  [_actuationContentView addSubview:worklogView];
  AddSameConstraints(worklogView, _actuationContentView);
  [_worklogViewController didMoveToParentViewController:self];

  _interventionViewController.delegate = self;
  [self addChildViewController:_interventionViewController];
  UIView* interventionView = _interventionViewController.view;
  interventionView.translatesAutoresizingMaskIntoConstraints = NO;
  [_actuationContentView addSubview:interventionView];
  AddSameConstraintsToSides(interventionView, _actuationContentView,
                            LayoutSides::kBottom | LayoutSides::kHorizontal);
  [_interventionViewController didMoveToParentViewController:self];
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
