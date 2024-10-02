// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_OBSERVER_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_OBSERVER_H_

#import "ios/chrome/app/application_delegate/app_init_stage.h"

@class AppState;
@class SceneState;

@protocol AppStateObserver <NSObject>

@optional

// Called when a scene is connected.
- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState;

// Called when the first scene initializes its UI.
- (void)appState:(AppState*)appState
    firstSceneHasInitializedUI:(SceneState*)sceneState;

// Called when the app is about to transition to `nextInitStage`. The init stage
// of the app at that moment is still `nextInitStage` - 1.
- (void)appState:(AppState*)appState
    willTransitionToInitStage:(AppInitStage)nextInitStage;

// Called right after the app is transitioned out of to the
// `previousInitStage`. The init stage of the app at that
// moment is `previousInitStage` + 1.
- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage;

// Called when Scene with activation level SceneActivationLevelForegroundActive
// is available.
- (void)appState:(AppState*)appState
    sceneDidBecomeActive:(SceneState*)sceneState;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_OBSERVER_H_
