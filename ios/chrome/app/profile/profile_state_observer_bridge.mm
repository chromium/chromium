// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_state_observer_bridge.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/memory/raw_ptr.h"

@implementation ProfileStateObserverBridge {
  raw_ptr<ProfileStateObserver> _observer;
}

- (instancetype)initWithObserver:(ProfileStateObserver*)observer {
  if ((self = [super init])) {
    CHECK(observer);
    _observer = observer;
  }
  return self;
}

- (void)resetObserver {
  _observer = nullptr;
}

- (void)dealloc {
  CHECK(!_observer) << "-resetObserver must be called before -dealloc";
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
      sceneConnected:(SceneState*)sceneState {
  if (!_observer) {
    return;
  }
  _observer->OnSceneConnected(profileState, sceneState);
}

- (void)profileState:(ProfileState*)profileState
    sceneDisconnected:(SceneState*)sceneState {
  if (!_observer) {
    return;
  }
  _observer->OnSceneDisconnected(profileState, sceneState);
}

- (void)profileState:(ProfileState*)profileState
    firstSceneHasInitializedUI:(SceneState*)sceneState {
  if (!_observer) {
    return;
  }
  _observer->OnFirstSceneHasInitializedUI(profileState, sceneState);
}

- (void)profileState:(ProfileState*)profileState
    sceneDidBecomeActive:(SceneState*)sceneState {
  if (!_observer) {
    return;
  }
  _observer->OnSceneDidBecomeActive(profileState, sceneState);
}

- (void)profileState:(ProfileState*)profileState
    willTransitionToInitStage:(ProfileInitStage)nextInitStage
                fromInitStage:(ProfileInitStage)fromInitStage {
  if (!_observer) {
    return;
  }
  _observer->OnProfileStateWillTransitionToInitStage(
      profileState, nextInitStage, fromInitStage);
}

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (!_observer) {
    return;
  }
  _observer->OnProfileStateDidTransitionToInitStage(profileState, nextInitStage,
                                                    fromInitStage);
}

@end
