// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/start_surface/start_surface_util.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The key to store the timestamp when the scene enters into background.
NSString* kStartSurfaceSceneEnterIntoBackgroundTime =
    @"StartSurfaceSceneEnterIntoBackgroundTime";

}  // namespace

bool ShouldShowStartSurfaceForSceneState(SceneState* sceneState) {
  if (!IsStartSurfaceEnabled()) {
    return NO;
  }

  NSDate* timestamp = (NSDate*)[sceneState
      sessionObjectForKey:kStartSurfaceSceneEnterIntoBackgroundTime];
  if (timestamp == nil || [[NSDate date] timeIntervalSinceDate:timestamp] <
                              GetReturnToStartSurfaceDuration()) {
    return NO;
  }

  if (sceneState.presentingFirstRunUI || sceneState.presentingModalOverlay ||
      sceneState.startupHadExternalIntent || sceneState.pendingUserActivity ||
      sceneState.incognitoContentVisible) {
    return NO;
  }

  return YES;
}

void SetStartSurfaceSessionObjectForSceneState(SceneState* sceneState) {
  if (!IsStartSurfaceEnabled()) {
    return;
  }

  [sceneState setSessionObject:[NSDate date]
                        forKey:kStartSurfaceSceneEnterIntoBackgroundTime];
}
