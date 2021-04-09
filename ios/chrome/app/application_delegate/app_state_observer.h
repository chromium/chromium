// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_OBSERVER_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_OBSERVER_H_

@class AppState;
@class UIWindow;
@class SceneState;

// App initialization stages. The app will go sequentially in-order through each
// stage each time the app is launched. This enum might expand in the future but
// the start and last stages will always keep the same label and relative
// position.
typedef NS_ENUM(NSUInteger, InitStage) {
  // The first stage when starting the initialization. The label and value of
  // this enum item should not change.
  InitStageStart = 0,
  // The app is starting the minimal basic browser services to support safe
  // mode.
  InitStageBrowserBasic,
  // The app is considering whether safe mode should be used.
  InitStageSafeMode,
  // The final stage before being done with initialization. The label and
  // relative position (always last) of this enum item should not change.
  // The value may change when inserting enum items between Start and Final.
  InitStageFinal
};

@protocol AppStateObserver <NSObject>

@optional

// Called when a scene is connected.
// On iOS 12, called when the mainSceneState is set.
- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState;

// Called when the first scene initializes its UI.
- (void)appState:(AppState*)appState
    firstSceneHasInitializedUI:(SceneState*)sceneState;

// Called after the app exits safe mode.
- (void)appStateDidExitSafeMode:(AppState*)appState;

// Called when |AppState.lastTappedWindow| changes.
- (void)appState:(AppState*)appState lastTappedWindowChanged:(UIWindow*)window;

// Called when the app is about to transition to |nextInitStage|. The init stage
// of the app at that moment is still |nextInitStage| - 1.
- (void)appState:(AppState*)appState
    willTransitionToInitStage:(InitStage)nextInitStage;

// Called right after the app is transitioned to the |initStage|.
- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_OBSERVER_H_
