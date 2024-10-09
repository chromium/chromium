// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_
#define IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

@class AppState;
@protocol ProfileStateAgent;
@protocol ProfileStateObserver;
@class SceneState;

// Represents the state for a single Profile and responds to the state
// changes and system events.
@interface ProfileState : NSObject

// The global AppState.
@property(nonatomic, weak, readonly) AppState* appState;

// Profile initialisation stage.
@property(nonatomic, assign) ProfileInitStage initStage;

// The non-incognito ProfileIOS instance.
// This will be null until `initStage` >= `ProfileInitStage::kProfileLoaded`.
@property(nonatomic, assign) ProfileIOS* profile;

// The foreground and active scene, if there is one.
@property(nonatomic, readonly) SceneState* foregroundActiveScene;

// The list of all connected scenes.
@property(nonatomic, readonly) NSArray<SceneState*>* connectedScenes;

// The list of all scenes in the foreground (even if they are not active).
@property(nonatomic, readonly) NSArray<SceneState*>* foregroundScenes;

// All agents that have been attached. Use -addAgent: and -removeAgent: to
// add and remove agents.
@property(nonatomic, readonly) NSArray<id<ProfileStateAgent>>* connectedAgents;

// The designated initializer.
- (instancetype)initWithAppState:(AppState*)appState NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

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

// Queue the transition to the next profile initialization stage.
//
// All observers will be notified about each transitions. If an observer call
// this method from a transition notification, the transition will be queued
// and performed once the in-progress transition is complete. It is an error
// to queue more than one transition at once, or to queue a transition when
// the stage is already ProfileInitStage::kFinal.
- (void)queueTransitionToNextInitStage;

@end

#endif  // IOS_CHROME_APP_PROFILE_PROFILE_STATE_H_
