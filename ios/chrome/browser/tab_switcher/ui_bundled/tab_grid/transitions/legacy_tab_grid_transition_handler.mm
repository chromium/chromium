// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/legacy_tab_grid_transition_handler.h"

#import "ios/chrome/browser/main/ui/browser_layout_view_controller.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/legacy_grid_transition_animation.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/legacy_grid_transition_animation_layout_providing.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/transitions/legacy_grid_transition_layout.h"

namespace {
const CGFloat kBrowserToGridDuration = 0.3;
const CGFloat kGridToBrowserDuration = 0.5;
const CGFloat kReducedMotionDuration = 0.25;
const CGFloat kToTabGroupAnimationDuration = 0.25;
}  // namespace

@interface LegacyTabGridTransitionHandler ()
@property(nonatomic, weak) id<LegacyGridTransitionAnimationLayoutProviding>
    layoutProvider;
// Animation object for the transition.
@property(nonatomic, strong) LegacyGridTransitionAnimation* animation;
@end

@implementation LegacyTabGridTransitionHandler

#pragma mark - Public

- (instancetype)initWithLayoutProvider:
    (id<LegacyGridTransitionAnimationLayoutProviding>)layoutProvider {
  self = [super init];
  if (self) {
    _layoutProvider = layoutProvider;
  }
  return self;
}

- (void)transitionFromBrowserLayout:
            (BrowserLayoutViewController*)browserLayoutViewController
                          toTabGrid:(UIViewController*)tabGrid
                         toTabGroup:(BOOL)toTabGroup
                         activePage:(TabGridPage)activePage
                     withCompletion:(void (^)(void))completion {
  [browserLayoutViewController willMoveToParentViewController:nil];

  if (UIAccessibilityIsReduceMotionEnabled() || self.animationDisabled) {
    __weak __typeof(self) weakSelf = self;
    [self transitionWithFadeForTab:browserLayoutViewController.view
                        toTabGroup:toTabGroup
                    beingPresented:NO
                    withCompletion:^{
                      [weakSelf reducedTransitionFromBrowserLayout:
                                    browserLayoutViewController
                                                         toTabGrid:tabGrid
                                                        completion:completion];
                    }];
    return;
  }

  GridAnimationDirection direction = GridAnimationDirectionContracting;
  CGFloat duration = self.animationDisabled ? 0 : kBrowserToGridDuration;

  self.animation = [[LegacyGridTransitionAnimation alloc]
          initWithLayout:[self
                             transitionLayoutForTabInViewController:
                                 browserLayoutViewController
                                                         activePage:activePage]
      gridContainerFrame:[self.layoutProvider gridContainerFrame]
                duration:duration
               direction:direction];

  UIView* animationContainer = [self.layoutProvider animationViewsContainer];
  UIView* bottomViewForAnimations =
      [self.layoutProvider animationViewsContainerBottomView];
  [animationContainer insertSubview:self.animation
                       aboveSubview:bottomViewForAnimations];

  UIView* activeItem = self.animation.activeItem;
  UIView* selectedItem = self.animation.selectionItem;
  BOOL shouldReparentSelectedCell =
      [self.layoutProvider shouldReparentSelectedCell:direction];

  if (shouldReparentSelectedCell) {
    [tabGrid.view addSubview:selectedItem];
    [tabGrid.view addSubview:activeItem];
  }

  [self.animation.animator addAnimations:^{
    [tabGrid setNeedsStatusBarAppearanceUpdate];
  }];

  [self.animation.animator addCompletion:^(UIViewAnimatingPosition position) {
    if (shouldReparentSelectedCell) {
      [activeItem removeFromSuperview];
      [selectedItem removeFromSuperview];
    }
    [self.animation removeFromSuperview];
    if (position == UIViewAnimatingPositionEnd) {
      [browserLayoutViewController.view removeFromSuperview];
      [browserLayoutViewController removeFromParentViewController];
    }
    if (completion) {
      completion();
    }
  }];

  browserLayoutViewController.view.alpha = 0;

  // Run the main animation. There is an issue if the animation is run directly.
  // See crbug.com/1458980.
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                               static_cast<int64_t>(0.01 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [self.animation.animator startAnimation];
                 });
}

- (void)transitionFromTabGrid:(UIViewController*)tabGrid
              toBrowserLayout:
                  (BrowserLayoutViewController*)browserLayoutViewController
                   activePage:(TabGridPage)activePage
               withCompletion:(void (^)(void))completion {
  [tabGrid addChildViewController:browserLayoutViewController];

  browserLayoutViewController.view.frame = tabGrid.view.bounds;
  [tabGrid.view addSubview:browserLayoutViewController.view];

  browserLayoutViewController.view.accessibilityViewIsModal = YES;

  if (self.animationDisabled) {
    browserLayoutViewController.view.alpha = 1;
    [tabGrid setNeedsStatusBarAppearanceUpdate];
    if (completion) {
      completion();
    }
    return;
  }

  browserLayoutViewController.view.alpha = 0;

  __weak __typeof(self) weakSelf = self;
  if (UIAccessibilityIsReduceMotionEnabled() ||
      !self.layoutProvider.selectedCellVisible) {
    [self
        transitionWithFadeForTab:browserLayoutViewController.view
                      toTabGroup:NO
                  beingPresented:YES
                  withCompletion:^{
                    [weakSelf
                        reducedTransitionFromTabGrid:tabGrid
                                     toBrowserLayout:browserLayoutViewController
                                          completion:completion];
                  }];
    return;
  }

  GridAnimationDirection direction = GridAnimationDirectionExpanding;
  CGFloat duration = self.animationDisabled ? 0 : kGridToBrowserDuration;

  self.animation = [[LegacyGridTransitionAnimation alloc]
          initWithLayout:[self
                             transitionLayoutForTabInViewController:
                                 browserLayoutViewController
                                                         activePage:activePage]
      gridContainerFrame:[self.layoutProvider gridContainerFrame]
                duration:duration
               direction:direction];

  UIView* animationContainer = [self.layoutProvider animationViewsContainer];
  UIView* bottomViewForAnimations =
      [self.layoutProvider animationViewsContainerBottomView];
  [animationContainer insertSubview:self.animation
                       aboveSubview:bottomViewForAnimations];

  UIView* activeItem = self.animation.activeItem;
  UIView* selectedItem = self.animation.selectionItem;
  BOOL shouldReparentSelectedCell =
      [self.layoutProvider shouldReparentSelectedCell:direction];

  if (shouldReparentSelectedCell) {
    [tabGrid.view addSubview:selectedItem];
    [tabGrid.view addSubview:activeItem];
  }

  [self.animation.animator addAnimations:^{
    [tabGrid setNeedsStatusBarAppearanceUpdate];
  }];

  [self.animation.animator addCompletion:^(UIViewAnimatingPosition position) {
    if (shouldReparentSelectedCell) {
      [activeItem removeFromSuperview];
      [selectedItem removeFromSuperview];
    }
    [self.animation removeFromSuperview];
    if (position == UIViewAnimatingPositionEnd) {
      browserLayoutViewController.view.alpha = 1;
      [browserLayoutViewController didMoveToParentViewController:tabGrid];
    }
    if (completion) {
      completion();
    }
  }];

  // Run the main animation.
  [self.animation.animator startAnimation];
}

#pragma mark - Private

// Returns the transition layout for the `activePage`, based on the
// `browserLayoutViewController`.
- (LegacyGridTransitionLayout*)
    transitionLayoutForTabInViewController:
        (BrowserLayoutViewController*)browserLayoutViewController
                                activePage:(TabGridPage)activePage {
  LegacyGridTransitionLayout* layout =
      [self.layoutProvider transitionLayout:activePage];

  // Get the frame for the snapshotted content of the active tab.
  NamedGuide* contentAreaGuide = [browserLayoutViewController contentAreaGuide];
  UIView* tabContentView = browserLayoutViewController.view;

  CGRect contentArea = contentAreaGuide.layoutFrame;

  CGFloat previousAlpha = tabContentView.alpha;
  tabContentView.alpha = 1;
  [layout.activeItem populateWithSnapshotsFromView:tabContentView
                                        middleRect:contentArea];
  tabContentView.alpha = previousAlpha;

  layout.expandedRect = [[self.layoutProvider animationViewsContainer]
      convertRect:tabContentView.frame
         fromView:browserLayoutViewController.view];

  return layout;
}

// Animates the transition for the `tab`, whether it is `beingPresented` or not,
// with a fade in/out.
- (void)transitionWithFadeForTab:(UIView*)tab
                      toTabGroup:(BOOL)toTabGroup
                  beingPresented:(BOOL)beingPresented
                  withCompletion:(void (^)(void))completion {
  // The animation here creates a simple quick zoom effect -- the tab view
  // fades in/out as it expands/contracts. The zoom is not large (75% to 100%)
  // and is centered on the view's final center position, so it's not directly
  // connected to any tab grid positions.
  CGFloat tabFinalAlpha;
  CGAffineTransform tabFinalTransform;
  CGFloat tabFinalCornerRadius;

  if (beingPresented) {
    // If presenting, the tab view animates in from 0% opacity, 75% scale
    // transform, and a 26pt corner radius
    tabFinalAlpha = 1;
    tabFinalTransform = tab.transform;
    tab.transform = CGAffineTransformScale(tabFinalTransform, 0.75, 0.75);
    tabFinalCornerRadius = DeviceCornerRadius();
    tab.layer.cornerRadius = 26.0;
  } else {
    // If dismissing, the the tab view animates out to 0% opacity, 75% scale,
    // and 26px corner radius.
    tabFinalAlpha = 0;
    tabFinalTransform = CGAffineTransformScale(tab.transform, 0.75, 0.75);
    tab.layer.cornerRadius = DeviceCornerRadius();
    tabFinalCornerRadius = 26.0;
  }

  // Set clipsToBounds on the animating view so its corner radius will look
  // right.
  BOOL oldClipsToBounds = tab.clipsToBounds;
  tab.clipsToBounds = YES;

  CGFloat duration =
      toTabGroup ? kToTabGroupAnimationDuration : kReducedMotionDuration;
  duration = self.animationDisabled ? 0 : duration;
  [UIView animateWithDuration:duration
      delay:0.0
      options:UIViewAnimationOptionCurveEaseOut
      animations:^{
        tab.alpha = tabFinalAlpha;
        tab.transform = tabFinalTransform;
        tab.layer.cornerRadius = tabFinalCornerRadius;
      }
      completion:^(BOOL finished) {
        // When presenting the FirstRun ViewController, this can be called with
        // `finished` to NO on official builds. For now, the animation not
        // finishing isn't handled anywhere.
        tab.clipsToBounds = oldClipsToBounds;
        tab.transform = CGAffineTransformIdentity;
        if (completion) {
          completion();
        }
      }];
}

// Called when the transition with reduced animations from the `tabGrid` to the
// `browserLayoutViewController` is complete.
- (void)reducedTransitionFromTabGrid:(UIViewController*)tabGrid
                     toBrowserLayout:(BrowserLayoutViewController*)
                                         browserLayoutViewController
                          completion:(void (^)(void))completion {
  [browserLayoutViewController didMoveToParentViewController:tabGrid];
  [tabGrid setNeedsStatusBarAppearanceUpdate];
  if (completion) {
    completion();
  }
}

// Called when the transition with reduced animations from the
// `browserLayoutViewController` to the `tabGrid` is complete.
- (void)reducedTransitionFromBrowserLayout:
            (BrowserLayoutViewController*)browserLayoutViewController
                                 toTabGrid:(UIViewController*)tabGrid
                                completion:(void (^)(void))completion {
  [browserLayoutViewController.view removeFromSuperview];
  [browserLayoutViewController removeFromParentViewController];
  [tabGrid setNeedsStatusBarAppearanceUpdate];
  if (completion) {
    completion();
  }
}

@end
