// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_sheet_view_controller.h"

#import "ios/chrome/browser/assistant/ui/assistant_sheet_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Margin for the sheet content relative to the screen edges.
constexpr CGFloat kSheetMargin = 5.0;
constexpr CGFloat kMinSheetHeight = 100.0;

}  // namespace

@implementation AssistantSheetViewController {
  NSLayoutConstraint* _heightConstraint;
  AssistantSheetView* _assistantSheetView;

  // State storage for configuration before view load.
  AssistantNavbarConfiguration* _navbarConfiguration;
  UIViewController* _childViewController;
}

- (void)loadView {
  _assistantSheetView = [[AssistantSheetView alloc] init];
  self.view = _assistantSheetView;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;

  // Set up target for the close button.
  [_assistantSheetView.closeButton addTarget:self
                                      action:@selector(didTapClose)
                            forControlEvents:UIControlEventTouchUpInside];

  [self setUpGestures];

  // Apply pending configuration.
  if (_navbarConfiguration) {
    [_assistantSheetView setTitle:_navbarConfiguration.title];
  }
  if (_childViewController) {
    [self addChildViewController:_childViewController];
    _childViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    [_assistantSheetView.contentView addSubview:_childViewController.view];
    AddSameConstraints(_childViewController.view,
                       _assistantSheetView.contentView);
    [_childViewController didMoveToParentViewController:self];
  }
}

#pragma mark - Private

// Adds gesture recognizers to the view.
- (void)setUpGestures {
  // TODO(crbug.com/469050167): Implement Pan gesture for interactive resizing.
}

// Notifies the delegate that the close button was tapped.
- (void)didTapClose {
  [self.delegate assistantSheetViewControllerDidTapClose:self];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (parent) {
    [self layoutInParentView:parent.view];
  }
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
      NSDirectionalEdgeInsetsMake(0, kSheetMargin, 0, kSheetMargin));

  CGFloat preferredHeight = [_assistantSheetView preferredHeight];
  CGFloat initialHeight = MAX(preferredHeight, kMinSheetHeight);

  _heightConstraint =
      [self.view.heightAnchor constraintEqualToConstant:initialHeight];

  [NSLayoutConstraint activateConstraints:@[
    [self.view.bottomAnchor constraintEqualToAnchor:bottomAnchor
                                           constant:-kSheetMargin],
    _heightConstraint,
  ]];
}

#pragma mark - AssistantSheetConsumer

- (void)setNavigationBarConfiguration:
    (AssistantNavbarConfiguration*)configuration {
  _navbarConfiguration = configuration;
  if (self.isViewLoaded) {
    [_assistantSheetView setTitle:configuration.title];
    // Update height in case title lines changed.
    [self updateHeightConstraint];
  }
}

- (void)setChildViewController:(UIViewController*)viewController {
  if (viewController == _childViewController) {
    return;
  }

  // Remove existing child view controllers.
  if (_childViewController) {
    [_childViewController willMoveToParentViewController:nil];
    [_childViewController.view removeFromSuperview];
    [_childViewController removeFromParentViewController];
  }

  _childViewController = viewController;

  if (self.isViewLoaded) {
    [self addChildViewController:viewController];
    viewController.view.translatesAutoresizingMaskIntoConstraints = NO;
    [_assistantSheetView.contentView addSubview:viewController.view];
    AddSameConstraints(viewController.view, _assistantSheetView.contentView);
    [viewController didMoveToParentViewController:self];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateHeightConstraint];
}

// Updates the height constraint based on preferred content size and detents.
- (void)updateHeightConstraint {
  if (!_heightConstraint.active) {
    return;
  }

  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }

  CGFloat safeAreaTop = superview.safeAreaInsets.top;
  CGFloat maxHeight =
      CGRectGetMaxY(self.view.frame) - safeAreaTop - kSheetMargin;

  CGFloat preferredHeight = [_assistantSheetView preferredHeight];

  // TODO(crbug.com/469050167): Handle detents (expansion vs content).
  CGFloat targetHeight = MIN(preferredHeight, maxHeight);
  targetHeight = MAX(targetHeight, kMinSheetHeight);

  // Check for significant change to prevent infinite layout loops.
  if (ABS(_heightConstraint.constant - targetHeight) > 0.1) {
    _heightConstraint.constant = targetHeight;
  }
}

@end
