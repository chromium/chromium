// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_state.h"

#import "base/check.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/memory/weak_ptr.h"
#import "base/types/cxx23_to_underlying.h"
#import "ios/chrome/app/profile/profile_state_agent.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// A sub-class of CRBProtocolObservers that declares it conforms to the
// ProfileStateObserver protocol to please the compiler as it can't see
// that CRBProtocolObservers conforms to any protocol of the registered
// observers.
@interface ProfileStateObserverList
    : CRBProtocolObservers <ProfileStateObserver>

+ (instancetype)observers;

@end

@implementation ProfileStateObserverList

+ (instancetype)observers {
  return [self observersWithProtocol:@protocol(ProfileStateObserver)];
}

@end

@interface ProfileState () <SceneStateObserver>

@end

@implementation ProfileState {
  base::WeakPtr<ChromeBrowserState> _browserState;

  // Agents attached to this profile state.
  NSMutableArray<id<ProfileStateAgent>>* _agents;

  // Observers registered with this profile state.
  ProfileStateObserverList* _observers;

  // YES if `-sceneStateDidEnableUI` been called.
  BOOL _firstSceneHasInitializedUI;

  // Set of connected scenes.
  std::set<SceneState*> _connectedSceneStates;
}

#pragma mark - NSObject

- (instancetype)init {
  if ((self = [super init])) {
    _agents = [[NSMutableArray alloc] init];
    _observers = [ProfileStateObserverList observers];
  }
  return self;
}

#pragma mark - Properties

- (ChromeBrowserState*)browserState {
  return _browserState.get();
}

- (void)setBrowserState:(ChromeBrowserState*)browserState {
  CHECK(browserState);
  _browserState = browserState->AsWeakPtr();
}

- (NSArray<id<ProfileStateAgent>>*)connectedAgents {
  return [_agents copy];
}

- (void)setInitStage:(ProfileInitStage)initStage {
  CHECK_GE(initStage, ProfileInitStage::InitStageLoadProfile);
  CHECK_LE(initStage, ProfileInitStage::InitStageFinal);

  if (initStage == ProfileInitStage::InitStageLoadProfile) {
    // Support setting the initStage to InitStageLoadProfile for startup.
    CHECK_EQ(_initStage, ProfileInitStage::InitStageLoadProfile);
  } else {
    // After InitStageLoadProfile, the init stages must be incremented by one
    // only. If a stage needs to be skipped, it can just be a no-op.
    CHECK_EQ(base::to_underlying(initStage),
             base::to_underlying(_initStage) + 1);
  }

  const ProfileInitStage fromStage = _initStage;
  [_observers profileState:self
      willTransitionToInitStage:initStage
                  fromInitStage:fromStage];

  _initStage = initStage;

  [_observers profileState:self
      didTransitionToInitStage:initStage
                 fromInitStage:fromStage];

  if (initStage == ProfileInitStage::InitStageUIReady) {
    for (SceneState* sceneState : _connectedSceneStates) {
      [_observers profileState:self sceneConnected:sceneState];
      if (sceneState.activationLevel >= SceneActivationLevelForegroundActive) {
        [_observers profileState:self sceneDidBecomeActive:sceneState];
      }
    }
    _connectedSceneStates.clear();
  }
}

#pragma mark - Public

- (void)addAgent:(id<ProfileStateAgent>)agent {
  CHECK(agent);
  CHECK(![_agents containsObject:agent]);
  [_agents addObject:agent];
  [agent setProfileState:self];
}

- (void)removeAgent:(id<ProfileStateAgent>)agent {
  CHECK(agent);
  CHECK([_agents containsObject:agent]);
  [_agents removeObject:agent];
  [agent setProfileState:nil];
}

- (void)addObserver:(id<ProfileStateObserver>)observer {
  CHECK(observer);
  [_observers addObserver:observer];

  const ProfileInitStage initStage = self.initStage;
  if (initStage > ProfileInitStage::InitStageLoadProfile &&
      [observer respondsToSelector:@selector
                (profileState:didTransitionToInitStage:fromInitStage:)]) {
    const ProfileInitStage prevStage =
        static_cast<ProfileInitStage>(base::to_underlying(initStage) - 1);

    // Trigger an update on the newly added observer.
    [observer profileState:self
        didTransitionToInitStage:initStage
                   fromInitStage:prevStage];
  }
}

- (void)removeObserver:(id<ProfileStateObserver>)observer {
  CHECK(observer);
  [_observers removeObserver:observer];
}

- (void)sceneStateConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
  _connectedSceneStates.insert(sceneState);
  if (self.initStage >= ProfileInitStage::InitStageUIReady) {
    [_observers profileState:self sceneConnected:sceneState];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (level == SceneActivationLevelForegroundActive) {
    const ProfileInitStage initStage = self.initStage;
    if (initStage >= ProfileInitStage::InitStageUIReady) {
      [_observers profileState:self sceneDidBecomeActive:sceneState];
    } else {
      _connectedSceneStates.insert(sceneState);
    }
  } else {
    _connectedSceneStates.erase(sceneState);
  }
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  DCHECK_GE(self.initStage, ProfileInitStage::InitStagePrepareUI);
  if (!_firstSceneHasInitializedUI) {
    _firstSceneHasInitializedUI = YES;
    [_observers profileState:self firstSceneHasInitializedUI:sceneState];
  }
}

@end
