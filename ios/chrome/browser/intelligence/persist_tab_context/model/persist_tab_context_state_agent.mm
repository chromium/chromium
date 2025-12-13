// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_state_agent.h"

#import "base/check.h"
#import "base/functional/callback.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@implementation PersistTabContextStateAgent {
  // Callback executed whenever the SceneState's activation level changes.
  // This allows the owner of this observer to be notified of and react to scene
  // transitions, such as the PersistTabContextBrowserAgent will do when the app
  // is backgrounded.
  base::RepeatingCallback<void(SceneActivationLevel)> _transitionCallback;
}

- (instancetype)initWithTransitionCallback:
    (base::RepeatingCallback<void(SceneActivationLevel)>)callback {
  self = [super init];
  if (self) {
    CHECK(callback);
    _transitionCallback = std::move(callback);
  }
  return self;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // Execute the stored callback with the new activation level.
  _transitionCallback.Run(level);
}

@end
