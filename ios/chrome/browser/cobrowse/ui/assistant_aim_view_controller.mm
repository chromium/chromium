// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_view_controller.h"

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_header_view.h"
#import "ios/chrome/browser/composebox/ui/composebox_input_plate_view_controller.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

constexpr CGFloat kInputPlateMargin = 10.0f;
constexpr CGFloat kTitleVerticalMargin = 12.0;
constexpr CGFloat kHeaderCenteringVerticalMargin = 16.0;
constexpr CGFloat kThresholdForClosedState = 0.12;
constexpr CGFloat kThresholdForCompleteVisibility = 0.3;

}  // namespace

@interface AssistantAIMViewController () <AssistantAIMHeaderViewDelegate>
@end

@implementation AssistantAIMViewController {
  UIView* _webStateView;
  NSArray<NSLayoutConstraint*>* _webStateViewConstraints;
  ComposeboxInputPlateViewController* _inputViewController;
  AssistantAIMHeaderView* _headerView;
  NSLayoutConstraint* _headerTopMargin;
}

@synthesize delegate = _delegate;

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setUpHeader];
  [self setUpWebStateView];
}

- (void)addInputViewController:
    (ComposeboxInputPlateViewController*)inputViewController {
  [self loadViewIfNeeded];
  [self addChildViewController:inputViewController];
  [self.view addSubview:inputViewController.view];
  inputViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [inputViewController didMoveToParentViewController:self];
  _inputViewController = inputViewController;

  [_inputViewController.view
      setContentHuggingPriority:UILayoutPriorityRequired
                        forAxis:UILayoutConstraintAxisVertical];
  // Allow compression on the input view to limit it's height in the available
  // space (between the keyboard and the top of the view).
  [_inputViewController.view
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];

  [self setupConstraints];
}

- (void)adjustForContainerOpenPercentage:(CGFloat)percentage {
  // The percentage to use for animations, that is proportional to the container
  // open percentage with more sudden thresholds.
  CGFloat effectPercentage;
  if (percentage < kThresholdForClosedState) {
    effectPercentage = 0;
  } else if (percentage > kThresholdForCompleteVisibility) {
    effectPercentage = 1;
  } else {
    effectPercentage =
        (percentage - kThresholdForClosedState) /
        (kThresholdForCompleteVisibility - kThresholdForClosedState);
  }

  // This ensures the header end up centered in the collapsed state.
  _headerTopMargin.constant =
      kHeaderCenteringVerticalMargin +
      effectPercentage *
          (kTitleVerticalMargin - kHeaderCenteringVerticalMargin);

  _inputViewController.view.alpha = effectPercentage;
  _webStateView.alpha = effectPercentage;
  [_headerView adjustForPercentage:effectPercentage];
}

- (void)setupConstraints {
  NSLayoutConstraint* attachInputPlateToKeyboard =
      [_inputViewController.view.bottomAnchor
          constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                         constant:-kInputPlateMargin];
  attachInputPlateToKeyboard.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    attachInputPlateToKeyboard,
    [_inputViewController.view.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                 constant:-kInputPlateMargin],
    [_inputViewController.view.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kInputPlateMargin],
    [_inputViewController.view.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor
                       constant:-kInputPlateMargin]
  ]];
}

#pragma mark - AssistantAIMConsumer

- (void)setWebStateView:(UIView*)webStateView {
  if (_webStateView == webStateView) {
    return;
  }
  [_webStateView removeFromSuperview];
  _webStateView = webStateView;
  [self setUpWebStateView];
}

#pragma mark - Private helpers

// Sets up the web state view.
- (void)setUpWebStateView {
  if (!_webStateView || !self.isViewLoaded) {
    return;
  }

  if (_webStateViewConstraints) {
    [NSLayoutConstraint deactivateConstraints:_webStateViewConstraints];
    _webStateViewConstraints = nil;
  }

  _webStateView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view insertSubview:_webStateView atIndex:0];

  _webStateViewConstraints = @[
    [_webStateView.topAnchor constraintEqualToAnchor:_headerView.bottomAnchor
                                            constant:kTitleVerticalMargin],
    [_webStateView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_webStateView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_webStateView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
  ];
  [NSLayoutConstraint activateConstraints:_webStateViewConstraints];
}

// Sets up the title.
- (void)setUpHeader {
  _headerView = [[AssistantAIMHeaderView alloc] init];
  _headerView.translatesAutoresizingMaskIntoConstraints = NO;
  // TODO(crbug.com/492442806): Update title.
  [_headerView setTitle:@"Commuter Bike"];
  _headerView.delegate = self;
  [self.view addSubview:_headerView];

  _headerTopMargin =
      [_headerView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                            constant:kTitleVerticalMargin];
  [NSLayoutConstraint activateConstraints:@[
    _headerTopMargin,
    [_headerView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_headerView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_headerView.heightAnchor constraintEqualToConstant:40],
  ]];
}

#pragma mark - AssistantAIMHeaderViewDelegate

- (void)assistantAIMHeaderViewDidPressClose:
    (AssistantAIMHeaderView*)headerView {
  [self.delegate assistantAIMViewControllerDidTapClose:self];
}

@end
