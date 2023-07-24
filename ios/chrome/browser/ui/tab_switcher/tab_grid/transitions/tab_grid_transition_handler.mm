// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/transitions/tab_grid_transition_handler.h"

#import "base/ios/block_types.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridTransitionHandler {
  TabGridTransitionType _transitionType;
  TabGridTransitionDirection _direction;

  UIViewController* _tabGridViewController;
  BVCContainerViewController* _bvcContainerViewController;
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
  [self prepareBrowserToTabGridTransition];
}

// Performs the Tab Grid to Browser transition with a `completion` block.
- (void)performTabGridToBrowserTransitionWithCompletion:
    (ProceduralBlock)completion {
  [self prepareTabGridToBrowserTransition];
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

@end
