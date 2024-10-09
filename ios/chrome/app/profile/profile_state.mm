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

// TODO(crbug.com/353683675): Remove once each ProfileState -initStage is
// managed separately (this requires some refactoring before it can happen).
#import "ios/chrome/app/application_delegate/app_state.h"

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
  base::WeakPtr<ProfileIOS> _profile;

  // Agents attached to this profile state.
  NSMutableArray<id<ProfileStateAgent>>* _agents;

  // List of connected scenes.
  NSMutableArray<SceneState*>* _connectedSceneStates;

  // Observers registered with this profile state.
  ProfileStateObserverList* _observers;

  // YES if `-sceneStateDidEnableUI` been called.
  BOOL _firstSceneHasInitializedUI;
}

#pragma mark - NSObject

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _appState = appState;
    _agents = [[NSMutableArray alloc] init];
    _connectedSceneStates = [[NSMutableArray alloc] init];
    _observers = [ProfileStateObserverList observers];
  }
  return self;
}

#pragma mark - Properties

- (ProfileIOS*)profile {
  return _profile.get();
}

- (void)setProfile:(ProfileIOS*)profile {
  CHECK(profile);
  _profile = profile->AsWeakPtr();
}

- (SceneState*)foregroundActiveScene {
  if (self.initStage < ProfileInitStage::kUIReady) {
    return nil;
  }

  for (SceneState* sceneState in _connectedSceneStates) {
    if (sceneState.activationLevel == SceneActivationLevelForegroundActive) {
      return sceneState;
    }
  }

  return nil;
}

- (NSArray<SceneState*>*)connectedScenes {
  if (self.initStage < ProfileInitStage::kUIReady) {
    return nil;
  }

  return [_connectedSceneStates copy];
}

- (NSArray<SceneState*>*)foregroundScenes {
  if (self.initStage < ProfileInitStage::kUIReady) {
    return nil;
  }

  NSMutableArray<SceneState*>* foregroundScenes = [[NSMutableArray alloc] init];
  for (SceneState* sceneState in _connectedSceneStates) {
    if (sceneState.activationLevel >= SceneActivationLevelForegroundInactive) {
      [foregroundScenes addObject:sceneState];
    }
  }
  return foregroundScenes;
}

- (NSArray<id<ProfileStateAgent>>*)connectedAgents {
  return [_agents copy];
}

- (void)setInitStage:(ProfileInitStage)initStage {
  CHECK_GE(initStage, ProfileInitStage::kStart);
  CHECK_LE(initStage, ProfileInitStage::kFinal);

  if (initStage == ProfileInitStage::kStart) {
    // Support setting the initStage to kStart for startup.
    CHECK_EQ(_initStage, ProfileInitStage::kStart);
  } else {
    // After kLoadProfile, the init stages must be incremented by one only. If a
    // stage needs to be skipped, it can just be a no-op.
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

  if (initStage == ProfileInitStage::kUIReady) {
    for (SceneState* sceneState in _connectedSceneStates) {
      [_observers profileState:self sceneConnected:sceneState];
      if (sceneState.activationLevel >= SceneActivationLevelForegroundActive) {
        [_observers profileState:self sceneDidBecomeActive:sceneState];
      }
    }
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
  if (initStage > ProfileInitStage::kStart &&
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
  [_connectedSceneStates addObject:sceneState];
  if (self.initStage >= ProfileInitStage::kUIReady) {
    [_observers profileState:self sceneConnected:sceneState];
  }
}

- (void)queueTransitionToNextInitStage {
  // TODO(crbug.com/353683675): once ProfileInitStage and AppInitStage
  // have been decoupled, then this method should only update the current
  // object. Until then forward the call to AppState if the object is the
  // "main" profile. This allow converting incrementally the AppAgents to
  // ProfileStateAgents.
  if (self.appState.mainProfile == self) {
    [self.appState queueTransitionToNextInitStage];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  switch (level) {
    case SceneActivationLevelUnattached:
      // Nothing to do.
      break;

    case SceneActivationLevelDisconnected:
      [_connectedSceneStates removeObject:sceneState];
      [sceneState removeObserver:self];
      break;

    case SceneActivationLevelBackground:
    case SceneActivationLevelForegroundInactive:
      // Nothing to do.
      break;

    case SceneActivationLevelForegroundActive:
      if (self.initStage >= ProfileInitStage::kUIReady) {
        [_observers profileState:self sceneDidBecomeActive:sceneState];
      }
      break;
  }
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  DCHECK_GE(self.initStage, ProfileInitStage::kPrepareUI);
  if (!_firstSceneHasInitializedUI) {
    _firstSceneHasInitializedUI = YES;
    [_observers profileState:self firstSceneHasInitializedUI:sceneState];
  }
}

@end
