// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_OBSERVER_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_OBSERVER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"

@class SceneState;

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
// Notifies when UIEnabled has been set to true. Is only fired once during the
// scene's life period when the scene UI has initialized.
- (void)sceneStateDidEnableUI:(SceneState*)sceneState;
// Notifies when UIEnabled has been set to false. Is only fired once during the
// scene's life period when the scene UI is tearing down.
- (void)sceneStateDidDisableUI:(SceneState*)sceneState;
// Notifies when URLContexts have been added to `URLContextsToOpen`.
- (void)sceneState:(SceneState*)sceneState
    hasPendingURLs:(NSSet<UIOpenURLContext*>*)URLContexts;
// Notifies that a new activity request has been received.
- (void)sceneState:(SceneState*)sceneState
    receivedUserActivity:(NSUserActivity*)userActivity;
// Notifies that the scene switched between incognito/normal mode.
- (void)sceneState:(SceneState*)sceneState
    isDisplayingIncognitoContent:(BOOL)incognitoContentVisible;
// Notifies that prompting to sign-in did start.
- (void)signinDidStart:(SceneState*)sceneState;
// Notifies that prompting to sign-in and the authentication flow are done.
- (void)signinDidEnd:(SceneState*)sceneState;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_SCENE_SCENE_STATE_OBSERVER_H_
