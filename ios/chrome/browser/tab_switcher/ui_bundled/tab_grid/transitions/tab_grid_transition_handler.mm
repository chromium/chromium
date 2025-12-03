// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_handler.h"

#import "base/check.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/grid_to_tab_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_parameters.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_reduced_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_transition_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_to_grid_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_layout.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_layout_providing.h"

@implementation TabGridTransitionHandler {
  TabGridTransitionType _transitionType;
  TabGridTransitionDirection _direction;

  UIViewController* _tabGridViewController;
  UIViewController* _BVCContainerViewController;

  // Transition layout provider for the tab grid.
  id<TabGridTransitionLayoutProviding> _tabGridTransitionLayoutProvider;

  // Transition item for the selected cell in tab grid.
  TabGridTransitionItem* _tabGridCellItem;

  // The view controller of the currently active tab grid.
  UIViewController* _activeGrid;

  // The view controller of the pinned tabs.
  UIViewController* _pinnedTabsViewController;

  // Whether the active cell if from a pinned tab.
  BOOL _activeCellPinned;

  // The tab grid transition animation to be performed.
  id<TabGridTransitionAnimation> _animation;

  // The layout guide center associated to the current browser.
  LayoutGuideCenter* _layoutGuideCenter;

  // Whether the transition is for a tab that is a regular (non-icongnito) NTP.
  BOOL _isRegularBrowserNTP;

  // Whether the transition is for an incognito tab.
  BOOL _incognito;
}

#pragma mark - Public

- (instancetype)initWithTransitionType:(TabGridTransitionType)transitionType
                             direction:(TabGridTransitionDirection)direction
       tabGridTransitionLayoutProvider:
           (id<TabGridTransitionLayoutProviding>)tabGridTransitionLayoutProvider
                 tabGridViewController:(UIViewController*)tabGridViewController
            bvcContainerViewController:
                (UIViewController*)bvcContainerViewController
                     layoutGuideCenter:(LayoutGuideCenter*)layoutGuideCenter
                   isRegularBrowserNTP:(BOOL)isRegularBrowserNTP
                             incognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    TabGridTransitionLayout* transitionLayout = [tabGridTransitionLayoutProvider
        transitionLayoutForIsIncognito:incognito];
    _transitionType = transitionType;
    _direction = direction;
    _tabGridTransitionLayoutProvider = tabGridTransitionLayoutProvider;
    _tabGridViewController = tabGridViewController;
    _BVCContainerViewController = bvcContainerViewController;
    _tabGridCellItem = transitionLayout.activeCell;
    _activeGrid = transitionLayout.activeGrid;
    _pinnedTabsViewController = transitionLayout.pinnedTabs;
    _activeCellPinned = transitionLayout.isActiveCellPinned;
    _layoutGuideCenter = layoutGuideCenter;
    _isRegularBrowserNTP = isRegularBrowserNTP;
    _incognito = incognito;
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
  [self performTransitionAnimationWithCompletion:animationCompletion];
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
  [self performTransitionAnimationWithCompletion:animationCompletion];
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
  // The animation is ugly or crashes when the selected cell is not visible.
  TabGridTransitionType transitionType = _transitionType;
  if (transitionType == TabGridTransitionType::kNormal &&
      !_tabGridTransitionLayoutProvider.isSelectedCellVisible) {
    transitionType = TabGridTransitionType::kReducedMotion;
  }

  switch (transitionType) {
    case TabGridTransitionType::kNormal: {
      TabGridAnimationParameters* animationParameters =
          [self createAnimationParameters];

      switch (_direction) {
        case TabGridTransitionDirection::kFromTabGridToBrowser: {
          _animation = [[GridToTabAnimation alloc]
              initWithAnimationParameters:animationParameters];
          break;
        }

        case TabGridTransitionDirection::kFromBrowserToTabGrid: {
          _animation = [[TabToGridAnimation alloc]
              initWithAnimationParameters:animationParameters];
          break;
        }
      }

      break;
    }

    case TabGridTransitionType::kReducedMotion: {
      _animation = [[TabGridReducedAnimation alloc]
          initWithAnimatedView:_BVCContainerViewController.view
                beingPresented:_direction == TabGridTransitionDirection::
                                                 kFromTabGridToBrowser];
      break;
    }

    case TabGridTransitionType::kAnimationDisabled: {
      break;
    }
  }

  if (_animation) {
    [_animation animateWithCompletion:completion];
  } else if (completion) {
    completion();
  }
}

// Creates animation parameters for the transition.
- (TabGridAnimationParameters*)createAnimationParameters {
  // Get the "top toolbar height" (everything above the web content area) by
  // converting the top toolbar's frame into window coordinates and getting its
  // max Y coordinate (this also includes the tab strip for iPads).
  UIView* primaryToolbarView =
      [_layoutGuideCenter referencedViewUnderName:kPrimaryToolbarGuide];
  CGRect primaryToolbarFrameInWindow =
      [primaryToolbarView.superview convertRect:primaryToolbarView.frame
                                         toView:nil];

  BOOL topToolbarHidden = [self shouldHideTopToolbar];
  CGFloat topToolbarHeight =
      topToolbarHidden ? _tabGridViewController.view.safeAreaInsets.top
                       : CGRectGetMaxY(primaryToolbarFrameInWindow);

  // Get the "bottom toolbar height" (everything below the web content area).
  UIView* bottomToolbarView =
      [_layoutGuideCenter referencedViewUnderName:kSecondaryToolbarGuide];
  CGRect bottomToolbarFrameInWindow =
      [bottomToolbarView.superview convertRect:bottomToolbarView.frame
                                        toView:nil];
  CGFloat bottomToolbarHeight = bottomToolbarView.window.bounds.size.height -
                                CGRectGetMinY(bottomToolbarFrameInWindow);

  // Whether the top toolbar should be scaled during the transition,
  // disabled for regular browser NTPs and iPads.
  BOOL scaleTopToolbar =
      !_isRegularBrowserNTP && IsSplitToolbarMode(_activeGrid);

  // Get the content area frame.
  UIView* tabContentView = [self tabContentView];
  CGRect contentAreaFrame = [NamedGuide guideWithName:kContentAreaGuide
                                                 view:tabContentView]
                                .layoutFrame;

  // No top toolbar snapshot for regular browser NTPs for grid to tab
  // animations. `topToolbarHidden` is not directly used here as the screenshot
  // of the content below the status bar is needed when doing a Tab to Grid
  // transition.
  UIView* topToolbarSnapshotView =
      topToolbarHidden &&
              _direction == TabGridTransitionDirection::kFromTabGridToBrowser
          ? nil
          : [self snapshotOfViewPortionAboveRect:tabContentView
                                      middleRect:contentAreaFrame];

  // Get the animation's destination and origin frames.
  CGRect destinationFrame =
      _direction == TabGridTransitionDirection::kFromBrowserToTabGrid
          ? _tabGridCellItem.originalFrame
          : _BVCContainerViewController.view.frame;

  CGRect originFrame =
      _direction == TabGridTransitionDirection::kFromBrowserToTabGrid
          ? _BVCContainerViewController.view.frame
          : _tabGridCellItem.originalFrame;

  CHECK(_tabGridCellItem);

  return [[TabGridAnimationParameters alloc]
       initWithDestinationFrame:destinationFrame
                    originFrame:originFrame
                     activeGrid:_activeGrid
                     pinnedTabs:_pinnedTabsViewController
               activeCellPinned:_activeCellPinned
                   animatedView:_BVCContainerViewController.view
                contentSnapshot:_tabGridCellItem.snapshot
               topToolbarHeight:topToolbarHeight
            bottomToolbarHeight:bottomToolbarHeight
         topToolbarSnapshotView:topToolbarSnapshotView
      bottomToolbarSnapshotView:
          [self snapshotOfViewPortionBelowRect:tabContentView
                                    middleRect:contentAreaFrame]
          shouldScaleTopToolbar:scaleTopToolbar
                      incognito:_incognito
               topToolbarHidden:topToolbarHidden];
}

// Returns the frame for the snapshotted content of the active tab.
// Conceptually the transition is dismissing/presenting a tab (a BVC).
// However, currently the BVC instances are themselves contained within a
// BVCContainer view controller. This means that the
// `viewControllerForTab.view` is not the BVC's view; rather it's the view of
// the view controller that contains the BVC. Unfortunately, the layout guide
// needed here is attached to the BVC's view, which is the first (and only)
// subview of the BVCContainerViewController's view.
// TODO(crbug.com/40583629) Clean up this arrangement.
- (UIView*)tabContentView {
  return _BVCContainerViewController.view.subviews[0];
}

// Returns a snapshot of the portion of the view that is above the given rect.
- (UIView*)snapshotOfViewPortionAboveRect:(UIView*)view
                               middleRect:(CGRect)rect {
  if (view != nil && rect.origin.y > 0) {
    // `topRect` starts from the origin of the view, and ends at the top of
    // `rect`.
    CGRect topRect = CGRectMake(0, 0, view.bounds.size.width, rect.origin.y);
    return [view resizableSnapshotViewFromRect:topRect
                            afterScreenUpdates:YES
                                 withCapInsets:UIEdgeInsetsZero];
  }

  return nil;
}

// Returns a snapshot of the portion of the view that is below the given rect.
- (UIView*)snapshotOfViewPortionBelowRect:(UIView*)view
                               middleRect:(CGRect)rect {
  CGSize viewSize = view.bounds.size;
  CGFloat middleRectBottom = CGRectGetMaxY(rect);
  CGFloat bottomHeight = viewSize.height - middleRectBottom;

  if (bottomHeight > 0) {
    // `bottomRect` start at the bottom of `rect` and ends at the bottom of
    // `view`.
    CGRect bottomRect =
        CGRectMake(0, middleRectBottom, viewSize.width, bottomHeight);
    return [view resizableSnapshotViewFromRect:bottomRect
                            afterScreenUpdates:YES
                                 withCapInsets:UIEdgeInsetsZero];
  }

  return nil;
}

// Returns YES if the transition should hide the top toolbar (use the safe area
// insets instead of the top toolbar LayoutGuide).
- (BOOL)shouldHideTopToolbar {
  return _isRegularBrowserNTP && !CanShowTabStrip(_tabGridViewController) &&
         IsSplitToolbarMode(_tabGridViewController);
}

@end
