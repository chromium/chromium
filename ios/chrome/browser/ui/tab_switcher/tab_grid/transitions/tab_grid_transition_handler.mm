// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_handler.h"

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/centered_zoom_transition_animation.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/animations/tab_grid_transition_animation.h"
#import "ios/chrome/common/ui/util/ui_util.h"

@implementation TabGridTransitionHandler {
  TabGridTransitionType _transitionType;
  TabGridTransitionDirection _direction;

  UIViewController* _tabGridViewController;
  BVCContainerViewController* _bvcContainerViewController;

  id<TabGridTransitionAnimation> _animation;
}

#pragma mark - Public

- (instancetype)initWithTransitionType:(TabGridTransitionType)transitionType
                             direction:(TabGridTransitionDirection)direction
                 tabGridViewController:(UIViewController*)tabGridViewController
            bvcContainerViewController:
                (BVCContainerViewController*)bvcContainerViewController {
  self = [super init];
  if (self) {
    _transitionType = transitionType;
    _direction = direction;
    _tabGridViewController = tabGridViewController;
    _bvcContainerViewController = bvcContainerViewController;
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
  [_bvcContainerViewController willMoveToParentViewController:nil];
}

// Prepares items for the Tab Grid to Browser transition.
- (void)prepareTabGridToBrowserTransition {
  [_tabGridViewController addChildViewController:_bvcContainerViewController];

  _bvcContainerViewController.view.frame = _tabGridViewController.view.bounds;
  [_tabGridViewController.view addSubview:_bvcContainerViewController.view];

  _bvcContainerViewController.view.accessibilityViewIsModal = YES;
  _bvcContainerViewController.view.alpha = 0;
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
  [_bvcContainerViewController.view removeFromSuperview];
  [_bvcContainerViewController removeFromParentViewController];

  [_tabGridViewController setNeedsStatusBarAppearanceUpdate];
}

// Takes all necessary actions to finish TabGrid to Browser transition.
- (void)finalizeTabGridToBrowserTransition {
  _bvcContainerViewController.view.alpha = 1;
  [_bvcContainerViewController
      didMoveToParentViewController:_tabGridViewController];

  [_bvcContainerViewController setNeedsStatusBarAppearanceUpdate];
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
  if (![self isTransitionLayoutValid]) {
    // Fallback to reduced motion animation.
    return [self browserToTabGridReducedMotionAnimation];
  }

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
  if (![self isTransitionLayoutValid]) {
    // Fallback to reduced motion animation.
    return [self tabGridToBrowserReducedMotionAnimation];
  }

  switch (_transitionType) {
    case TabGridTransitionType::kNormal:
      return [self tabGridToBrowserNormalAnimation];
    case TabGridTransitionType::kReducedMotion:
      return [self tabGridToBrowserReducedMotionAnimation];
    case TabGridTransitionType::kAnimationDisabled:
      return nil;
  }
}

// Checks if the validity of a transition layout.
- (BOOL)isTransitionLayoutValid {
  // TODO: Update this placeholder with an actual implementation.
  return NO;
}

// Returns Browser to TabGrid normal motion animation.
- (id<TabGridTransitionAnimation>)browserToTabGridNormalAnimation {
  return nil;
}

// Returns TabGrid to Browser normal motion animation.
- (id<TabGridTransitionAnimation>)tabGridToBrowserNormalAnimation {
  return nil;
}

// Returns Browser to TabGrid reduced motion animation.
- (id<TabGridTransitionAnimation>)browserToTabGridReducedMotionAnimation {
  return [[CenteredZoomTransitionAnimation alloc]
      initWithView:_bvcContainerViewController.view
         direction:CenteredZoomTransitionAnimationDirection::kContracting];
}

// Returns TabGrid to Browser reduced motion animation.
- (id<TabGridTransitionAnimation>)tabGridToBrowserReducedMotionAnimation {
  return [[CenteredZoomTransitionAnimation alloc]
      initWithView:_bvcContainerViewController.view
         direction:CenteredZoomTransitionAnimationDirection::kExpanding];
}

@end
