// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/app_metrics_app_state_agent.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/metrics/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/ui/main/scene_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AppMetricsAppStateAgent () <SceneStateObserver>

// Observed app state.
@property(nonatomic, weak) AppState* appState;

@end

@implementation AppMetricsAppStateAgent

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
}

- (void)appStateDidExitSafeMode:(AppState*)appState {
  DCHECK(self.appState.lastTimeInForeground.is_null());
  // Log session start. This normally happens in
  // sceneState:transitionedToActivationLevel:, but is skipped in safe mode.
  [self handleSessionStart];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (self.appState.isInSafeMode) {
    // Don't log any metrics at safe mode. Wait for AppStateObserver's
    // -appStateDidExitSafeMode to log session start.
    return;
  }

  if (level >= SceneActivationLevelForegroundInactive &&
      self.appState.lastTimeInForeground.is_null()) {
    [self handleSessionStart];
  } else if (level <= SceneActivationLevelBackground) {
    for (SceneState* scene in self.appState.connectedScenes) {
      if (scene.activationLevel > SceneActivationLevelBackground) {
        // One scene has gone background, but at least one other is still
        // foreground. Consider the session ongoing.
        return;
      }
    }

    if (self.appState.lastTimeInForeground.is_null()) {
      // This method will be called multiple times, once per scene, if multiple
      // scenes go background simulatneously (for example, if two windows were
      // in split screen and the user swiped to go home). Only log the session
      // duration once.
      return;
    }

    [self handleSessionEnd];
    DCHECK(self.appState.lastTimeInForeground.is_null());
  }
}

#pragma mark - private

- (void)handleSessionStart {
  self.appState.lastTimeInForeground = base::TimeTicks::Now();

  IOSProfileSessionDurationsService* psdService = [self psdService];
  if (psdService) {
    psdService->OnSessionStarted(self.appState.lastTimeInForeground);
  }
}

- (void)handleSessionEnd {
  DCHECK(!self.appState.lastTimeInForeground.is_null());

  base::TimeDelta duration =
      base::TimeTicks::Now() - self.appState.lastTimeInForeground;

  UMA_HISTOGRAM_LONG_TIMES("Session.TotalDuration", duration);
  UMA_HISTOGRAM_CUSTOM_TIMES("Session.TotalDurationMax1Day", duration,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromHours(24), 50);

  IOSProfileSessionDurationsService* psdService = [self psdService];
  if (psdService) {
    psdService->OnSessionEnded(duration);
  }

  self.appState.lastTimeInForeground = base::TimeTicks();
}

- (IOSProfileSessionDurationsService*)psdService {
  if (!self.appState.mainBrowserState) {
    return nil;
  }

  return IOSProfileSessionDurationsServiceFactory::GetForBrowserState(
      self.appState.mainBrowserState);
}

@end
