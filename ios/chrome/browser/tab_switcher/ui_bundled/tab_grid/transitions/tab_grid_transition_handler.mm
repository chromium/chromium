// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_handler.h"

#import "base/check.h"
#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/grid_to_tab_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_animation_parameters.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_reduced_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_grid_transition_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/animations/tab_to_grid_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_context_provider.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_layout.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/tab_grid_transition_layout_providing.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Transition types available.
enum class TabGridTransitionType {
  kNormal,
  kReducedMotion,
  kDisabledAnimation,
};
}  // namespace

@implementation TabGridTransitionHandler {
  TabGridTransitionType _transitionType;

  // The common parameters for all transitions.
  std::unique_ptr<TabGridTransitionHandlerInitParams> _params;

  // Transition layout provider for the tab grid.
  __weak id<TabGridTransitionLayoutProviding> _tabGridTransitionLayoutProvider;

  // Transition item for the selected cell in tab grid.
  TabGridTransitionItem* _tabGridCellItem;

  // The view controller of the currently active tab grid.
  UIViewController* _activeGrid;

  // The view controller of the pinned tabs.
  UIViewController* _pinnedTabsViewController;

  // Whether the active cell if from a pinned tab.
  BOOL _activeCellPinned;

  // The layout guide center associated to the current browser.
  LayoutGuideCenter* _browserLayoutGuideCenter;

  // Whether the transition is for a tab that is a regular (non-icongnito) NTP.
  BOOL _isRegularBrowserNTP;

  // Whether the transition is for an incognito tab.
  BOOL _incognito;

  // The top and bottom toolbar snapshot views.
  UIView* _topToolbarSnapshotView;
  UIView* _bottomToolbarSnapshotView;
}

#pragma mark - Public

- (instancetype)initWithCommonParams:
                    (std::unique_ptr<TabGridTransitionHandlerInitParams>)params
     tabGridTransitionLayoutProvider:
         (id<TabGridTransitionLayoutProviding>)tabGridTransitionLayoutProvider
            browserLayoutGuideCenter:
                (LayoutGuideCenter*)browserLayoutGuideCenter
                 isRegularBrowserNTP:(BOOL)isRegularBrowserNTP
                           incognito:(BOOL)incognito {
  self = [super init];
  if (self) {
    _transitionType = TabGridTransitionType::kNormal;
    _params = std::move(params);

    // Full animation setup
    TabGridTransitionLayout* transitionLayout = [tabGridTransitionLayoutProvider
        transitionLayoutForIsIncognito:incognito];
    _tabGridTransitionLayoutProvider = tabGridTransitionLayoutProvider;
    _tabGridCellItem = transitionLayout.activeCell;
    _activeGrid = transitionLayout.activeGrid;
    _pinnedTabsViewController = transitionLayout.pinnedTabs;
    _activeCellPinned = transitionLayout.isActiveCellPinned;
    _browserLayoutGuideCenter = browserLayoutGuideCenter;
    _isRegularBrowserNTP = isRegularBrowserNTP;
    _incognito = incognito;
  }
  return self;
}

- (instancetype)initWithReducedMotionCommonParams:
    (std::unique_ptr<TabGridTransitionHandlerInitParams>)params {
  self = [super init];
  if (self) {
    _transitionType = TabGridTransitionType::kReducedMotion;
    _params = std::move(params);
  }
  return self;
}

- (instancetype)initWithNoAnimationCommonParams:
    (std::unique_ptr<TabGridTransitionHandlerInitParams>)params {
  self = [super init];
  if (self) {
    _transitionType = TabGridTransitionType::kDisabledAnimation;
    _params = std::move(params);
  }
  return self;
}

- (void)performTransitionWithCompletion:(ProceduralBlock)completion {
  CHECK(_params);
  switch (_params->direction) {
    case TabGridTransitionDirection::kFromBrowserToTabGrid:
      [self performBrowserToTabGridTransitionWithCompletion:completion];
      break;

    case TabGridTransitionDirection::kFromTabGridToBrowser:
      [self performTabGridToBrowserTransitionWithCompletion:completion];
      break;
  }
}

#pragma mark - Private

// Takes snapshots of the top and bottom toolbars, for normal transitions.
- (void)takeToolbarSnapshots {
  if (_transitionType != TabGridTransitionType::kNormal) {
    return;
  }

  CGRect contentAreaFrame = [self contentAreaFrame];

  // No top toolbar snapshot for regular browser NTPs in grid to tab
  // animations. `shouldHideTopToolbar` is not directly used here as the
  // screenshot of the content below the status bar is needed when doing a Tab
  // to Grid transition.
  BOOL shouldSkipTopToolbarSnapshot =
      [self shouldHideTopToolbar] &&
      _params->direction == TabGridTransitionDirection::kFromTabGridToBrowser;

  UIViewController* browserLayout = _params->browser_layout_view_controller;
  if (!shouldSkipTopToolbarSnapshot) {
    _topToolbarSnapshotView =
        [self snapshotOfViewPortionAboveRect:browserLayout.view
                                  middleRect:contentAreaFrame];
  }

  _bottomToolbarSnapshotView =
      [self snapshotOfViewPortionBelowRect:browserLayout.view
                                middleRect:contentAreaFrame];
}

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
  // Take the toolbar snapshots before adding the `_browserLayoutViewController`
  // to the hierarchy (since taking the snapshots forces a screen update). This
  // fixes some transition issues.
  [self takeToolbarSnapshots];

  [_params->browser_layout_view_controller willMoveToParentViewController:nil];
}

// Prepares items for the Tab Grid to Browser transition.
- (void)prepareTabGridToBrowserTransition {
  UIViewController* tabGrid = _params->tab_grid_view_controller;
  UIViewController* parentViewController = _params->parent_view_controller;
  UIViewController* browserLayout = _params->browser_layout_view_controller;
  UIView* appContentGuide = _params->app_content_view;

  if (IsChromeNextIaEnabled()) {
    // Remove from superview to ensure all constraints are gone.
    [browserLayout.view removeFromSuperview];

    if (IsFullscreenRefactoringEnabled()) {
      browserLayout.view.frame =
          [tabGrid.view convertRect:appContentGuide.bounds
                           fromView:appContentGuide];
    } else {
      browserLayout.view.frame = appContentGuide.bounds;
    }
  } else {
    browserLayout.view.frame = tabGrid.view.bounds;
  }

  if (_transitionType != TabGridTransitionType::kDisabledAnimation) {
    // Taking a snapshot can take a few milliseconds during which a screen
    // refresh can occur. If the browserLayout is added to the final position
    // before taking the snapshot, it means that it will be visible in its final
    // position before the animation starts. But it is also necessary to add it
    // to the view hierarchy before taking a snapshot otherwise
    // `-viewWillAppear` and
    // `-viewDidDisappear` are called during the snapshot. The compromise is to
    // add it below all the views so it is part of the view hierarchy but hidden
    // by all the views.
    CGRect browserLayoutOriginalFrame = browserLayout.view.frame;
    UIView* sourceView = tabGrid.view;
    if (IsChromeNextIaEnabled() && !IsFullscreenRefactoringEnabled()) {
      sourceView = appContentGuide;
    }
    UIViewController* rootViewController =
        tabGrid.view.window.rootViewController;
    if (IsFullscreenRefactoringEnabled()) {
      // Temporarily re-enable autoresizing so that the frame can be manually
      // set for the snapshot.
      browserLayout.view.translatesAutoresizingMaskIntoConstraints = YES;
    }
    browserLayout.view.frame =
        [sourceView convertRect:browserLayoutOriginalFrame
                         toView:rootViewController.view];
    [rootViewController addChildViewController:browserLayout];
    [rootViewController.view insertSubview:browserLayout.view atIndex:0];
    if (IsFullscreenRefactoringEnabled()) {
      // Running a layout here ensures that the toolbar frames are correct for
      // the snapshots.
      [browserLayout.view layoutIfNeeded];
    }
    [self takeToolbarSnapshots];
    browserLayout.view.frame = browserLayoutOriginalFrame;
  }

  if (IsChromeNextIaEnabled()) {
    [parentViewController addChildViewController:browserLayout];
    [appContentGuide addSubview:browserLayout.view];
    if (IsFullscreenRefactoringEnabled()) {
      browserLayout.view.translatesAutoresizingMaskIntoConstraints = NO;
      AddSameConstraints(browserLayout.view, appContentGuide);
    }
  } else {
    [tabGrid addChildViewController:browserLayout];
    [tabGrid.view addSubview:browserLayout.view];
    if (IsFullscreenRefactoringEnabled()) {
      browserLayout.view.translatesAutoresizingMaskIntoConstraints = NO;
      AddSameConstraints(browserLayout.view, tabGrid.view);
    }
  }

  // `didMoveToParentViewController` is called in
  // `finalizeTabGridToBrowserTransition`, no need to call here.
  browserLayout.view.accessibilityViewIsModal = YES;
}

// Takes all necessary actions to finish Browser to TabGrid transition.
- (void)finalizeBrowserToTabGridTransition {
  UIViewController* tabGrid = _params->tab_grid_view_controller;
  UIViewController* browserLayout = _params->browser_layout_view_controller;
  [browserLayout.view removeFromSuperview];
  [browserLayout removeFromParentViewController];

  [tabGrid setNeedsStatusBarAppearanceUpdate];
}

// Takes all necessary actions to finish TabGrid to Browser transition.
- (void)finalizeTabGridToBrowserTransition {
  UIViewController* browserLayout = _params->browser_layout_view_controller;
  UIViewController* parentViewController = _params->parent_view_controller;
  [browserLayout didMoveToParentViewController:parentViewController];

  [browserLayout setNeedsStatusBarAppearanceUpdate];
}

// Performs transition animation.
- (void)performTransitionAnimationWithCompletion:(ProceduralBlock)completion {
  // The animation is ugly or crashes when the selected cell is not visible.
  TabGridTransitionType transitionType = _transitionType;
  if (transitionType == TabGridTransitionType::kNormal && !_tabGridCellItem) {
    transitionType = TabGridTransitionType::kReducedMotion;
  }

  // The tab grid transition animation to be performed.
  id<TabGridTransitionAnimation> animation;
  switch (transitionType) {
    case TabGridTransitionType::kNormal: {
      TabGridAnimationParameters* animationParameters =
          [self createAnimationParameters];

      switch (_params->direction) {
        case TabGridTransitionDirection::kFromTabGridToBrowser: {
          animation = [[GridToTabAnimation alloc]
              initWithAnimationParameters:animationParameters];
          break;
        }

        case TabGridTransitionDirection::kFromBrowserToTabGrid: {
          animation = [[TabToGridAnimation alloc]
              initWithAnimationParameters:animationParameters];
          break;
        }
      }

      break;
    }

    case TabGridTransitionType::kReducedMotion: {
      animation = [[TabGridReducedAnimation alloc]
          initWithAnimatedView:_params->browser_layout_view_controller.view
                beingPresented:_params->direction ==
                               TabGridTransitionDirection::
                                   kFromTabGridToBrowser];
      break;
    }

    case TabGridTransitionType::kDisabledAnimation:
      completion();
      return;
  }

  CHECK(animation);
  [animation animateWithCompletion:completion];
}

// Creates animation parameters for the transition.
- (TabGridAnimationParameters*)createAnimationParameters {
  UIViewController* tabGrid = _params->tab_grid_view_controller;
  UIViewController* browserLayout = _params->browser_layout_view_controller;
  CGRect contentAreaFrame = [self contentAreaFrame];

  // Get the "top toolbar height" (everything above the web content area) by
  // using the `contentAreaFrame.origin.y`. This dynamically handles the
  // presence of the Tab Strip and Toolbar across different devices.
  BOOL topToolbarHidden = [self shouldHideTopToolbar];
  CGFloat topToolbarHeight = topToolbarHidden ? tabGrid.view.safeAreaInsets.top
                                              : contentAreaFrame.origin.y;

  // Get the "bottom toolbar height" (everything below the web content area).
  UIView* bottomToolbarView = [_browserLayoutGuideCenter
      referencedViewUnderName:kSecondaryToolbarGuide];
  CGRect bottomToolbarFrameInWindow =
      [bottomToolbarView.superview convertRect:bottomToolbarView.frame
                                        toView:nil];
  CGFloat bottomToolbarHeight = bottomToolbarView.window.bounds.size.height -
                                CGRectGetMinY(bottomToolbarFrameInWindow);

  // Whether the top toolbar should be scaled during the transition,
  // disabled for regular browser NTPs and iPads.
  BOOL scaleTopToolbar =
      !_isRegularBrowserNTP && IsSplitToolbarMode(_activeGrid);

  // Get the animation's destination and origin frames.
  CGRect destinationFrame =
      _params->direction == TabGridTransitionDirection::kFromBrowserToTabGrid
          ? _tabGridCellItem.originalFrame
          : browserLayout.view.frame;

  CGRect originFrame =
      _params->direction == TabGridTransitionDirection::kFromBrowserToTabGrid
          ? browserLayout.view.frame
          : _tabGridCellItem.originalFrame;

  CHECK(_tabGridCellItem);

  return [[TabGridAnimationParameters alloc]
       initWithDestinationFrame:destinationFrame
                    originFrame:originFrame
                     activeGrid:_activeGrid
                     pinnedTabs:_pinnedTabsViewController
               activeCellPinned:_activeCellPinned
                   animatedView:browserLayout.view
                contentSnapshot:_tabGridCellItem.snapshot
               topToolbarHeight:topToolbarHeight
            bottomToolbarHeight:bottomToolbarHeight
         topToolbarSnapshotView:_topToolbarSnapshotView
      bottomToolbarSnapshotView:_bottomToolbarSnapshotView
          shouldScaleTopToolbar:scaleTopToolbar
                      incognito:_incognito
               topToolbarHidden:topToolbarHidden];
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
  UIViewController* tabGrid = _params->tab_grid_view_controller;
  return _isRegularBrowserNTP && !CanShowTabStrip(tabGrid) &&
         IsSplitToolbarMode(tabGrid);
}

// Get the content area's frame.
- (CGRect)contentAreaFrame {
  UIViewController<TabGridTransitionContextProvider>* browserLayout =
      _params->browser_layout_view_controller;

  NamedGuide* contentAreaGuide = [browserLayout contentAreaGuide];
  return [contentAreaGuide.owningView convertRect:contentAreaGuide.layoutFrame
                                           toView:browserLayout.view];
}

@end
