// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/ui/browser_layout_view_controller.h"

#import <ostream>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "ios/chrome/browser/browser_view/ui_bundled/safe_area_provider.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_updater.h"
#import "ios/chrome/browser/main/ui/browser_layout_consumer.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/browser/tab_switcher/tab_strip/ui/tab_strip_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/device_form_factor.h"

@interface BrowserLayoutViewController () <FullscreenUIElement>
@end

@implementation BrowserLayoutViewController {
  // Observer for the fullscreen controller.
  std::unique_ptr<FullscreenUIUpdater> _fullscreenUIUpdater;
  // The range of the viewport insets.
  CGFloat _fullscreenViewportInsetRange;
  // The current fullscreen progress.
  CGFloat _fullscreenProgress;

  // View acting as the status bar background.
  UIView* _fadingStatusBarView;
  // View acting as the background for the status bar when the tab strip is
  // hidden or transparent.
  UIView* _staticStatusBarView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Register for trait changes that affect the tab strip visibility.
  NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
    UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class,
    UITraitUserInterfaceIdiom.class
  ]);
  __weak __typeof(self) weakSelf = self;
  UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                   UITraitCollection* previousCollection) {
    [weakSelf updateUIOnTraitChange:previousCollection];
  };
  [self registerForTraitChanges:traits withHandler:handler];
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.browserViewController;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.browserViewController;
}

- (BOOL)shouldAutorotate {
  return self.browserViewController
             ? [self.browserViewController shouldAutorotate]
             : [super shouldAutorotate];
}

#pragma mark - Public

- (void)setBrowserViewController:
    (UIViewController<BrowserLayoutConsumer>*)browserViewController {
  DCHECK(browserViewController);
  if (_browserViewController == browserViewController) {
    return;
  }

  // Remove the current browserViewController, if any.
  if (_browserViewController) {
    [_browserViewController willMoveToParentViewController:nil];
    [_browserViewController.view removeFromSuperview];
    [_browserViewController removeFromParentViewController];
    _browserViewController = nil;
  }

  DCHECK_EQ(nil, _browserViewController);

  // Add the new active view controller.
  [self addChildViewController:browserViewController];
  // If the BVC's view has a transform, then its frame isn't accurate.
  // Instead, remove the transform, set the frame, then reapply the transform.
  CGAffineTransform oldTransform = browserViewController.view.transform;
  browserViewController.view.transform = CGAffineTransformIdentity;
  browserViewController.view.frame = self.view.bounds;
  browserViewController.view.transform = oldTransform;
  [self.view insertSubview:browserViewController.view atIndex:0];
  [browserViewController didMoveToParentViewController:self];
  _browserViewController = browserViewController;

  // This will fail if there's another child view controller added before
  // `browserViewController`. If this happens during startup, it may be the BVC
  // adding launch screen as a child VC of this VC (BVC's parent).
  // TODO:(crbug.com/472278494): This fires frequently in stable.
  CHECK_EQ(_browserViewController, browserViewController,
           base::NotFatalUntil::M150);

  [self updateCurrentBVCLayoutInsets];
}

- (void)updateCurrentBVCLayoutInsets {
  CGFloat topInset = 0;
  if (CanShowTabStrip(self)) {
    CHECK(self.safeAreaProvider);
    topInset = self.safeAreaProvider.safeArea.top;

    if (self.tabStripViewController) {
      topInset += TabStripCollectionViewConstants.height;
    }
  }
  self.browserViewController.topToolbarInset = topInset;
}

- (void)setUpFullscreenObservation:(FullscreenController*)fullscreenController {
  if (fullscreenController) {
    _fullscreenUIUpdater =
        std::make_unique<FullscreenUIUpdater>(fullscreenController, self);
  } else {
    _fullscreenUIUpdater = nullptr;
  }
}

- (void)viewSafeAreaInsetsDidChange {
  [super viewSafeAreaInsetsDidChange];
  [self updateCurrentBVCLayoutInsets];
  if (_tabStripViewController) {
    [self updateForFullscreenProgress:_fullscreenProgress];
  }
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenMinViewportInsets:(UIEdgeInsets)minViewportInsets
                           maxViewportInsets:(UIEdgeInsets)maxViewportInsets {
  _fullscreenViewportInsetRange = maxViewportInsets.top - minViewportInsets.top;
}

- (void)updateForFullscreenProgress:(CGFloat)progress {
  _fullscreenProgress = progress;
  // Calculate offset based on progress (0 = collapsed/hidden, 1 =
  // expanded/visible).
  CGFloat offset =
      AlignValueToPixel((1.0 - progress) * _fullscreenViewportInsetRange);

  // Update frame directly for synchronous layout.
  // We don't rely on constraints here to avoid fighting with the layout system
  // during safe area transitions.
  CHECK(self.safeAreaProvider);
  CGFloat topInset = self.safeAreaProvider.safeArea.top;
  CGRect frame = _tabStripViewController.view.frame;
  frame.origin.y = topInset - offset;
  _tabStripViewController.view.frame = frame;

  // When offset >= topInset, the tab strip is fully hidden (or covered by
  // status bar area). When offset == 0, the tab strip is fully visible.
  // Interpolate alpha from 0 to 1 as offset goes from topInset to 0.
  CGFloat clampedOffset = std::clamp(offset, 0.0, topInset);
  CGFloat alpha = 1.0 - (clampedOffset / topInset);
  _fadingStatusBarView.alpha = alpha;
  _tabStripViewController.view.alpha = alpha;
}

- (void)updateForFullscreenEnabled:(BOOL)enabled {
  if (!enabled) {
    [self updateForFullscreenProgress:1.0];
  }
}

- (void)animateFullscreenWithAnimator:(FullscreenAnimator*)animator {
  CGFloat finalProgress = animator.finalProgress;
  [animator addAnimations:^{
    [self updateForFullscreenProgress:finalProgress];
    [self.view layoutIfNeeded];
  }];
}

#pragma mark - Private

// Updates the UI when the trait collection changes.
- (void)updateUIOnTraitChange:(UITraitCollection*)previousTraitCollection {
  [self updateStatusBarBackgroundViews];
  [self updateCurrentBVCLayoutInsets];
  if (_tabStripViewController) {
    _tabStripViewController.view.hidden = !CanShowTabStrip(self);
  }
}

// Updates the status bar background views properties and visibility.
- (void)updateStatusBarBackgroundViews {
  DCHECK(self.isViewLoaded);

  // Remove views to reset constraints.
  [_fadingStatusBarView removeFromSuperview];
  [_staticStatusBarView removeFromSuperview];

  // Only install status bar background when the tab strip is visible.
  if (!CanShowTabStrip(self) || !_tabStripViewController) {
    return;
  }

  // Ensure the tab strip is on top.
  [self.view bringSubviewToFront:_tabStripViewController.view];

  // Insert status bars below the tab strip.
  [self.view insertSubview:self.staticStatusBarView
              belowSubview:_tabStripViewController.view];
  [self.view insertSubview:self.fadingStatusBarView
              belowSubview:_tabStripViewController.view];

  [NSLayoutConstraint activateConstraints:@[
    [self.fadingStatusBarView.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [self.fadingStatusBarView.bottomAnchor
        constraintEqualToAnchor:_tabStripViewController.view.bottomAnchor],
    [self.fadingStatusBarView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.fadingStatusBarView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
  AddSameConstraints(self.staticStatusBarView, self.fadingStatusBarView);
  [self updateOverlayContainerOrder];
}

// Updates the ordering of the overlay containers so that they are layered
// directly on top of the tab strip UI. Banner overlays appear behind modal
// overlays.
- (void)updateOverlayContainerOrder {
  // Both infobar overlay container views should exist in front of the entire
  // browser UI, and the banner container should appear behind the modal
  // container.
  [self bringOverlayContainerToFront:
            self.infobarBannerOverlayContainerViewController];
  [self bringOverlayContainerToFront:
            self.infobarModalOverlayContainerViewController];
}

// Helper method to bring the given `containerViewController` to the front.
- (void)bringOverlayContainerToFront:
    (UIViewController*)containerViewController {
  if (!containerViewController) {
    return;
  }
  [self.view bringSubviewToFront:containerViewController.view];
  // If `containerViewController` is presenting a view over its current context,
  // its presentation container view is added as a sibling to
  // `containerViewController`'s view. This presented view should be brought in
  // front of the container view.
  UIView* presentedContainerView =
      containerViewController.presentedViewController.presentationController
          .containerView;
  if (presentedContainerView.superview == self.view) {
    [self.view bringSubviewToFront:presentedContainerView];
  }
}

// Removes the tab strip view controller from the hierarchy.
- (void)removeTabStripViewController {
  if (!_tabStripViewController) {
    return;
  }
  [_tabStripViewController willMoveToParentViewController:nil];
  [_tabStripViewController.view removeFromSuperview];
  [_tabStripViewController removeFromParentViewController];
}

// Adds the tab strip view controller to the hierarchy and sets constraints.
- (void)addTabStripViewController {
  if (!_tabStripViewController) {
    return;
  }
  UIView* tabStripView = _tabStripViewController.view;

  [self addChildViewController:_tabStripViewController];

  // Use manual frame layout for tab strip to avoid constraint fighting during
  // fullscreen.
  tabStripView.translatesAutoresizingMaskIntoConstraints = YES;
  tabStripView.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleBottomMargin;

  [self.view addSubview:tabStripView];
  [_tabStripViewController didMoveToParentViewController:self];

  // Set default frame to ensure valid initial position.
  CHECK(self.safeAreaProvider);
  CGFloat topInset = self.safeAreaProvider.safeArea.top;
  CGRect frame = self.view.bounds;
  frame.origin.y = topInset;
  frame.size.height = TabStripCollectionViewConstants.height;
  tabStripView.frame = frame;
  tabStripView.hidden = !CanShowTabStrip(self);
}

#pragma mark - Properties

// Sets the incognito state and updates status bar styles.
- (void)setIncognito:(BOOL)incognito {
  if (_incognito == incognito) {
    return;
  }
  _incognito = incognito;

  UIUserInterfaceStyle style =
      _incognito ? UIUserInterfaceStyleDark : UIUserInterfaceStyleUnspecified;
  _fadingStatusBarView.overrideUserInterfaceStyle = style;
  _staticStatusBarView.overrideUserInterfaceStyle = style;
}

// Sets the tab strip view controller and installs it in the view hierarchy.
- (void)setTabStripViewController:(UIViewController*)tabStripViewController {
  if (_tabStripViewController == tabStripViewController) {
    return;
  }

  [self removeTabStripViewController];

  _tabStripViewController = tabStripViewController;

  [self addTabStripViewController];
  [self updateStatusBarBackgroundViews];

  // Notify the BVC about the layout inset changes.
  [self updateCurrentBVCLayoutInsets];

  if (!_tabStripViewController) {
    return;
  }

  [self updateForFullscreenProgress:_fullscreenProgress];
}

- (void)setInfobarBannerOverlayContainerViewController:
    (UIViewController*)infobarBannerOverlayContainerViewController {
  if (_infobarBannerOverlayContainerViewController ==
      infobarBannerOverlayContainerViewController) {
    return;
  }
  _infobarBannerOverlayContainerViewController =
      infobarBannerOverlayContainerViewController;
  [self updateOverlayContainerOrder];
}

- (void)setInfobarModalOverlayContainerViewController:
    (UIViewController*)infobarModalOverlayContainerViewController {
  if (_infobarModalOverlayContainerViewController ==
      infobarModalOverlayContainerViewController) {
    return;
  }
  _infobarModalOverlayContainerViewController =
      infobarModalOverlayContainerViewController;
  [self updateOverlayContainerOrder];
}

- (UIView*)fadingStatusBarView {
  if (!_fadingStatusBarView) {
    _fadingStatusBarView = [[UIView alloc] init];
    _fadingStatusBarView.translatesAutoresizingMaskIntoConstraints = NO;
    _fadingStatusBarView.backgroundColor = TabStripHelper.backgroundColor;
    _fadingStatusBarView.overrideUserInterfaceStyle =
        _incognito ? UIUserInterfaceStyleDark : UIUserInterfaceStyleUnspecified;
  }
  return _fadingStatusBarView;
}

- (UIView*)staticStatusBarView {
  if (!_staticStatusBarView) {
    _staticStatusBarView = [[UIView alloc] init];
    _staticStatusBarView.translatesAutoresizingMaskIntoConstraints = NO;
    _staticStatusBarView.backgroundColor =
        [UIColor colorNamed:kBackgroundColor];
    _staticStatusBarView.overrideUserInterfaceStyle =
        _incognito ? UIUserInterfaceStyleDark : UIUserInterfaceStyleUnspecified;
  }
  return _staticStatusBarView;
}

#pragma mark - TabGridTransitionContextProvider

- (NamedGuide*)contentAreaGuide {
  return [NamedGuide guideWithName:kContentAreaGuide
                              view:self.browserViewController.view];
}

@end
