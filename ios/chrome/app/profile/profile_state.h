// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

@protocol ProfileStateAgent;

// Represents the state for a single Profile and responds to the state
// changes and system events.
@interface ProfileState : NSObject

// Profile initialisation stage.
@property(nonatomic, assign) ProfileInitStage initStage;

// The non-incognito ChromeBrowserState used for this Profile. This will be null
// until `initStage` >= `InitStageProfileLoaded`.
@property(nonatomic, assign) ChromeBrowserState* browserState;

// All agents that have been attached. Use -addAgent: and -removeAgent: to
// add and remove agents.
@property(nonatomic, readonly) NSArray<id<ProfileStateAgent>>* connectedAgents;

// Adds a new agent. Agents are owned by the profile state.
// This automatically sets the profile state on the `agent`.
- (void)addAgent:(id<ProfileStateAgent>)agent;

// Removes an agent.
- (void)removeAgent:(id<ProfileStateAgent>)agent;

@end

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_
