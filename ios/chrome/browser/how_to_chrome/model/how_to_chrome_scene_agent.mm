// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/how_to_chrome/model/how_to_chrome_scene_agent.h"

#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/metrics/user_metrics.h"
#import "base/time/time.h"

@implementation HowToChromeSceneAgent {
  // The callback registered with `base::AddActionCallback`.
  base::ActionCallback _actionCallback;
}

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  if (sceneState.activationLevel >= SceneActivationLevelForegroundActive) {
    [self startListening];
  }
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level >= SceneActivationLevelForegroundActive) {
    [self startListening];
  } else {
    [self stopListening];
  }
}

- (void)startListening {
  if (_actionCallback) {
    return;
  }
  __weak HowToChromeSceneAgent* weakSelf = self;
  _actionCallback = base::BindRepeating(
      ^(const std::string& action, base::TimeTicks action_time) {
        [weakSelf onUserAction:action];
      });
  base::AddActionCallback(_actionCallback);
}

- (void)stopListening {
  base::RemoveActionCallback(_actionCallback);
  _actionCallback.Reset();
}

- (void)dealloc {
  [self stopListening];
}

- (void)onUserAction:(const std::string&)action {
}

@end
