// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_navigation_controller.h"

#import <algorithm>

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_constants.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_screen/account_picker_screen_view_controller.h"
#import "ios/chrome/common/ui/util/background_util.h"

namespace {

// Corner radius for centered style dialog.
constexpr CGFloat kCornerRadius = 12.;

}  // namespace

@interface AccountPickerScreenNavigationController ()

// View to get transparent blurred background.
@property(nonatomic, strong, readwrite) UIView* backgroundView;
@property(nonatomic, strong, readwrite)
    UIPercentDrivenInteractiveTransition* interactionTransition;

@end

@implementation AccountPickerScreenNavigationController

- (CGSize)layoutFittingSizeForWidth:(CGFloat)width {
  UINavigationController* navigationController =
      self.childViewControllers.lastObject;
  DCHECK([navigationController
      conformsToProtocol:@protocol(AccountPickerScreenViewController)]);
  UIViewController<AccountPickerScreenViewController>*
      childNavigationController =
          static_cast<UIViewController<AccountPickerScreenViewController>*>(
              navigationController);

  // If the child controller updates its view due to an external action such
  // as adding or removing an identity then force a layout to ensure the child
  // height is up-to-date.
  [childNavigationController.view setNeedsLayout];
  [childNavigationController.view layoutIfNeeded];

  CGFloat height =
      [childNavigationController layoutFittingHeightForWidth:width];
  CGFloat maxViewHeight =
      self.view.window.frame.size.height * kMaxPickAccountHeightRatioWithWindow;
  return CGSizeMake(width, std::min(height, maxViewHeight));
}

- (void)didUpdateControllerViewFrame {
  self.backgroundView.frame = self.view.bounds;
}

- (void)preferredContentSizeDidChangeForChildContentContainer:
    (id<UIContentContainer>)container {
  [super preferredContentSizeDidChangeForChildContentContainer:container];
  [self.layoutDelegate
          preferredContentSizeDidChangeForAccountPickerScreenViewController];
}

#pragma mark - AccountPickerLayoutDelegate

- (AccountPickerSheetDisplayStyle)displayStyle {
  return [self displayStyleWithTraitCollection:self.traitCollection];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.backgroundView = PrimaryBackgroundBlurView();
  [self.view insertSubview:self.backgroundView atIndex:0];
  self.backgroundView.frame = self.view.bounds;
  self.view.layer.masksToBounds = YES;
  self.view.clipsToBounds = YES;
  self.view.accessibilityIdentifier =
      kAccountPickerScreenNavigationControllerAccessibilityIdentifier;
  UIScreenEdgePanGestureRecognizer* edgeSwipeGesture =
      [[UIScreenEdgePanGestureRecognizer alloc]
          initWithTarget:self
                  action:@selector(swipeAction:)];
  edgeSwipeGesture.edges = UIRectEdgeLeft;
  [self.view addGestureRecognizer:edgeSwipeGesture];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.navigationBar setBackgroundImage:[[UIImage alloc] init]
                           forBarMetrics:UIBarMetricsDefault];
  [self updateViewWithTraitCollection:self.traitCollection];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self didUpdateControllerViewFrame];
}

#pragma mark - UINavigationController

- (void)pushViewController:(UIViewController*)viewController
                  animated:(BOOL)animated {
  DCHECK([viewController
      conformsToProtocol:@protocol(AccountPickerScreenViewController)]);
  [super pushViewController:viewController animated:animated];
}

#pragma mark - UIContentContainer

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  [self updateViewWithTraitCollection:newCollection];
}

#pragma mark - SwipeGesture

// Called when the swipe gesture is active. This method controls the sliding
// between two view controls in `self`.
- (void)swipeAction:(UIScreenEdgePanGestureRecognizer*)gestureRecognizer {
  if (!gestureRecognizer.view) {
    self.interactionTransition = nil;
    return;
  }
  UIView* view = gestureRecognizer.view;
  CGFloat percentage =
      [gestureRecognizer translationInView:view].x / view.bounds.size.width;
  switch (gestureRecognizer.state) {
    case UIGestureRecognizerStateBegan:
      self.interactionTransition =
          [[UIPercentDrivenInteractiveTransition alloc] init];
      [self popViewControllerAnimated:YES];
      [self.interactionTransition updateInteractiveTransition:percentage];
      break;
    case UIGestureRecognizerStateChanged:
      [self.interactionTransition updateInteractiveTransition:percentage];
      break;
    case UIGestureRecognizerStateEnded:
      if (percentage > .5 &&
          gestureRecognizer.state != UIGestureRecognizerStateCancelled) {
        [self.interactionTransition finishInteractiveTransition];
      } else {
        [self.interactionTransition cancelInteractiveTransition];
      }
      self.interactionTransition = nil;
      break;
    case UIGestureRecognizerStatePossible:
    case UIGestureRecognizerStateCancelled:
    case UIGestureRecognizerStateFailed:
      break;
  }
}

#pragma mark - Private

// Updates the view according to the trait collection.
- (void)updateViewWithTraitCollection:(UITraitCollection*)collection {
  switch ([self displayStyleWithTraitCollection:collection]) {
    case AccountPickerSheetDisplayStyle::kBottom:
      self.view.layer.cornerRadius = 0;
      break;
    case AccountPickerSheetDisplayStyle::kCentered:
      self.view.layer.cornerRadius = kCornerRadius;
      break;
  }
}

// Returns the display style based on the trait collection.
- (AccountPickerSheetDisplayStyle)displayStyleWithTraitCollection:
    (UITraitCollection*)collection {
  // If one trait dimension is compact, the returned style is bottom.
  // Otherwise, the returned style is centered.
  BOOL hasAtLeastOneCompactSize =
      (collection.horizontalSizeClass == UIUserInterfaceSizeClassCompact) ||
      (collection.verticalSizeClass == UIUserInterfaceSizeClassCompact);
  return hasAtLeastOneCompactSize ? AccountPickerSheetDisplayStyle::kBottom
                                  : AccountPickerSheetDisplayStyle::kCentered;
}

@end
