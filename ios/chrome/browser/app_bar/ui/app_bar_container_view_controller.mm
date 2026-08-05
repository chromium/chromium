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
#import "ios/chrome/browser/shared/coordinator/scene/state/browser_layout_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/scene_layout_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_constants.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

@interface AppBarContainerViewController () <AppBarContainerViewDelegate,
                                             BrowserLayoutStateObserver,
                                             IncognitoStateObserver,
                                             SceneLayoutStateObserver>
@property(nonatomic, readonly, weak)
    BrowserLayoutState* currentBrowserLayoutState;
@property(nonatomic, strong) AppBarContainerView* view;
@end

@implementation AppBarContainerViewController {
  AppBarViewController* _appBar;
  // The last fullscreen progress value received.
  CGFloat _fullscreenProgress;
}

#pragma mark - Properties

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

- (void)setRegularBrowserLayoutState:
    (BrowserLayoutState*)regularBrowserLayoutState {
  if (_regularBrowserLayoutState == regularBrowserLayoutState) {
    return;
  }
  [_regularBrowserLayoutState removeObserver:self];
  _regularBrowserLayoutState = regularBrowserLayoutState;
  [_regularBrowserLayoutState addObserver:self];
  [self updateLayout];
}

- (void)setIncognitoBrowserLayoutState:
    (BrowserLayoutState*)incognitoBrowserLayoutState {
  if (_incognitoBrowserLayoutState == incognitoBrowserLayoutState) {
    return;
  }
  [_incognitoBrowserLayoutState removeObserver:self];
  _incognitoBrowserLayoutState = incognitoBrowserLayoutState;
  [_incognitoBrowserLayoutState addObserver:self];
  [self updateLayout];
}

- (void)setIncognitoState:(IncognitoState*)incognitoState {
  if (_incognitoState == incognitoState) {
    return;
  }
  [_incognitoState removeObserver:self];
  _incognitoState = incognitoState;
  [_incognitoState addObserver:self];
  [self updateLayout];
}

- (BrowserLayoutState*)currentBrowserLayoutState {
  return self.incognitoState.incognitoContentVisible
             ? self.incognitoBrowserLayoutState
             : self.regularBrowserLayoutState;
}

- (void)setSceneLayoutState:(SceneLayoutState*)sceneLayoutState {
  if (_sceneLayoutState == sceneLayoutState) {
    return;
  }
  [_sceneLayoutState removeObserver:self];
  _sceneLayoutState = sceneLayoutState;
  [_sceneLayoutState addObserver:self];
  [self updateLayout];
}

#pragma mark - IncognitoStateObserver

- (void)willEnterIncognitoForState:(IncognitoState*)incognitoState {
  [self updateLayout];
}

- (void)willExitIncognitoForState:(IncognitoState*)incognitoState {
  [self updateLayout];
}

#pragma mark - SceneLayoutStateObserver

- (void)layoutState:(SceneLayoutState*)layoutState
    didChangeAppBarPosition:(AppBarPosition)appBarPosition {
  [self updateLayout];
}

- (void)layoutState:(SceneLayoutState*)layoutState
    didChangeAssistantContainerCutoutRadius:
        (CGFloat)assistantContainerCutoutRadius {
  [self updateCutoutRadius:assistantContainerCutoutRadius];
}

- (void)layoutState:(SceneLayoutState*)layoutState
    didChangeAssistantContainerInvoked:(BOOL)assistantContainerInvoked {
  [self updateAndApplyLayout];
}

#pragma mark - BrowserLayoutStateObserver

- (void)browserLayoutState:(BrowserLayoutState*)browserLayoutState
    didChangeToolbarPosition:(ToolbarPosition)toolbarPosition {
  [self updateLayout];
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
  if (self.sceneLayoutState.assistantContainerInvoked) {
    return;
  }
  [self updateAndApplyLayout];
}

#pragma mark - FullscreenBrowserAgentObserving

- (void)fullscreenWillUpdateObscuredInsetRange:(FullscreenBrowserAgent*)agent {
  AppBarPosition position = self.sceneLayoutState.appBarPosition;
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
  if (self.sceneLayoutState.assistantContainerInvoked &&
      !IsAppBarHiddenInFullscreen()) {
    _fullscreenProgress = agent->bottom_progress();
    agent->AddObscuredInset(UIRectEdgeBottom, kAppBarHeightFullscreen);
    return;
  }

  AppBarPosition position = self.sceneLayoutState.appBarPosition;
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

  AppBarPosition position = self.sceneLayoutState.appBarPosition;
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
  if (self.sceneLayoutState.assistantContainerInvoked &&
      !IsAppBarHiddenInFullscreen()) {
    progress = 0.0;
  }
  self.view.assistantContainerInvoked =
      self.sceneLayoutState.assistantContainerInvoked;
  self.view.fullscreenProgress = progress;
  self.view.appBarPosition = position;
  [_appBar updateForAngle:-angle];
  [self
      updateCutoutRadius:self.sceneLayoutState.assistantContainerCutoutRadius];
}

- (void)updateCutoutRadius:(CGFloat)cutoutRadius {
  CGFloat clampedRadius = kAppBarCornerRadius;
  if (self.sceneLayoutState.appBarPosition == AppBarPosition::kBottom &&
      self.currentBrowserLayoutState.toolbarPosition == ToolbarPosition::kTop) {
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
