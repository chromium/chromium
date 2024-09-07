// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

@protocol ProfileStateAgent;
@protocol ProfileStateObserver;
@class SceneState;

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

// Adds an observer to this profile state. The observers will be notified about
// profile state changes per ProfileStateObserver protocol. The observer will be
// *immediately* notified about the latest profile init stage transition before
// this method returns, if any such transitions happened, by calling
// profileState:didTransitionToInitStage:fromInitStage:, .
- (void)addObserver:(id<ProfileStateObserver>)observer;

// Removes the observer. It's safe to call this at any time, including from
// ProfileStateObserver callbacks.
- (void)removeObserver:(id<ProfileStateObserver>)observer;

// Informs the profile the given `sceneState` connected.
- (void)sceneStateConnected:(SceneState*)sceneState;

@end

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_
