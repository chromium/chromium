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

- (void)setProfileState:(ProfileState*)profileState {
  // This should only be called once!
  DCHECK(!_profileState);
  _profileState = profileState;
  [profileState addObserver:self];
}

@end
