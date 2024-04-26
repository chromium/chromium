// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_handler.h"

#import "base/check.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/centered_zoom_transition_animation.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/point_zoom_transition_animation.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/tab_grid_transition_animation.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/tab_grid_transition_animation_group.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_layout_providing.h"

@implementation TabGridTransitionHandler {
  TabGridTransitionType _transitionType;
  TabGridTransitionDirection _direction;

  UIViewController<TabGridTransitionLayoutProviding>* _tabGridViewController;
  UIViewController* _BVCContainerViewController;

  // Transition item for the selected cell in tab grid.
  TabGridTransitionItem* _tabGridCellItem;

  // Transition animation to execute.
  id<TabGridTransitionAnimation> _animation;
}

#pragma mark - Public

- (instancetype)initWithTransitionType:(TabGridTransitionType)transitionType
                             direction:(TabGridTransitionDirection)direction
                 tabGridViewController:
                     (UIViewController<TabGridTransitionLayoutProviding>*)
                         tabGridViewController
            bvcContainerViewController:
                (UIViewController*)bvcContainerViewController {
  self = [super init];
  if (self) {
    CHECK(tabGridViewController.transitionLayout);

    _transitionType = transitionType;
    _direction = direction;
    _tabGridViewController = tabGridViewController;
    _BVCContainerViewController = bvcContainerViewController;
    _tabGridCellItem = tabGridViewController.transitionLayout.activeCell;
  }
  return self;
}

- (void)performTransitionWithCompletion:(ProceduralBlock)completion {
  switch (_direction) {
    case TabGridTransitionDirection::kFromBrowserToTabGrid:
      [self performBrowserToTabGridTransitionWithCompletion:completion];
      break;

    case TabGridTransitionDirection::kFromTabGridToBrowser:
      [self performTabGridToBrowserTransitionWithCompletion:completion];
      break;
  }
}

#pragma mark - Private

// Performs the Browser to Tab Grid transition with a `completion` block.
- (void)performBrowserToTabGridTransitionWithCompletion:
    (ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock animationCompletion = ^{
    [weakSelf finalizeBrowserToTabGridTransition];

    if (completion) {
      completion();
    }
  };

  [self prepareBrowserToTabGridTransition];
  [self performBrowserToTabGridTransitionAnimationWithCompletion:
            animationCompletion];
}

// Performs the Tab Grid to Browser transition with a `completion` block.
- (void)performTabGridToBrowserTransitionWithCompletion:
    (ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock animationCompletion = ^{
    [weakSelf finalizeTabGridToBrowserTransition];

    if (completion) {
      completion();
    }
  };

  [self prepareTabGridToBrowserTransition];
  [self performTabGridToBrowserTransitionAnimationWithCompletion:
            animationCompletion];
}

// Prepares items for the Browser to Tab Grid transition.
- (void)prepareBrowserToTabGridTransition {
  [_BVCContainerViewController willMoveToParentViewController:nil];
}

// Prepares items for the Tab Grid to Browser transition.
- (void)prepareTabGridToBrowserTransition {
  [_tabGridViewController addChildViewController:_BVCContainerViewController];
  _BVCContainerViewController.view.frame = _tabGridViewController.view.bounds;
  [_tabGridViewController.view addSubview:_BVCContainerViewController.view];

  _BVCContainerViewController.view.accessibilityViewIsModal = YES;
}

// Performs Browser to TabGrid transition animation.
- (void)performBrowserToTabGridTransitionAnimationWithCompletion:
    (ProceduralBlock)completion {
  _animation = [self determineAnimationForBrowserToTabGridTransition];
  [self performTransitionAnimationWithCompletion:completion];
}

// Performs TabGrid to Browser transition animation.
- (void)performTabGridToBrowserTransitionAnimationWithCompletion:
    (ProceduralBlock)completion {
  _animation = [self determineAnimationForTabGridToBrowserTransition];
  [self performTransitionAnimationWithCompletion:completion];
}

// Takes all necessary actions to finish Browser to TabGrid transition.
- (void)finalizeBrowserToTabGridTransition {
  [_BVCContainerViewController.view removeFromSuperview];
  [_BVCContainerViewController removeFromParentViewController];

  [_tabGridViewController setNeedsStatusBarAppearanceUpdate];
}

// Takes all necessary actions to finish TabGrid to Browser transition.
- (void)finalizeTabGridToBrowserTransition {
  [_BVCContainerViewController
      didMoveToParentViewController:_tabGridViewController];

  [_BVCContainerViewController setNeedsStatusBarAppearanceUpdate];
}

// Performs transition animation.
- (void)performTransitionAnimationWithCompletion:(ProceduralBlock)completion {
  if (_animation) {
    [_animation animateWithCompletion:completion];

  } else if (completion) {
    completion();
  }
}

// Determines the proper animation that should be used in Browser to TabGrid
// transition.
- (id<TabGridTransitionAnimation>)
    determineAnimationForBrowserToTabGridTransition {
  switch (_transitionType) {
    case TabGridTransitionType::kNormal:
      return [self browserToTabGridNormalAnimation];
    case TabGridTransitionType::kReducedMotion:
      return [self browserToTabGridReducedMotionAnimation];
    case TabGridTransitionType::kAnimationDisabled:
      return nil;
  }
}

// Determines the proper animation that should be used in TabGrid to Browser
// transition.
- (id<TabGridTransitionAnimation>)
    determineAnimationForTabGridToBrowserTransition {
  switch (_transitionType) {
    case TabGridTransitionType::kNormal:
      return [self tabGridToBrowserNormalAnimation];
    case TabGridTransitionType::kReducedMotion:
      return [self tabGridToBrowserReducedMotionAnimation];
    case TabGridTransitionType::kAnimationDisabled:
      return nil;
  }
}

// Returns Browser to TabGrid normal motion animation.
// TODO(crbug.com/40287759): Finish this animation.
- (id<TabGridTransitionAnimation>)browserToTabGridNormalAnimation {
  // Main animation.
  PointZoomAnimationParameters animationParam = PointZoomAnimationParameters{
      .direction =
          PointZoomAnimationParameters::AnimationDirection::kContracting,
      .destinationFrame = _tabGridCellItem.originalFrame,
      .destinationCornerRadius = _tabGridCellItem.view.layer.cornerRadius};
  id<TabGridTransitionAnimation> mainAnimation =
      [[PointZoomTransitionAnimation alloc]
                 initWithView:_BVCContainerViewController.view
          animationParameters:animationParam];

  // Combine animation.
  id<TabGridTransitionAnimation> combinedIntroAndMainAnimations =
      [[TabGridTransitionAnimationGroup alloc]
          initWithAnimations:@[ mainAnimation ]];
  return combinedIntroAndMainAnimations;
}

// Returns TabGrid to Browser normal motion animation.
// TODO(crbug.com/40287759): Finish this animation.
- (id<TabGridTransitionAnimation>)tabGridToBrowserNormalAnimation {
  // Set the frame to be the same as the active cell.
  _BVCContainerViewController.view.frame = _tabGridCellItem.originalFrame;

  // Set the frame to be the same as the active cell.
  PointZoomAnimationParameters animationParam = PointZoomAnimationParameters{
      .direction = PointZoomAnimationParameters::AnimationDirection::kExpanding,
      .destinationFrame = _tabGridViewController.view.bounds,
      .destinationCornerRadius = DeviceCornerRadius()};
  id<TabGridTransitionAnimation> mainAnimation =
      [[PointZoomTransitionAnimation alloc]
                 initWithView:_BVCContainerViewController.view
          animationParameters:animationParam];

  // Combine animation.
  id<TabGridTransitionAnimation> combinedIntroAndMainAnimations =
      [[TabGridTransitionAnimationGroup alloc]
          initWithAnimations:@[ mainAnimation ]];
  return combinedIntroAndMainAnimations;
}

// Returns Browser to TabGrid reduced motion animation.
- (id<TabGridTransitionAnimation>)browserToTabGridReducedMotionAnimation {
  return [[CenteredZoomTransitionAnimation alloc]
      initWithView:_BVCContainerViewController.view
         direction:CenteredZoomTransitionAnimationDirection::kContracting];
}

// Returns TabGrid to Browser reduced motion animation.
- (id<TabGridTransitionAnimation>)tabGridToBrowserReducedMotionAnimation {
  return [[CenteredZoomTransitionAnimation alloc]
      initWithView:_BVCContainerViewController.view
         direction:CenteredZoomTransitionAnimationDirection::kExpanding];
}

@end
