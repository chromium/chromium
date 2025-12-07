// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_OBSERVING_PROFILE_AGENT_H_
#define IOS_CHROME_APP_PROFILE_OBSERVING_PROFILE_AGENT_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/profile_state_agent.h"
#import "ios/chrome/app/profile/profile_state_observer.h"

@class ProfileState;

// A profile agent that acts as a profile state observer.
// Since most profile agents are also profile state observer, this is a
// convenience base class that provides universally useful functionality for
// profile agents.
@interface ObservingProfileAgent
    : NSObject <ProfileStateAgent, ProfileStateObserver>

// Returns the agent of this class iff one is already added to `profileState`.
+ (instancetype)agentFromProfile:(ProfileState*)profileState;

// Profile state this agent serves and observes.
@property(nonatomic, weak) ProfileState* profileState;

@end

#endif  // IOS_CHROME_APP_PROFILE_OBSERVING_PROFILE_AGENT_H_
