// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/observing_profile_agent.h"

#import "base/check.h"
#import "ios/chrome/app/profile/profile_state.h"

@implementation ObservingProfileAgent

+ (instancetype)agentFromProfile:(ProfileState*)profileState {
  for (id agent in profileState.connectedAgents) {
    if ([agent isMemberOfClass:[self class]]) {
      return agent;
    }
  }

  return nil;
}

// This method is called when -addAgent: and -removeAgent: are called on
// a ProfileState. Automatically register/unregister self as an observer.
- (void)setProfileState:(ProfileState*)profileState {
  DCHECK(!_profileState || !profileState);
  if (_profileState) {
    [_profileState removeObserver:self];
  }

  _profileState = profileState;

  if (_profileState) {
    [_profileState addObserver:self];
  }
}

@end
