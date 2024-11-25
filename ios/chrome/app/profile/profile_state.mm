// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_state.h"

#import "base/check.h"
#import "base/ios/crb_protocol_observers.h"
#import "base/memory/weak_ptr.h"
#import "base/types/cxx23_to_underlying.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/deferred_initialization_queue.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/profile/profile_state_agent.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

#pragma mark - ProfileStateObserverList

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

#pragma mark - UIBlockerManagerObserverList

@interface UIBlockerManagerObservers
    : CRBProtocolObservers <UIBlockerManagerObserver>
@end

@implementation UIBlockerManagerObservers
@end

#pragma mark - ProfileState

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

  // The current blocker target if any.
  id<UIBlockerTarget> _uiBlockerTarget;

  // The counter of currently shown blocking UIs. Do not use this directly,
  // instead use incrementBlockingUICounterForScene: and
  // incrementBlockingUICounterForScene or the ScopedUIBlocker.
  NSUInteger _blockingUICounter;

  // Container for observers.
  UIBlockerManagerObservers* _uiBlockerManagerObservers;
}

#pragma mark - NSObject

- (instancetype)initWithAppState:(AppState*)appState {
  if ((self = [super init])) {
    _appState = appState;
    _agents = [[NSMutableArray alloc] init];
    _connectedSceneStates = [[NSMutableArray alloc] init];
    _observers = [ProfileStateObserverList observers];
    _uiBlockerManagerObservers = [UIBlockerManagerObservers
        observersWithProtocol:@protocol(UIBlockerManagerObserver)];
    _deferredRunner = [[DeferredInitializationRunner alloc]
        initWithQueue:[DeferredInitializationQueue sharedInstance]];
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
  for (SceneState* sceneState in _connectedSceneStates) {
    if (sceneState.activationLevel == SceneActivationLevelForegroundActive) {
      return sceneState;
    }
  }

  return nil;
}

- (NSArray<SceneState*>*)connectedScenes {
  return [_connectedSceneStates copy];
}

- (NSArray<SceneState*>*)foregroundScenes {
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
}

- (id<StartupInformation>)startupInformation {
  return _appState.startupInformation;
}

- (void)setUiBlockerTarget:(id<UIBlockerTarget>)uiBlockerTarget {
  _uiBlockerTarget = uiBlockerTarget;
  for (SceneState* scene in _connectedSceneStates) {
    // When there's a scene with blocking UI, all other scenes should show the
    // overlay.
    BOOL shouldPresentOverlay =
        (uiBlockerTarget != nil) && (scene != uiBlockerTarget);
    scene.presentingModalOverlay = shouldPresentOverlay;
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

  if (_initStage > ProfileInitStage::kStart &&
      [observer respondsToSelector:@selector
                (profileState:didTransitionToInitStage:fromInitStage:)]) {
    const ProfileInitStage prevStage =
        static_cast<ProfileInitStage>(base::to_underlying(_initStage) - 1);

    // Trigger an update on the newly added observer.
    [observer profileState:self
        didTransitionToInitStage:_initStage
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
  [_observers profileState:self sceneConnected:sceneState];
}

- (void)queueTransitionToNextInitStage {
  // TODO(crbug.com/353683675): once ProfileInitStage and AppInitStage
  // have been decoupled, then this method should only update the current
  // object. Until then forward the call to AppState if the object is the
  // "main" profile. This allow converting incrementally the AppAgents to
  // ProfileStateAgents.
  if (_appState.mainProfile == self) {
    [_appState queueTransitionToNextInitStage];
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
      [_observers profileState:self sceneDidBecomeActive:sceneState];
      break;
  }
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  DCHECK_GE(_initStage, ProfileInitStage::kPrepareUI);
  if (!_firstSceneHasInitializedUI) {
    _firstSceneHasInitializedUI = YES;
    [_observers profileState:self firstSceneHasInitializedUI:sceneState];
  }
}

#pragma mark - UIBlockerManager

- (void)incrementBlockingUICounterForTarget:(id<UIBlockerTarget>)target {
  CHECK(_uiBlockerTarget == nil || target == _uiBlockerTarget)
      << "Another scene is already showing a blocking UI!";
  _blockingUICounter++;
  [self setUiBlockerTarget:target];
}

- (void)decrementBlockingUICounterForTarget:(id<UIBlockerTarget>)target {
  CHECK_GT(_blockingUICounter, 0u);
  CHECK_EQ(_uiBlockerTarget, target);
  if (--_blockingUICounter == 0) {
    [self setUiBlockerTarget:nil];
    [_uiBlockerManagerObservers currentUIBlockerRemoved];
  }
}

- (id<UIBlockerTarget>)currentUIBlocker {
  if (_appState.currentUIBlocker) {
    return _appState.currentUIBlocker;
  }
  return _uiBlockerTarget;
}

- (void)addUIBlockerManagerObserver:(id<UIBlockerManagerObserver>)observer {
  [_uiBlockerManagerObservers addObserver:observer];
}

- (void)removeUIBlockerManagerObserver:(id<UIBlockerManagerObserver>)observer {
  [_uiBlockerManagerObservers removeObserver:observer];
}

@end
