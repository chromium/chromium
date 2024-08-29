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

@implementation ProfileState {
  base::WeakPtr<ChromeBrowserState> _browserState;

  // Agents attached to this profile state.
  NSMutableArray<id<ProfileStateAgent>>* _agents;

  // Observers registered with this profile state.
  ProfileStateObserverList* _observers;
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

@end
