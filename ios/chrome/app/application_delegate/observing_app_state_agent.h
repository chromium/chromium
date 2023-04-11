// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_OBSERVING_APP_STATE_AGENT_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_OBSERVING_APP_STATE_AGENT_H_

#import "ios/chrome/app/application_delegate/app_state_agent.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"

// An app agent that acts as a app state observer.
// Since most agents are also app state observers, this is a convenience base
// class that provides universally useful functionality for app agents.
@interface ObservingAppAgent : NSObject <AppStateAgent, AppStateObserver>

// App state this agent serves and observes.
@property(nonatomic, weak) AppState* appState;

@end

// An App Agent that observes the AppState and every connected scene.
// Provides some convenient synthetic events.
// Use this class when you want to observe some simple event that is made
// complicated because of the multiple windows, e.g. "the app is foreground".
// Extend this class with new events as necessary.
@interface SceneObservingAppAgent : ObservingAppAgent <SceneStateObserver>

// For convenience overridable events below, the events will not be delivered
// until this stage is reached. The default is Final.
@property(nonatomic, assign) InitStage minimumStageForNotifications;

// For convenience overridable events below, will call events such as "some
// scene is foreground" if the conditions are met when the minimum init stage is
// reached. The default is YES.
@property(nonatomic, assign) BOOL notifyOfPastEventsWhenMinimumStageReached;

// Overridable methods.

// Called when the app enters foreground, e.g. any scene is foreground.
// See also minimumStageForNotifications.
- (void)appDidEnterForeground;

// Called when the app enters background, e.g. no scene is foreground.
// See also minimumStageForNotifications.
- (void)appDidEnterBackground;

// Require super calls for overriden observer callbacks:
- (void)appState:(AppState*)appState
    sceneConnected:(SceneState*)sceneState NS_REQUIRES_SUPER;
- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage NS_REQUIRES_SUPER;
- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_OBSERVING_APP_STATE_AGENT_H_
