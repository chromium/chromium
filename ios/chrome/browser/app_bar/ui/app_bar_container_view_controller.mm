// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_controller.h"

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_container_view.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_delegate.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@interface AppBarContainerViewController () <AppBarContainerViewDelegate,
                                             LayoutStateObserver>
@property(nonatomic, strong) AppBarContainerView* view;
@end

@implementation AppBarContainerViewController {
  AppBarViewController* _appBar;
  // The last fullscreen progress value received.
  CGFloat _fullscreenProgress;
}

- (void)dealloc {
  [_layoutState removeObserver:self];
}

- (void)setLayoutState:(LayoutState*)layoutState {
  if (_layoutState == layoutState) {
    return;
  }
  [_layoutState removeObserver:self];
  _layoutState = layoutState;
  [_layoutState addObserver:self];
  [self updateLayout];
}

#pragma mark - LayoutStateObserver

- (void)layoutState:(LayoutState*)layoutState
    didChangeAppBarPosition:(AppBarPosition)appBarPosition {
  [self updateLayout];
}

@dynamic view;

- (void)setAppBar:(AppBarViewController*)appBar {
  if (_appBar == appBar) {
    return;
  }
  [_appBar willMoveToParentViewController:nil];
  [_appBar.view removeFromSuperview];
  [_appBar removeFromParentViewController];

  _appBar = appBar;

  [self addChildViewController:_appBar];
  [self.view setAppBar:_appBar.view];
  [_appBar didMoveToParentViewController:self];
}

#pragma mark - UIViewController

- (void)loadView {
  self.view = [[AppBarContainerView alloc] init];
  self.view.delegate = self;
  _fullscreenProgress = 1;
}

#pragma mark - AppBarContainerViewDelegate

- (void)appBarContainerDidMoveToWindow:(AppBarContainerView*)appBarContainer {
  [self updateLayout];
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  _fullscreenProgress = progress;
  [self updateLayout];
  [self.view setNeedsLayout];
  [self.view layoutIfNeeded];
}

#pragma mark - FullscreenBrowserAgentObserving

- (void)fullscreenWillUpdateObscuredInsetRange:(FullscreenBrowserAgent*)agent {
  AppBarPosition position = self.layoutState.appBarPosition;
  switch (position) {
    case AppBarPosition::kBottom:
      agent->AddObscuredInsetRange(UIRectEdgeBottom, kAppBarHeightFullscreen,
                                   kAppBarHeight);
      break;
    case AppBarPosition::kLeft:
      agent->AddObscuredInsetRange(UIRectEdgeLeft, kAppBarHeightLandscape,
                                   kAppBarHeightLandscape);
      break;
    case AppBarPosition::kRight:
      agent->AddObscuredInsetRange(UIRectEdgeRight, kAppBarHeightLandscape,
                                   kAppBarHeightLandscape);
      break;
    case AppBarPosition::kNone:
      break;
  }
}

- (void)fullscreenWillUpdateState:(FullscreenBrowserAgent*)agent {
  AppBarPosition position = self.layoutState.appBarPosition;
  switch (position) {
    case AppBarPosition::kBottom: {
      _fullscreenProgress = agent->bottom_progress();
      CGFloat currentHeight =
          kAppBarHeightFullscreen +
          (kAppBarHeight - kAppBarHeightFullscreen) * agent->bottom_progress();
      agent->AddObscuredInset(UIRectEdgeBottom, currentHeight);
      [self updateLayout];
      // If this is inside an animation, layout immediately.
      if (!agent->animation_duration().is_zero()) {
        [self.view layoutIfNeeded];
      }
      break;
    }
    case AppBarPosition::kLeft:
      agent->AddObscuredInset(UIRectEdgeLeft, kAppBarHeightLandscape);
      break;
    case AppBarPosition::kRight:
      agent->AddObscuredInset(UIRectEdgeRight, kAppBarHeightLandscape);
      break;
    case AppBarPosition::kNone:
      break;
  }
}

#pragma mark - Private

// Handles updating the UI for a size transition.
- (void)setFullscreenProgress:(CGFloat)progress {
  _fullscreenProgress = progress;
}

// Updates the layout based on the current orientation and fullscreen progress.
- (void)updateLayout {
  UIWindowScene* windowScene = self.view.window.windowScene;
  if (!windowScene) {
    return;
  }

  AppBarPosition position = self.layoutState.appBarPosition;
  CGFloat angle = 0;

  switch (position) {
    case AppBarPosition::kLeft:
      angle = M_PI_2;
      break;

    case AppBarPosition::kRight:
      angle = -M_PI_2;
      break;

    default:  // kBottom, kNone (Portrait)
      angle = 0;
      break;
  }

  self.view.transform = CGAffineTransformMakeRotation(angle);
  self.view.fullscreenProgress = _fullscreenProgress;
  self.view.appBarPosition = position;
  [_appBar updateForAngle:-angle];
}

@end
