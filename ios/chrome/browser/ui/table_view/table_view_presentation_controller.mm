// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"

#include <algorithm>

#import "ios/chrome/browser/ui/image_util/image_util.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller_delegate.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The rounded corner radius for the bubble container, in Regular widths.
const CGFloat kContainerCornerRadius = 13.0;

// The size of the margin around |tableViewContainer| in which the shadow is
// drawn.
const CGFloat kShadowMargin = 196.0;

// The minimum margin between |tableViewContainer| and any screen edge.
const CGFloat kTableViewEdgeMargin = 15.0;

// The top margin for |tableViewContainer|.
const CGFloat kTableViewTopMargin = 35.0;

// The maximum allowed width for |tableViewContainer|.
const CGFloat kTableViewMaxWidth = 414.0;

}  // namespace

@interface TableViewPresentationController ()

// A view which prevents touches from reaching views below this controller's
// |containerView|.  This view is normally clear and dismisses the presented
// view controller when tapped, but can optionally act as a dimming view and
// ignore touches.
@property(nonatomic, readwrite, strong) UIView* dimmingShield;

// A container view for |tableViewContainer| and |shadowImage|.
@property(nonatomic, readwrite, strong) UIView* shadowContainer;

// Draws a shadow to visually separate the contents of |tableViewContainer| from
// the views below.
@property(nonatomic, readwrite, strong) UIImageView* shadowImage;

// Acts as a container for the presented view controller's view.
@property(nonatomic, readwrite, strong) UIView* tableViewContainer;

// Cleans up and removes any views that are managed by this controller.
- (void)cleanUpPresentationContainerViews;

@end

@implementation TableViewPresentationController
@synthesize dimmingShield = _dimmingShield;
@synthesize modalDelegate = _modalDelegate;
@synthesize shadowContainer = _shadowContainer;
@synthesize shadowImage = _shadowImage;
@synthesize tableViewContainer = _tableViewContainer;

- (CGRect)frameOfPresentedViewInContainerView {
  CGRect safeAreaBounds = self.containerView.safeAreaLayoutGuide.layoutFrame;
  UIEdgeInsets safeAreaInsets = self.containerView.safeAreaInsets;

  CGFloat safeAreaWidth = CGRectGetWidth(safeAreaBounds);
  CGFloat safeAreaHeight = CGRectGetHeight(safeAreaBounds);

  CGFloat maxAvailableWidth = safeAreaWidth - 2 * kTableViewEdgeMargin;
  CGFloat tableWidth = std::min(maxAvailableWidth, kTableViewMaxWidth);

  // The space between the bubble and the edge of the screen is equal to the
  // width of the safe area, on the position edge, plus the table view edge
  // margin. When |self.position| is TablePresentationPositionLeading, the
  // leading edge of the bubble is equal to that spacing. And if |self.position|
  // is TablePresentationPositionTrailing, the leading edge of the margin is
  // equal to the containerView's bounds minus that spacing minus the width of
  // the table view itself.
  CGFloat tableLeadingX;
  if (self.position == TablePresentationPositionTrailing) {
    tableLeadingX = CGRectGetWidth(self.containerView.bounds) -
                    UIEdgeInsetsGetTrailing(safeAreaInsets) -
                    kTableViewEdgeMargin - tableWidth;
  } else {
    tableLeadingX =
        UIEdgeInsetsGetLeading(safeAreaInsets) + kTableViewEdgeMargin;
  }
  CGFloat containerWidth = CGRectGetWidth(self.containerView.bounds);
  CGFloat tableOriginY = CGRectGetMinY(safeAreaBounds) + kTableViewTopMargin;
  CGFloat tableHeight =
      safeAreaHeight - kTableViewTopMargin - kTableViewEdgeMargin;

  // The tableview container should be pinned to the top, bottom, and either
  // trailing or leading edges of the safe area, depending on |self.position|.
  // It will also have a fixed margin on those sides.
  LayoutRect tableLayoutRect = LayoutRectMake(
      tableLeadingX, containerWidth, tableOriginY, tableWidth, tableHeight);
  return LayoutRectGetRect(tableLayoutRect);
}

- (UIView*)presentedView {
  return self.shadowContainer;
}

- (void)presentationTransitionWillBegin {
  // The dimming view is added first, so that all other views are layered on top
  // of it.
  BOOL shieldIsModal =
      self.modalDelegate &&
      ![self.modalDelegate
          presentationControllerShouldDismissOnTouchOutside:self];
  self.dimmingShield = [[UIView alloc] init];
  self.dimmingShield.backgroundColor = [UIColor clearColor];
  self.dimmingShield.frame = self.containerView.bounds;
  [self.containerView addSubview:self.dimmingShield];
  [self.dimmingShield
      addGestureRecognizer:[[UITapGestureRecognizer alloc]
                               initWithTarget:self
                                       action:@selector(handleShieldTap)]];

  self.shadowContainer = [[UIView alloc] init];
  self.shadowImage =
      [[UIImageView alloc] initWithImage:StretchableImageNamed(@"menu_shadow")];
  self.shadowImage.translatesAutoresizingMaskIntoConstraints = NO;
  self.shadowImage.alpha = 0.0;
  [self.shadowContainer addSubview:self.shadowImage];
  AddSameConstraintsToSidesWithInsets(
      self.shadowContainer, self.shadowImage,
      LayoutSides::kTop | LayoutSides::kBottom | LayoutSides::kLeading |
          LayoutSides::kTrailing,
      ChromeDirectionalEdgeInsetsMake(kShadowMargin, kShadowMargin,
                                      kShadowMargin, kShadowMargin));

  self.tableViewContainer = [[UIView alloc] init];
  self.tableViewContainer.backgroundColor =
      [UIColor colorWithHue:0 saturation:0 brightness:0.98 alpha:1.0];
  [self.tableViewContainer addSubview:self.presentedViewController.view];

  self.tableViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  self.tableViewContainer.layer.cornerRadius = kContainerCornerRadius;
  self.tableViewContainer.layer.masksToBounds = YES;
  self.tableViewContainer.clipsToBounds = YES;

  [self.shadowContainer addSubview:self.tableViewContainer];
  AddSameConstraints(self.shadowContainer, self.tableViewContainer);
  [self.containerView addSubview:self.shadowContainer];

  self.shadowContainer.frame = [self frameOfPresentedViewInContainerView];
  self.presentedViewController.view.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  self.presentedViewController.view.frame = self.tableViewContainer.bounds;

  // Start the shadowImage with an alpha of 0.0 and animate it alongside the
  // presentation transition. If the animation could not be queued, then simply
  // leave the shadowImage's alpha as 1.0.
  BOOL animationQueued = [self.presentedViewController.transitionCoordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
        [self updateDimmingShieldForModal:shieldIsModal];
      }
                      completion:nil];
  if (!animationQueued) {
    [self updateDimmingShieldForModal:shieldIsModal];
  }
}

- (void)presentationTransitionDidEnd:(BOOL)completed {
  if (!completed) {
    [self cleanUpPresentationContainerViews];
  }
}

- (void)dismissalTransitionWillBegin {
  [self.presentedViewController.transitionCoordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
        self.shadowImage.alpha = 0.0;
        self.dimmingShield.backgroundColor = [UIColor clearColor];
      }
                      completion:nil];
}

- (void)dismissalTransitionDidEnd:(BOOL)completed {
  if (completed) {
    [self cleanUpPresentationContainerViews];
  }
}

- (void)containerViewWillLayoutSubviews {
  self.dimmingShield.frame = self.containerView.bounds;
  self.shadowContainer.frame = [self frameOfPresentedViewInContainerView];

  // The TableView will be presented modally, make sure the container A11y is
  // marked as so in order to prevent voice over focusing the presenter VC
  // instead of the presented VC (TableView).
  self.containerView.accessibilityViewIsModal = YES;

  // Force the presented VC's view to fill the tableViewContainer.  Otherwise
  // there are cases (switching size classes while another VC is presented over
  // the tableView) where autoresizing does not properly size the presented VC's
  // view to fill its parent.
  self.presentedViewController.view.frame = self.tableViewContainer.bounds;
}

#pragma mark - TableViewModalPresenting

- (void)setShouldDismissOnTouchOutside:(BOOL)shouldDismiss
             withTransitionCoordinator:
                 (id<UIViewControllerTransitionCoordinator>)
                     transitionCoordinator {
  if (transitionCoordinator) {
    auto animation =
        ^(id<UIViewControllerTransitionCoordinatorContext> context) {
          [self updateDimmingShieldForModal:!shouldDismiss];
        };
    [transitionCoordinator animateAlongsideTransitionInView:self.containerView
                                                  animation:animation
                                                 completion:nil];
  } else {
    [self updateDimmingShieldForModal:!shouldDismiss];
  }
}

#pragma mark - Private Methods

- (void)cleanUpPresentationContainerViews {
  [self.tableViewContainer removeFromSuperview];
  self.tableViewContainer = nil;
  [self.shadowImage removeFromSuperview];
  self.shadowImage = nil;
  [self.shadowContainer removeFromSuperview];
  self.shadowContainer = nil;
  [self.dimmingShield removeFromSuperview];
  self.dimmingShield = nil;
}

// Updates |self.shadowImage.alpha| and |self.dimmingShield.backgroundColor| as
// appropriate for the given |modal| mode.  This method will animate the changes
// if it is called from within an animation block.
- (void)updateDimmingShieldForModal:(BOOL)modal {
  if (modal) {
    self.dimmingShield.backgroundColor = [UIColor colorWithWhite:0.0 alpha:0.4];
    self.shadowImage.alpha = 0.0;
  } else {
    self.dimmingShield.backgroundColor = [UIColor clearColor];
    self.shadowImage.alpha = 1.0;
  }
}

#pragma mark - Actions

- (void)handleShieldTap {
  if (self.modalDelegate) {
    // If a delegate is set, ask it for permission to dismiss, then ask it to
    // handle the dismissal.
    if ([self.modalDelegate
            presentationControllerShouldDismissOnTouchOutside:self]) {
      [self.modalDelegate presentationControllerWillDismiss:self];
    }
  } else {
    // If no delegate is set, handle the dismissal directly.
    [self.presentedViewController dismissViewControllerAnimated:YES
                                                     completion:nil];
  }
}

#pragma mark - Adaptivity

- (UIModalPresentationStyle)adaptivePresentationStyleForTraitCollection:
    (UITraitCollection*)traitCollection {
  if (traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassCompact &&
      traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact) {
    return UIModalPresentationFullScreen;
  }

  return UIModalPresentationNone;
}

@end
