// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_controller.h"

#import <algorithm>

#import "ios/chrome/browser/app_bar/ui/app_bar_constants.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_container_view.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_container_view_delegate.h"
#import "ios/chrome/browser/app_bar/ui/app_bar_view_controller.h"
#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_animator.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/layout_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@interface AppBarContainerViewController () <AppBarContainerViewDelegate,
                                             LayoutStateObserver>
@property(nonatomic, strong) AppBarContainerView* view;
@end

@implementation AppBarContainerViewController {
  AppBarViewController* _appBar;
  // The last fullscreen progress value received.
  CGFloat _fullscreenProgress;
  // Following next responder for ResponderChaining.
  __weak UIResponder* _followingNextResponder;
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

- (void)layoutState:(LayoutState*)layoutState
    didChangeAssistantContainerCutoutRadius:
        (CGFloat)assistantContainerCutoutRadius {
  [self updateCutoutRadius:assistantContainerCutoutRadius];
}

- (void)layoutState:(LayoutState*)layoutState
    didChangeAppBarLockedInFullscreen:(BOOL)appBarLockedInFullscreen {
  [self updateAndApplyLayout];
}

- (void)layoutState:(LayoutState*)layoutState
    didChangeToolbarPosition:(ToolbarPosition)toolbarPosition {
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

#pragma mark - ResponderChaining

- (void)respondBeforeResponder:(UIResponder*)nextResponder {
  _followingNextResponder = nextResponder;
}

#pragma mark - AppBarContainerViewDelegate

- (void)appBarContainerDidMoveToWindow:(AppBarContainerView*)appBarContainer {
  [self updateLayout];
}

#pragma mark - UIResponder

- (UIResponder*)nextResponder {
  UIResponder* nextResponder = _followingNextResponder ?: [super nextResponder];
  if (_appBar) {
    [_appBar respondBeforeResponder:nextResponder];
    nextResponder = _appBar;
  }
  return nextResponder;
}

#pragma mark - FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  _fullscreenProgress = progress;
  if (self.layoutState.appBarLockedInFullscreen) {
    return;
  }
  [self updateAndApplyLayout];
}

#pragma mark - FullscreenBrowserAgentObserving

- (void)fullscreenWillUpdateObscuredInsetRange:(FullscreenBrowserAgent*)agent {
  AppBarPosition position = self.layoutState.appBarPosition;
  switch (position) {
    case AppBarPosition::kBottom: {
      CGFloat minHeight =
          IsAppBarHiddenInFullscreen() ? 0 : kAppBarHeightFullscreen;
      agent->AddObscuredInsetRange(UIRectEdgeBottom, minHeight,
                                   [self appBarHeightPortrait]);
      break;
    }
    case AppBarPosition::kLeft:
      agent->AddObscuredInsetRange(UIRectEdgeLeft, AppBarHeightLandscape(),
                                   AppBarHeightLandscape());
      break;
    case AppBarPosition::kRight:
      agent->AddObscuredInsetRange(UIRectEdgeRight, AppBarHeightLandscape(),
                                   AppBarHeightLandscape());
      break;
    case AppBarPosition::kNone:
      break;
  }
}

- (void)fullscreenWillUpdateState:(FullscreenBrowserAgent*)agent {
  if (self.layoutState.appBarLockedInFullscreen) {
    _fullscreenProgress = agent->bottom_progress();
    agent->AddObscuredInset(UIRectEdgeBottom, kAppBarHeightFullscreen);
    return;
  }

  AppBarPosition position = self.layoutState.appBarPosition;
  switch (position) {
    case AppBarPosition::kBottom: {
      _fullscreenProgress = agent->bottom_progress();
      CGFloat minHeight =
          IsAppBarHiddenInFullscreen() ? 0 : kAppBarHeightFullscreen;
      CGFloat currentHeight =
          minHeight +
          ([self appBarHeightPortrait] - minHeight) * agent->bottom_progress();
      agent->AddObscuredInset(UIRectEdgeBottom, currentHeight);
      [self updateLayout];
      // If this is inside an animation, layout immediately.
      if (!agent->animation_duration().is_zero()) {
        [self.view layoutIfNeeded];
      }
      break;
    }
    case AppBarPosition::kLeft:
      agent->AddObscuredInset(UIRectEdgeLeft, AppBarHeightLandscape());
      break;
    case AppBarPosition::kRight:
      agent->AddObscuredInset(UIRectEdgeRight, AppBarHeightLandscape());
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
  CGFloat progress = _fullscreenProgress;
  if (self.layoutState.appBarLockedInFullscreen) {
    progress = 0.0;
  }
  self.view.appBarLockedInFullscreen =
      self.layoutState.appBarLockedInFullscreen;
  self.view.fullscreenProgress = progress;
  self.view.appBarPosition = position;
  [_appBar updateForAngle:-angle];
  [self updateCutoutRadius:self.layoutState.assistantContainerCutoutRadius];
}

- (void)updateCutoutRadius:(CGFloat)cutoutRadius {
  CGFloat clampedRadius = kAppBarCornerRadius;
  if (self.layoutState.appBarPosition == AppBarPosition::kBottom &&
      self.layoutState.toolbarPosition == ToolbarPosition::kTop) {
    clampedRadius =
        std::clamp(cutoutRadius, kAppBarCornerRadius, kAppBarCornerRadiusMax);
  }
  [_appBar updateCornerRadius:clampedRadius];
}

// Updates the layout and triggers a redraw of the view.
- (void)updateAndApplyLayout {
  [self updateLayout];
  [self.view setNeedsLayout];
  [self.view layoutIfNeeded];
}

- (CGFloat)appBarHeightPortrait {
  return [_appBar currentAppBarHeightPortrait];
}

@end
