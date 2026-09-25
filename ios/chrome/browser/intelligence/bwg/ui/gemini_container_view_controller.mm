// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
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
}

- (instancetype)
    initWithGeminiViewController:(UIViewController*)geminiViewController
           worklogViewController:
               (ActuationWorklogViewController*)worklogViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _geminiViewController = geminiViewController;
    _worklogViewController = worklogViewController;
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

  // Prevent double-padding/extra spacing at the bottom when the keyboard
  // is hidden.
  self.view.keyboardLayoutGuide.usesBottomSafeArea = NO;

  _geminiContentView = [[UIView alloc] init];
  _geminiContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_geminiContentView];

  AddSameConstraintsToSides(
      _geminiContentView, self.view,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
  [NSLayoutConstraint activateConstraints:@[
    [_geminiContentView.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor]
  ]];

  UIStackView* containerStack = [[UIStackView alloc] init];
  containerStack.translatesAutoresizingMaskIntoConstraints = NO;
  containerStack.axis = UILayoutConstraintAxisVertical;
  containerStack.alignment = UIStackViewAlignmentFill;
  [_geminiContentView addSubview:containerStack];
  AddSameConstraints(containerStack, _geminiContentView);

  if (self.zeroStateViewController) {
    [self addZeroStateToContainer:containerStack];
  }

  if (_geminiViewController) {
    [self addGeminiToContainer:containerStack];
  }

  if (_worklogViewController) {
    [self addWorklogSubviews];
  }
}

#pragma mark - ActuationWorklogViewControllerDelegate

- (void)worklogViewController:(ActuationWorklogViewController*)viewController
              didChangeHeight:(CGFloat)height {
  CGFloat totalDetentHeight =
      height + kWorklogContainerInsets.top + kWorklogContainerInsets.bottom;
  [self.mutator containerDidChangeActuationHeight:totalDetentHeight];
}

#pragma mark - GeminiContainerConsumer

- (void)updateZeroStateVisibility:(BOOL)visible {
  self.zeroStateViewController.view.hidden = !visible;
}

- (void)dismissKeyboard {
  [self.view endEditing:YES];
}

- (void)setWorklogCompact:(BOOL)compact {
  [_worklogViewController setCompact:compact];
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

- (CGFloat)contentHeight {
  [self.view layoutIfNeeded];
  CGSize targetSize = CGSizeMake(self.view.bounds.size.width,
                                 UILayoutFittingCompressedSize.height);
  return [_geminiContentView
               systemLayoutSizeFittingSize:targetSize
             withHorizontalFittingPriority:UILayoutPriorityRequired
                   verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
      .height;
}

#pragma mark - Private

// Adds the zero-state view controller to `containerStack`.
- (void)addZeroStateToContainer:(UIStackView*)containerStack {
  [self addChildViewController:self.zeroStateViewController];
  [containerStack addArrangedSubview:self.zeroStateViewController.view];

  // Allow the zero-state view to expand into any remaining vertical space
  // and compress first when vertical space is constrained (e.g., when the
  // keyboard is presented).
  [self.zeroStateViewController.view
      setContentHuggingPriority:UILayoutPriorityDefaultLow
                        forAxis:UILayoutConstraintAxisVertical];
  [self.zeroStateViewController.view
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  [self.zeroStateViewController didMoveToParentViewController:self];
}

// Adds the Gemini view controller to `containerStack`.
- (void)addGeminiToContainer:(UIStackView*)containerStack {
  [self addChildViewController:_geminiViewController];
  [containerStack addArrangedSubview:_geminiViewController.view];

  // Keep `_geminiViewController` sized strictly to its intrinsic content
  // height and prevent it from compressing when vertical space is
  // constrained.
  [_geminiViewController.view
      setContentHuggingPriority:UILayoutPriorityDefaultHigh
                        forAxis:UILayoutConstraintAxisVertical];
  [_geminiViewController.view
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [_geminiViewController didMoveToParentViewController:self];
}

// Adds the actuation worklog to the view hierarchy.
- (void)addWorklogSubviews {
  _worklogViewController.delegate = self;
  [self addChildViewController:_worklogViewController];
  _worklogViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  _worklogViewController.view.hidden = YES;
  [self.view addSubview:_worklogViewController.view];
  AddSameConstraintsToSidesWithInsets(
      _worklogViewController.view, self.view,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing,
      kWorklogContainerInsets);
  [NSLayoutConstraint activateConstraints:@[
    [_worklogViewController.view.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                       constant:-kWorklogContainerInsets.bottom]
  ]];
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
