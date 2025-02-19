// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/promo/signin_fullscreen_promo_scene_agent.h"

@implementation SigninFullscreenPromoSceneAgent

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelForegroundActive:
    // TODO(crbug.com/394874613): Register/deregister the sign-in promo.
    case SceneActivationLevelUnattached:
    case SceneActivationLevelDisconnected:
    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive:
      break;
  }
}

@end
