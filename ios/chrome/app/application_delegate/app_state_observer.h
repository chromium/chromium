// Copyright 2021 The Chromium Authors
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
  // The app is considering whether safe mode should be used. The app will stay
  // at the InitStageSafeMode stage if safe mode is needed, or will move to the
  // next stage otherwise.
  InitStageSafeMode,
  // The app is waiting for the Finch seed to be fetched on first run. The app
  // will stay at the InitStageVariationsSeed if it is the first launch after
  // installation, and the seed has not been fetched; it moves to the next stage
  // otherwise.
  InitStageVariationsSeed,
  // The app is initializing the browser objects for the background handlers.
  // In particular this creates ChromeMain instances which initialises many
  // low-level objects (such as PostTask, ChromeBrowserStateManager, named
  // threads, ApplicationContext, ...). Using the corresponding features when
  // the InitStage is below this stage is unsupported. Most likely, you want
  // all new stages to be >= InitStageBrowserObjectsForBackgroundHandlers.
  InitStageBrowserObjectsForBackgroundHandlers,
  // The app is fetching any enterprise policies. The initialization is blocked
  // on this because the policies might have an effect on later init stages.
  InitStageEnterprise,
  // The app is initializing the browser objects for the browser UI (e.g., the
  // browser state).
  InitStageBrowserObjectsForUI,
  // If there are connected scenes, the app is creating browsers and starting
  // the root coordinators. The BVCs and Tab switchers are created here. This
  // is what is considered the normal UI.
  //
  // The stage is no-op for regular startups (no FRE, no Safe Mode) in which
  // case the app will continue its transition to InitStageFinal and the UI is
  // initialized when the scene transitions to the foreground.
  InitStageNormalUI,
  // TODO(crbug.com/1198246): Decouple FRE from Browser views to be able to go
  // through this stage before InitStageNormalUI.
  // The app is considering presenting the FRE UI. Will remain in that state
  // when presenting the FRE.
  InitStageFirstRun,
  // The final stage before being done with initialization. The label and
  // relative position (always last) of this enum item should not change.
  // The value may change when inserting enum items between Start and Final.
  InitStageFinal
};

@protocol AppStateObserver <NSObject>

@optional

// Called when a scene is connected.
- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState;

// Called when the first scene initializes its UI.
- (void)appState:(AppState*)appState
    firstSceneHasInitializedUI:(SceneState*)sceneState;

// Called when `AppState.lastTappedWindow` changes.
- (void)appState:(AppState*)appState lastTappedWindowChanged:(UIWindow*)window;

// Called when the app is about to transition to `nextInitStage`. The init stage
// of the app at that moment is still `nextInitStage` - 1.
- (void)appState:(AppState*)appState
    willTransitionToInitStage:(InitStage)nextInitStage;

// Called right after the app is transitioned out of to the
// `previousInitStage`. The init stage of the app at that
// moment is `previousInitStage` + 1.
- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage;

// Called when Scene with activation level SceneActivationLevelForegroundActive
// is available.
- (void)appState:(AppState*)appState
    sceneDidBecomeActive:(SceneState*)sceneState;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_STATE_OBSERVER_H_
