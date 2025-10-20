// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/utils/utils.h"

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"

SceneState* GetEligibleSceneForSyncedSetUp(ProfileState* profile_state) {
  if (!profile_state) {
    return nil;
  }

  if (profile_state.initStage != ProfileInitStage::kFinal) {
    return nil;
  }

  if (profile_state.currentUIBlocker) {
    return nil;
  }

  SceneState* active_scene = profile_state.foregroundActiveScene;

  if (!active_scene) {
    return nil;
  }

  id<BrowserProviderInterface> provider_interface =
      active_scene.browserProviderInterface;
  id<BrowserProvider> presenting_interface =
      provider_interface.currentBrowserProvider;

  if (presenting_interface != provider_interface.mainBrowserProvider) {
    return nil;
  }

  // All preconditions met.
  return active_scene;
}
