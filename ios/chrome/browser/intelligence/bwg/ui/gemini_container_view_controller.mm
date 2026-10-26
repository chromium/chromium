// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"

#import <algorithm>

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_zero_state_background_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Small padding applied to all sides inside the container.
constexpr CGFloat kContainerPadding = 14.0;

// Standard insets applied around the worklog inside the container.
constexpr NSDirectionalEdgeInsets kWorklogContainerInsets =
    NSDirectionalEdgeInsets{intelligence::actor::kSpacingLarge, 0,
                            intelligence::actor::kSpacingLarge, 0};
}  // namespace

@interface GeminiContainerViewController () <
    ActuationWorklogViewControllerDelegate>
@end

@implementation GeminiContainerViewController {
  // The child view controller wrapping the actual Gemini UI provided by the
  // provider.
  UIViewController* _geminiViewController;
  // The child view controller displaying compact actuation status.
  ActuationWorklogViewController* _worklogViewController;
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
}

- (instancetype)initWithWorklogViewController:
    (ActuationWorklogViewController*)worklogViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _worklogViewController = worklogViewController;
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
    [self addWorklogSubviews];
  }
}

#pragma mark - ActuationWorklogViewControllerDelegate

- (void)worklogViewController:(ActuationWorklogViewController*)viewController
              didChangeHeight:(CGFloat)height {
  [self.mutator containerDidChangeActuationHeight:
                    [self containerHeightForWorklogHeight:height]];
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
  _worklogViewController.displayMode = displayMode;
}

- (void)setActuationActive:(BOOL)active {
  _geminiContentView.hidden = active;
  _worklogViewController.view.hidden = !active;
  if (!active) {
    return;
  }
  // The worklog may have reported its height before the mutator entered the
  // actuating state (`ActorService` observer order is not guaranteed), in which
  // case that report was ignored. Re-send it now that it will be applied.
  [_worklogViewController notifyHeightDidChange];
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

// TODO(crbug.com/532204179): Push the minimized height alongside the compact
// height so the minimized detent stays in sync with header height changes.
- (CGFloat)actuationMinimizedDetentHeight {
  return
      [self containerHeightForWorklogHeight:
                [_worklogViewController
                    heightForDisplayMode:ActuationWorklogDisplayModeMinimized]];
}

#pragma mark - Private

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
// the insets applied around the worklog in `addWorklogSubviews`.
// TODO(crbug.com/532204179): Move `kWorklogContainerInsets` into
// `ActuationWorklogViewController` so it reports padded heights directly.
- (CGFloat)containerHeightForWorklogHeight:(CGFloat)worklogHeight {
  return worklogHeight + kWorklogContainerInsets.top +
         kWorklogContainerInsets.bottom;
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

// Adds the actuation worklog to the view hierarchy.
- (void)addWorklogSubviews {
  _worklogViewController.delegate = self;
  [self addChildViewController:_worklogViewController];
  _worklogViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  _worklogViewController.view.hidden = YES;
  [self.view addSubview:_worklogViewController.view];
  AddSameConstraintsWithInsets(_worklogViewController.view, self.view,
                               kWorklogContainerInsets);
  [_worklogViewController didMoveToParentViewController:self];
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
