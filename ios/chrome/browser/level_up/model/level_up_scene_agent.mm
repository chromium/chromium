// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_scene_agent.h"

#import <map>

#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/time/time.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/level_up/model/level_up_service.h"
#import "ios/chrome/browser/level_up/model/level_up_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@implementation LevelUpSceneAgent {
  // The callback registered with `base::AddActionCallback`.
  base::ActionCallback _actionCallback;
  // The level up service.
  raw_ptr<LevelUpService> _levelUpService;
  // Map from user action to task type for fast lookup.
  std::map<std::string, TaskType> _actionToTaskMap;
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

  ProfileIOS* profile = self.sceneState.profileState.profile;
  if (!profile) {
    return;
  }
  _levelUpService = LevelUpServiceFactory::GetForProfile(profile);

  _actionToTaskMap.clear();
  for (const auto& [type, info] : _levelUpService->GetTasks()) {
    _actionToTaskMap[info.trigger_action] = type;
  }

  __weak LevelUpSceneAgent* weakSelf = self;
  _actionCallback = base::BindRepeating(
      ^(const std::string& action, base::TimeTicks action_time) {
        [weakSelf onUserAction:action];
      });
  base::AddActionCallback(_actionCallback);
}

- (void)stopListening {
  if (!_actionCallback) {
    return;
  }
  base::RemoveActionCallback(_actionCallback);
  _actionCallback.Reset();
  _levelUpService = nullptr;
  _actionToTaskMap.clear();
}

- (void)dealloc {
  [self stopListening];
}

- (void)onUserAction:(const std::string&)action {
  if (!_levelUpService) {
    return;
  }

  auto it = _actionToTaskMap.find(action);
  if (it != _actionToTaskMap.end()) {
    _levelUpService->MarkTaskCompleted(it->second);
  }
}

@end
