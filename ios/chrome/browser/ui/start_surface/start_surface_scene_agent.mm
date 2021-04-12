// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_scene_agent.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation StartSurfaceSceneAgent

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelBackground) {
    // TODO(crbug.com/1173160): Consider when to clear the session object since
    // Chrome may be closed without transiting to background, e.g. device power
    // off, then the previous session object is staled.
    NSLog(@"%@", [NSThread callStackSymbols]);
    SetStartSurfaceSessionObjectForSceneState(sceneState);
  }
}

@end
