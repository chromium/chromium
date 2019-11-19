// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_delegate.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/app/chrome_overlay_window.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SceneDelegate

- (SceneState*)sceneState {
  if (!_sceneState) {
    _sceneState = [[SceneState alloc] init];
    _sceneController = [[SceneController alloc] initWithSceneState:_sceneState];
  }
  return _sceneState;
}

#pragma mark - UIWindowSceneDelegate

// This getter is called when the SceneDelegate is created. Returning a
// ChromeOverlayWindow allows UIKit to use that as the main window for this
// scene.
- (UIWindow*)window {
  if (!_window) {
    // Sizing of the window is handled by UIKit.
    _window = [[ChromeOverlayWindow alloc] init];
  }
  return _window;
}

#pragma mark Connecting and Disconnecting the Scene

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions
    API_AVAILABLE(ios(13)) {
  self.sceneState.scene = base::mac::ObjCCastStrict<UIWindowScene>(scene);
  self.sceneState.activationLevel = SceneActivationLevelBackground;
}

- (void)sceneDidDisconnect:(UIScene*)scene API_AVAILABLE(ios(13)) {
  self.sceneState.activationLevel = SceneActivationLevelUnattached;
}

#pragma mark Transitioning to the Foreground

- (void)sceneWillEnterForeground:(UIScene*)scene API_AVAILABLE(ios(13)) {
  self.sceneState.activationLevel = SceneActivationLevelForegroundInactive;
}

- (void)sceneDidBecomeActive:(UIScene*)scene API_AVAILABLE(ios(13)) {
  self.sceneState.activationLevel = SceneActivationLevelForegroundActive;
}

#pragma mark Transitioning to the Background

- (void)sceneWillResignActive:(UIScene*)scene API_AVAILABLE(ios(13)) {
  self.sceneState.activationLevel = SceneActivationLevelForegroundInactive;
}

- (void)sceneDidEnterBackground:(UIScene*)scene API_AVAILABLE(ios(13)) {
  self.sceneState.activationLevel = SceneActivationLevelBackground;
}

@end
