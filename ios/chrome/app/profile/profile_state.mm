// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/profile_state.h"

#import "base/check.h"
#import "base/memory/weak_ptr.h"
#import "ios/chrome/app/profile/profile_state_agent.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"

@implementation ProfileState {
  base::WeakPtr<ChromeBrowserState> _browserState;
  // Agents attached to this app state.
  NSMutableArray<id<ProfileStateAgent>>* _agents;
}

#pragma mark - NSObject

- (instancetype)init {
  if ((self = [super init])) {
    _agents = [[NSMutableArray alloc] init];
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

@end
