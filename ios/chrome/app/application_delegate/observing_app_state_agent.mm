// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

#import "base/check.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@implementation ObservingAppAgent

#pragma mark - AppStateAgent

+ (instancetype)agentFromApp:(AppState*)appState {
  for (id agent in appState.connectedAgents) {
    if ([agent isMemberOfClass:[self class]]) {
      return agent;
    }
  }

  return nil;
}

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];
}

@end

#pragma mark - SceneObservingAppAgent

@interface SceneObservingAppAgent ()

// Tracks if the app has already notified that some scenes are in foreground.
// Reset when the app goes background.
@property(nonatomic, assign) BOOL notifiedForeground;
// Tracks if the app has already notified that some scenes are in background.
// Reset when the app goes foreground.
@property(nonatomic, assign) BOOL notifiedBackground;

@end

@implementation SceneObservingAppAgent

- (instancetype)init {
  self = [super init];
  if (self) {
    // The app starts with no connected scenes, so the first event should be
    // foreground.
    _notifiedBackground = YES;
  }
  return self;
}

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  [super setAppState:appState];

  // If there are already connected scenes, start observing them.
  for (SceneState* scene in self.appState.connectedScenes) {
    [scene addObserver:self];
  }
  [self notifyOfConvenienceEventsIfNecessary];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
}

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (appState.initStage == AppInitStage::kFinal) {
    [self notifyOfConvenienceEventsIfNecessary];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (self.appState.initStage < AppInitStage::kFinal) {
    return;
  }

  [self notifyOfConvenienceEventsIfNecessary];
}

- (void)notifyOfConvenienceEventsIfNecessary {
  if (self.appState.foregroundScenes.count > 0 && !self.notifiedForeground) {
    self.notifiedForeground = YES;
    self.notifiedBackground = NO;
    [self appDidEnterForeground];
  }

  if (self.appState.foregroundScenes.count == 0 && !self.notifiedBackground) {
    self.notifiedBackground = YES;
    self.notifiedForeground = NO;
    [self appDidEnterBackground];
  }
}

#pragma mark - template methods
- (void)appDidEnterForeground {
}

- (void)appDidEnterBackground {
}

@end
