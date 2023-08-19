// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter.h"

#import "base/check.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_presenter_delegate.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_view_controller.h"
#import "ios/chrome/browser/ui/popup_menu/public/popup_menu_view_controller_delegate.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
const CGFloat kMinHeight = 200;
const CGFloat kMinWidth = 200;
const CGFloat kMaxWidth = 300;
const CGFloat kMaxHeight = 435;
const CGFloat kMinWidthDifference = 50;
const CGFloat kMinHorizontalMargin = 5;
const CGFloat kMinVerticalMargin = 15;
const CGFloat kDamping = 0.85;
}  // namespace

@interface PopupMenuPresenter () <PopupMenuViewControllerDelegate>
@property(nonatomic, strong) PopupMenuViewController* popupViewController;
// Constraints used for the initial positioning of the popup.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* initialConstraints;
// Constraints used for the positioning of the popup when presented.
@property(nonatomic, strong) NSArray<NSLayoutConstraint*>* presentedConstraints;

@property(nonatomic, strong)
    NSLayoutConstraint* presentedViewControllerHeightConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* presentedViewControllerWidthConstraint;
@end

@implementation PopupMenuPresenter

@synthesize baseViewController = _baseViewController;
@synthesize delegate = _delegate;
@synthesize popupViewController = _popupViewController;
@synthesize initialConstraints = _initialConstraints;
@synthesize presentedConstraints = _presentedConstraints;
@synthesize presentedViewController = _presentedViewController;

#pragma mark - Public

- (void)prepareForPresentation {
  DCHECK(self.baseViewController);
  if (self.popupViewController)
    return;

  self.popupViewController = [[PopupMenuViewController alloc] init];
  self.popupViewController.delegate = self;
  [self.presentedViewController.view
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh + 1
                                      forAxis:UILayoutConstraintAxisHorizontal];

  // Set the frame of the table view to the maximum width to have the label
  // resizing correctly.
  CGRect frame = self.presentedViewController.view.frame;
  frame.size.width = kMaxWidth;
  self.presentedViewController.view.frame = frame;
  // It is necessary to do a first layout pass so the table view can size
  // itself.
  [self.presentedViewController.view setNeedsLayout];
  [self.presentedViewController.view layoutIfNeeded];

  // Set the sizing constraints, in case the UIViewController is using a
  // UIScrollView. The priority needs to be non-required to allow downsizing if
  // needed, and more than UILayoutPriorityDefaultHigh to take precedence on
  // compression resistance.
  self.presentedViewControllerWidthConstraint =
      [self.presentedViewController.view.widthAnchor
          constraintEqualToConstant:0];
  self.presentedViewControllerWidthConstraint.priority =
      UILayoutPriorityDefaultHigh + 1;

  self.presentedViewControllerHeightConstraint =
      [self.presentedViewController.view.heightAnchor
          constraintEqualToConstant:0];
  self.presentedViewControllerHeightConstraint.priority =
      UILayoutPriorityDefaultHigh + 1;

  // Set the constraint constants to their correct intial values.
  [self setPresentedViewControllerConstraintConstants];

  UIView* popup = self.popupViewController.contentContainer;
  [NSLayoutConstraint activateConstraints:@[
    self.presentedViewControllerWidthConstraint,
    self.presentedViewControllerHeightConstraint,
    [popup.heightAnchor constraintLessThanOrEqualToConstant:kMaxHeight],
    [popup.widthAnchor constraintLessThanOrEqualToConstant:kMaxWidth],
    [popup.widthAnchor constraintGreaterThanOrEqualToConstant:kMinWidth],
  ]];
  [self.popupViewController addContent:self.presentedViewController];

  [self.baseViewController addChildViewController:self.popupViewController];
  [self.baseViewController.view addSubview:self.popupViewController.view];
  self.popupViewController.view.frame = self.baseViewController.view.bounds;

  [popup.widthAnchor constraintLessThanOrEqualToAnchor:self.popupViewController
                                                           .view.widthAnchor
                                              constant:-kMinWidthDifference]
      .active = YES;

  UILayoutGuide* layoutGuide = self.layoutGuide;
  self.initialConstraints = @[
    [popup.centerXAnchor constraintEqualToAnchor:layoutGuide.centerXAnchor],
    [popup.centerYAnchor constraintEqualToAnchor:layoutGuide.centerYAnchor],
  ];
  [self setUpPresentedConstraints];

  // Configure the initial state of the animation.
  popup.alpha = 0;
  popup.transform = CGAffineTransformMakeScale(0.1, 0.1);
  [NSLayoutConstraint activateConstraints:self.initialConstraints];
  [self.baseViewController.view layoutIfNeeded];

  [self.popupViewController
      didMoveToParentViewController:self.baseViewController];
}

- (void)presentAnimated:(BOOL)animated {
  [NSLayoutConstraint deactivateConstraints:self.initialConstraints];
  [NSLayoutConstraint activateConstraints:self.presentedConstraints];
  [self
      animate:^{
        self.popupViewController.contentContainer.alpha = 1;
        [self.baseViewController.view layoutIfNeeded];
        self.popupViewController.contentContainer.transform =
            CGAffineTransformIdentity;
      }
      withCompletion:^(BOOL finished) {
        if ([self.delegate
                respondsToSelector:@selector(containedPresenterDidPresent:)]) {
          [self.delegate containedPresenterDidPresent:self];
        }
      }];
}

- (void)dismissAnimated:(BOOL)animated {
  [self.popupViewController willMoveToParentViewController:nil];
  // Notify the presented view controller that it will be removed to prevent it
  // from triggering unnecessary layout passes, which might lead to a hang. See
  // crbug.com/1126618.
  [self.presentedViewController willMoveToParentViewController:nil];
  [NSLayoutConstraint deactivateConstraints:self.presentedConstraints];
  [NSLayoutConstraint activateConstraints:self.initialConstraints];
  auto completion = ^(BOOL finished) {
    [self.popupViewController.view removeFromSuperview];
    [self.popupViewController removeFromParentViewController];
    self.popupViewController = nil;
    if ([self.delegate
            respondsToSelector:@selector(containedPresenterDidDismiss:)]) {
      [self.delegate containedPresenterDidDismiss:self];
    }
  };
  if (animated) {
    [self
               animate:^{
                 self.popupViewController.contentContainer.alpha = 0;
                 [self.baseViewController.view layoutIfNeeded];
                 self.popupViewController.contentContainer.transform =
                     CGAffineTransformMakeScale(0.1, 0.1);
               }
        withCompletion:completion];
  } else {
    completion(YES);
  }
}

#pragma mark - Private

// Animate the `animations` then execute `completion`.
- (void)animate:(void (^)(void))animation
    withCompletion:(void (^)(BOOL finished))completion {
  [UIView animateWithDuration:kMaterialDuration1
                        delay:0
       usingSpringWithDamping:kDamping
        initialSpringVelocity:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:animation
                   completion:completion];
}

// Sets `presentedConstraints` up, such as they are positioning the popup
// relatively to `layoutGuide`. The popup is positioned closest to the layout
// guide, by default it is presented below the layout guide, aligned on its
// leading edge. However, it is respecting the safe area bounds.
- (void)setUpPresentedConstraints {
  UIView* parentView = self.baseViewController.view;
  UIView* container = self.popupViewController.contentContainer;

  UILayoutGuide* layoutGuide = self.layoutGuide;
  CGRect guideFrame =
      [self.popupViewController.view convertRect:layoutGuide.layoutFrame
                                        fromView:layoutGuide.owningView];

  NSLayoutConstraint* verticalPositioning = nil;
  if (CGRectGetMaxY(guideFrame) + kMinHeight >
      CGRectGetHeight(parentView.frame)) {
    // Display above.
    verticalPositioning =
        [container.bottomAnchor constraintEqualToAnchor:layoutGuide.topAnchor];
  } else {
    // Display below.
    verticalPositioning =
        [container.topAnchor constraintEqualToAnchor:layoutGuide.bottomAnchor];
  }

  NSLayoutConstraint* center = [container.centerXAnchor
      constraintEqualToAnchor:layoutGuide.centerXAnchor];
  center.priority = UILayoutPriorityDefaultHigh;

  id<LayoutGuideProvider> safeArea = parentView.safeAreaLayoutGuide;
  self.presentedConstraints = @[
    center,
    verticalPositioning,
    [container.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:safeArea.leadingAnchor
                                    constant:kMinHorizontalMargin],
    [container.trailingAnchor
        constraintLessThanOrEqualToAnchor:safeArea.trailingAnchor
                                 constant:-kMinHorizontalMargin],
    [container.bottomAnchor
        constraintLessThanOrEqualToAnchor:safeArea.bottomAnchor
                                 constant:-kMinVerticalMargin],
    [container.topAnchor
        constraintGreaterThanOrEqualToAnchor:safeArea.topAnchor
                                    constant:kMinVerticalMargin],
  ];
}

// Updates the constants for the constraints constraining the presented view
// controller's height and width.
- (void)setPresentedViewControllerConstraintConstants {
  CGSize fittingSize = [self.presentedViewController.view
      sizeThatFits:CGSizeMake(kMaxWidth, kMaxHeight)];
  // Use preferredSize if it is set.
  CGSize preferredSize = self.presentedViewController.preferredContentSize;
  CGFloat width = fittingSize.width;
  CGFloat height = fittingSize.height;
  if (!CGSizeEqualToSize(preferredSize, CGSizeZero)) {
    width = preferredSize.width;
    height = preferredSize.height;
  }
  self.presentedViewControllerHeightConstraint.constant = height;
  self.presentedViewControllerWidthConstraint.constant = width;
}

#pragma mark - PopupMenuViewControllerDelegate

- (void)popupMenuViewControllerWillDismiss:
    (PopupMenuViewController*)viewController {
  [self.delegate popupMenuPresenterWillDismiss:self];
}

- (void)containedViewControllerContentSizeChangedForPopupMenuViewController:
    (PopupMenuViewController*)viewController {
  // Set the frame of the table view to the maximum width to have the label
  // resizing correctly.
  CGRect frame = self.presentedViewController.view.frame;
  frame.size.width = kMaxWidth;
  self.presentedViewController.view.frame = frame;
  // It is necessary to do a first layout pass so the table view can size
  // itself.
  [self.presentedViewController.view setNeedsLayout];
  [self.presentedViewController.view layoutIfNeeded];

  [self setPresentedViewControllerConstraintConstants];
}

@end
