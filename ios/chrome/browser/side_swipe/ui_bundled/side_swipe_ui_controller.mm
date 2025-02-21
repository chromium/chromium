// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller.h"

#import "base/ios/block_types.h"
#import "base/notreached.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mutator.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_delegate.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_snapshot_navigation_view.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_ui_controller_delegate.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_util.h"

@implementation SideSwipeUIController {
  // Side swipe view for page navigation.
  UIView<HorizontalPanGestureHandler>* _pageSideSwipeView;
}

- (void)animateSwipe:(SwipeType)swipeType
         inDirection:(UISwipeGestureRecognizerDirection)direction {
  if (self.mutator.inSwipe ||
      [_sideSwipeUIControllerDelegate preventSideSwipe]) {
    return;
  }
  switch (swipeType) {
    case SwipeType::NONE:
    case SwipeType::CHANGE_TAB:
      NOTREACHED();
    case SwipeType::CHANGE_PAGE:
      [self animatePageNavigationInDirection:direction];
      break;
  }
}

#pragma mark - private

// Animate page navigation.
- (void)animatePageNavigationInDirection:
    (UISwipeGestureRecognizerDirection)direction {
  [_mutator setInSwipe:YES];

  [_sideSwipeUIControllerDelegate
      updateAccessoryViewsForSideSwipeWithVisibility:NO];

  UIView* navigatingView =
      [_sideSwipeUIControllerDelegate sideSwipeContentView].superview;
  CGRect navigatingBounds = navigatingView.bounds;
  CGFloat headerHeight =
      [_sideSwipeUIControllerDelegate headerHeightForSideSwipe];
  CGRect navigationFrame =
      CGRectMake(CGRectGetMinX(navigatingBounds),
                 CGRectGetMinY(navigatingBounds) + headerHeight,
                 CGRectGetWidth(navigatingBounds),
                 CGRectGetHeight(navigatingBounds) - headerHeight);

  BOOL canNavigate = [self.navigationDelegate
      canNavigateInDirection:IsSwipingBack(direction)
                                 ? NavigationDirectionBack
                                 : NavigationDirectionForward];

  if ([self swipingFullScreenContent:direction]) {
    _pageSideSwipeView = [self
        fullscreenSnapshotSideSwipeView:direction
                          snapshotImage:[self.navigationDelegate
                                            swipeNavigationSnapshotForDirection:
                                                direction]];
    UIView* fullscreenView =
        [_sideSwipeUIControllerDelegate sideSwipeFullscreenView];
    [_pageSideSwipeView setTargetView:fullscreenView];
    if (_pageSideSwipeView) {
      [fullscreenView.superview insertSubview:_pageSideSwipeView
                                 belowSubview:fullscreenView];
    }
  } else {
    _pageSideSwipeView = [self webContentSideSwipeView:navigationFrame
                                             direction:direction
                                           canNavigate:canNavigate];
    [_pageSideSwipeView
        setTargetView:[_sideSwipeUIControllerDelegate sideSwipeContentView]];
    [navigatingView
        insertSubview:_pageSideSwipeView
         belowSubview:[_sideSwipeUIControllerDelegate topToolbarView]];
  }

  __weak SideSwipeUIController* weakSelf = self;
  [_pageSideSwipeView
      animateHorizontalPanWithDirection:direction
                      completionHandler:^{
                        if (canNavigate) {
                          [weakSelf.mutator
                              handleOverThresholdCompletion:direction];
                        } else {
                          [weakSelf.mutator handleUnderThresholdCompletion];
                        }
                      }];
}

// Creates and returns a view, showing a navigation arrow covering the web
// content area.
- (SideSwipeNavigationView*)
    webContentSideSwipeView:(CGRect)frame
                  direction:(UISwipeGestureRecognizerDirection)direction
                canNavigate:(BOOL)canNavigate {
  return [[SideSwipeNavigationView alloc]
      initWithFrame:frame
      withDirection:direction
        canNavigate:canNavigate
              image:[UIImage imageNamed:@"side_swipe_navigation_back"]];
}

// Returns YES, if the the whole page should be swiped.
- (BOOL)swipingFullScreenContent:(UISwipeGestureRecognizerDirection)direction {
  return [self.navigationDelegate isSwipingToAnOverlay:direction];
}

// Creates and returns a view, showing a `snapshotImage` on fullscreen.
- (SideSwipeSnapshotNavigationView*)
    fullscreenSnapshotSideSwipeView:(UISwipeGestureRecognizerDirection)direction
                      snapshotImage:(UIImage*)snapshotImage {
  if (!snapshotImage) {
    return nil;
  }

  return [[SideSwipeSnapshotNavigationView alloc]
      initWithFrame:[[_sideSwipeUIControllerDelegate sideSwipeFullscreenView]
                        frame]
           snapshot:snapshotImage];
}

@end
