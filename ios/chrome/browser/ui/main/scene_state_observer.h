// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_SCENE_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_SCENE_STATE_OBSERVER_H_

#import <UIKit/UIKit.h>

@class SceneState;
enum SceneActivationLevel : NSUInteger;

// Observer for a SceneState.
@protocol SceneStateObserver <NSObject>

@optional

// Called whenever the scene state transitions between different activity
// states.
- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level;

// Notifies when presentingModalOverlay is being set to true.
- (void)sceneStateWillShowModalOverlay:(SceneState*)sceneState;
// Notifies when presentingModalOverlay is being set to false.
- (void)sceneStateWillHideModalOverlay:(SceneState*)sceneState;
// Notifies when presentingModalOverlay has been set to false.
- (void)sceneStateDidHideModalOverlay:(SceneState*)sceneState;
// Notifies when hasInitializedUI has been set.
- (void)sceneStateHasInitializedUI:(SceneState*)sceneState;
// Notifies when URLContexts have been added to |URLContextsToOpen|.
- (void)sceneState:(SceneState*)sceneState
    hasPendingURLs:(NSSet<UIOpenURLContext*>*)URLContexts;
// Notifies that a new activity request has been received.
- (void)sceneState:(SceneState*)sceneState
    receivedUserActivity:(NSUserActivity*)userActivity;
// Notifies that the scene switched between incognito/normal mode.
- (void)sceneState:(SceneState*)sceneState
    isDisplayingIncognitoContent:(BOOL)incognitoContentVisible;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_SCENE_STATE_OBSERVER_H_
