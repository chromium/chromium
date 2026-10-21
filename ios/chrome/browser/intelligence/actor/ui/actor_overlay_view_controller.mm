// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_glow_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_glow_view_data.h"
#import "ios/chrome/browser/intelligence/actor/ui/actor_overlay_scrim_view.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/ui/util/layout_constants.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/device_form_factor.h"

// Container view that passes through touches when no interactive subview is
// hit.
@interface ActorOverlayContainerView : UIView
@end

@implementation ActorOverlayContainerView

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  UIView* hitView = [super hitTest:point withEvent:event];
  return (hitView == self) ? nil : hitView;
}

@end

// Private interface for `ActorOverlayViewController`.
@interface ActorOverlayViewController () <SceneLayoutStateObserver>
@end

@implementation ActorOverlayViewController {
  // The layout guide center for the browser.
  __weak LayoutGuideCenter* _browserLayoutGuideCenter;
  // The base scrim color.
  UIColor* _scrimColor;
  // The base glow color.
  UIColor* _glowColor;
  // The scrim view.
  ActorOverlayScrimView* _scrimView;
  // The glow view.
  ActorOverlayGlowView* _glowView;
  // The primary toolbar layout guide.
  UILayoutGuide* _primaryToolbarGuide;
  // The layout state for observing app bar position changes.
  __weak SceneLayoutState* _layoutState;
  // Dynamic glow constraints whose constants change based on App Bar position.
  NSLayoutConstraint* _glowBottomConstraint;
  NSLayoutConstraint* _glowLeadingConstraint;
  NSLayoutConstraint* _glowTrailingConstraint;
  // Mutually exclusive top constraints for pinning below the primary toolbar vs
  // the full container top.
  NSLayoutConstraint* _scrimToolbarTopConstraint;
  NSLayoutConstraint* _scrimFullScreenTopConstraint;
  NSLayoutConstraint* _glowToolbarTopConstraint;
  NSLayoutConstraint* _glowFullScreenTopConstraint;
}

- (instancetype)initWithBrowserLayoutGuideCenter:
                    (LayoutGuideCenter*)browserCenter
                                      scrimColor:(UIColor*)scrimColor
                                       glowColor:(UIColor*)glowColor {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    CHECK(browserCenter);
    CHECK(scrimColor);
    CHECK(glowColor);
    _browserLayoutGuideCenter = browserCenter;
    _scrimColor = scrimColor;
    _glowColor = glowColor;
  }
  return self;
}

- (void)setLayoutState:(SceneLayoutState*)layoutState {
  if (_layoutState == layoutState) {
    return;
  }
  [_layoutState removeObserver:self];
  _layoutState = layoutState;
  [_layoutState addObserver:self];
  [self updateGlowConstraintsAndCornerRadii];
}

#pragma mark - UIViewController

- (void)loadView {
  _scrimView = [[ActorOverlayScrimView alloc] initWithScrimColor:_scrimColor];
  _scrimView.translatesAutoresizingMaskIntoConstraints = NO;
  _glowView = [[ActorOverlayGlowView alloc] initWithGlowColor:_glowColor];
  _glowView.translatesAutoresizingMaskIntoConstraints = NO;
  UIView* view = [[ActorOverlayContainerView alloc] initWithFrame:CGRectZero];
  view.translatesAutoresizingMaskIntoConstraints = NO;
  [view addSubview:_scrimView];
  [view addSubview:_glowView];
  self.view = view;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  [self setupScrimAndGlowConstraints];

  // Set initial constraints and corner radii based on current App Bar position.
  [self updateGlowConstraintsAndCornerRadii];

  [self
      registerForTraitChanges:
          @[ UITraitHorizontalSizeClass.class, UITraitVerticalSizeClass.class ]
                   withAction:@selector(updateGlowConstraintsAndCornerRadii)];
}

- (void)viewIsAppearing:(BOOL)animated {
  [super viewIsAppearing:animated];
  [self updateGlowConstraintsAndCornerRadii];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];

  if (!parent) {
    return;
  }
  UIView* view = self.view;
  UIView* superview = view.superview;
  if (!superview) {
    return;
  }

  AddSameConstraints(view, superview);
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // TODO(crbug.com/556733164): Remove once the
  // `kActorToolsDisableInputDuringActuationParam` killswitch is no longer
  // needed. `endEditing:YES` resigns first responder and prevents the keyboard
  // from reappearing when actuation ends.
  if (!ActorDisablesInputDuringActuation()) {
    [self.view.window endEditing:YES];
  }
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
  _glowView.hidden = YES;
  __weak UIView* weakGlowView = _glowView;
  if (coordinator) {
    [coordinator
        animateAlongsideTransition:nil
                        completion:^(
                            id<UIViewControllerTransitionCoordinatorContext>
                                context) {
                          weakGlowView.hidden = NO;
                        }];
  } else {
    _glowView.hidden = NO;
  }
}

#pragma mark - Private

// Returns YES if the scrim and glow views should be pinned below the primary
// toolbar.
- (BOOL)shouldPinBelowPrimaryToolbar {
  return (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET ||
          CanShowTabStrip(self)) &&
         _primaryToolbarGuide != nil;
}

// Updates the active top constraints for the scrim and glow views.
- (void)updateTopConstraints {
  BOOL pinBelowToolbar = [self shouldPinBelowPrimaryToolbar];
  _scrimFullScreenTopConstraint.active = !pinBelowToolbar;
  _glowFullScreenTopConstraint.active = !pinBelowToolbar;
  _scrimToolbarTopConstraint.active = pinBelowToolbar;
  _glowToolbarTopConstraint.active = pinBelowToolbar;
}

// Sets up the static and dynamic layout constraints for the subviews.
- (void)setupScrimAndGlowConstraints {
  UIView* contentView = self.view;

  NSLayoutYAxisAnchor* topAnchor = contentView.topAnchor;
  NSLayoutYAxisAnchor* bottomAnchor = contentView.bottomAnchor;
  NSLayoutXAxisAnchor* leadingAnchor = contentView.leadingAnchor;
  NSLayoutXAxisAnchor* trailingAnchor = contentView.trailingAnchor;

  // Create both the toolbar-pinned and full-screen top constraints upfront so
  // `-updateTopConstraints` can dynamically switch between them when the size
  // class changes.
  _primaryToolbarGuide =
      [_browserLayoutGuideCenter makeLayoutGuideNamed:kPrimaryToolbarGuide];
  if (_primaryToolbarGuide) {
    [contentView addLayoutGuide:_primaryToolbarGuide];
    _scrimToolbarTopConstraint = [_scrimView.topAnchor
        constraintEqualToAnchor:_primaryToolbarGuide.bottomAnchor];
    _glowToolbarTopConstraint = [_glowView.topAnchor
        constraintEqualToAnchor:_primaryToolbarGuide.bottomAnchor];
  }
  _scrimFullScreenTopConstraint =
      [_scrimView.topAnchor constraintEqualToAnchor:topAnchor];
  _glowFullScreenTopConstraint =
      [_glowView.topAnchor constraintEqualToAnchor:topAnchor];

  // Initialize dynamic glow constraints.
  _glowBottomConstraint =
      [_glowView.bottomAnchor constraintEqualToAnchor:bottomAnchor];
  _glowLeadingConstraint =
      [_glowView.leadingAnchor constraintEqualToAnchor:leadingAnchor];
  _glowTrailingConstraint =
      [_glowView.trailingAnchor constraintEqualToAnchor:trailingAnchor];

  [self updateTopConstraints];

  [NSLayoutConstraint activateConstraints:@[
    // Activate static scrim constraints.
    [_scrimView.bottomAnchor constraintEqualToAnchor:bottomAnchor],
    [_scrimView.leadingAnchor constraintEqualToAnchor:leadingAnchor],
    [_scrimView.trailingAnchor constraintEqualToAnchor:trailingAnchor],
    // Activate dynamic parts of glow constraints.
    _glowBottomConstraint,
    _glowLeadingConstraint,
    _glowTrailingConstraint,
  ]];
}

// Updates the active top constraints, dynamic glow constraints, and corner
// radii based on the current size class and App Bar position.
- (void)updateGlowConstraintsAndCornerRadii {
  if (!self.isViewLoaded) {
    return;
  }

  [self updateTopConstraints];

  CornerRadii radii(DeviceCornerRadius());

  if ([self shouldPinBelowPrimaryToolbar]) {
    radii.topLeft = 0.0;
    radii.topRight = 0.0;
  }

  AppBarPosition position =
      _layoutState ? _layoutState.appBarPosition : AppBarPosition::kNone;
  CGFloat bottomConstant = 0.0;
  CGFloat leadingConstant = 0.0;
  CGFloat trailingConstant = 0.0;

  switch (position) {
    case AppBarPosition::kBottom:
      bottomConstant = -CurrentAppBarHeightPortrait(YES, YES);
      radii.bottomLeft = kAppBarCornerRadius;
      radii.bottomRight = kAppBarCornerRadius;
      break;
    case AppBarPosition::kLeft:
      leadingConstant = AppBarHeightLandscape();
      radii.topLeft = kAppBarCornerRadius;
      radii.bottomLeft = kAppBarCornerRadius;
      break;
    case AppBarPosition::kRight:
      trailingConstant = -AppBarHeightLandscape();
      radii.topRight = kAppBarCornerRadius;
      radii.bottomRight = kAppBarCornerRadius;
      break;
    case AppBarPosition::kNone:
      break;
  }

  _glowBottomConstraint.constant = bottomConstant;
  _glowLeadingConstraint.constant = leadingConstant;
  _glowTrailingConstraint.constant = trailingConstant;

  _glowView.cornerRadii = radii;
}

#pragma mark - SceneLayoutStateObserver

- (void)layoutState:(SceneLayoutState*)layoutState
    didChangeAppBarPosition:(AppBarPosition)appBarPosition {
  [self updateGlowConstraintsAndCornerRadii];
}

- (void)layoutState:(SceneLayoutState*)layoutState
    didChangeAssistantContainerInvoked:(BOOL)assistantContainerInvoked {
  [self updateGlowConstraintsAndCornerRadii];
}

@end
