// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_INCOGNITO_STATE_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_INCOGNITO_STATE_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

enum class IncognitoLockState;
@class IncognitoReauthSceneAgent;

// Protocol to observe updates from the IncognitoState.
@protocol IncognitoStateObserver <NSObject>

@optional

// Called right before entering incognito.
- (void)willEnterIncognitoForState:(IncognitoState*)incognitoState;

// Called right before exiting incognito.
- (void)willExitIncognitoForState:(IncognitoState*)incognitoState;

// Called when the authentication requirement in a given scene might have
// changed.
// TODO(crbug.com/374073829): Remove after launching Soft Lock.
- (void)didUpdateAuthenticationRequirementForState:
    (IncognitoState*)incognitoState;

// Called when the incognito lock state in a given scene might have changed.
- (void)didUpdateIncognitoLockStateForState:(IncognitoState*)incognitoState;

@end

// Represents the incognito state for a scene.
@interface IncognitoState : NSObject

// Indicates whether the incognito content is currently visible.
@property(nonatomic, assign) BOOL incognitoContentVisible;

// Returns whether incognito tabs are hidden behind a reauthentication screen,
// soft lock screen or are not hidden at all.
@property(nonatomic, assign) IncognitoLockState lockState;

// The scene state associated with this incognito state.
@property(nonatomic, weak, readonly) SceneState* sceneState;

// Returns YES when the authentication is currently required.
@property(nonatomic, assign, readonly, getter=isAuthenticationRequired)
    BOOL authenticationRequired;

// Initializes with the given scene state.
- (instancetype)initWithSceneState:(SceneState*)sceneState;

- (instancetype)init NS_UNAVAILABLE;

// Adds `observer`.
- (void)addObserver:(id<IncognitoStateObserver>)observer;
// Removes `observer`.
- (void)removeObserver:(id<IncognitoStateObserver>)observer;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_STATE_INCOGNITO_STATE_H_
