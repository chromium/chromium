// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/profile/session_metrics_profile_agent.h"

#import "base/check.h"
#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service.h"
#import "ios/chrome/browser/metrics/model/ios_profile_session_durations_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_activation_level.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

@implementation SessionMetricsProfileAgent {
  // Timestamp recording the start of the active session. Null if the session
  // is not active (i.e. the Profile has no Scene in the foreground state).
  base::TimeTicks _sessionStartTimestamp;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  DCHECK(sceneState.profileState);
  ProfileState* profileState = sceneState.profileState;

  // The session cannot be considered as started before the UI is ready to
  // be displayed (even if a SceneState reaches the foreground during the
  // profile initialisation).
  const ProfileInitStage initStage = profileState.initStage;
  if (initStage < ProfileInitStage::kUIReady) {
    return;
  }

  switch (level) {
    case SceneActivationLevelUnattached:
      // Nothing to do, the scene is not ready yet.
      break;

    case SceneActivationLevelDisconnected:
    case SceneActivationLevelBackground:
      // Consider that the session ends when there is no connected scenes in
      // the foreground. Note that if multiple windows goes to the background
      // simultaneously (e.g. they were displayed side-by-side, and the user
      // switched to another app fullscreen), then the code will be notified
      // for each Scene reaching the background level, but the list of active
      // Scene will be empty each time. Thus ignore the spurious events by
      // also checking that the session start timestamp is not null (it will
      // be cleared by -handleSessionEnd).
      if (!_sessionStartTimestamp.is_null() &&
          profileState.foregroundScenes.count == 0) {
        [self handleSessionEnd];
      }
      break;

    case SceneActivationLevelForegroundInactive:
    case SceneActivationLevelForegroundActive:
      // When multiple windows are active, it is possible for a Scene to reach
      // the foreground while the session is already considered active. Ignore
      // the event in that case.
      if (_sessionStartTimestamp.is_null()) {
        [self handleSessionStart];
      }
      break;
  }
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  if (nextInitStage != ProfileInitStage::kUIReady) {
    return;
  }

  // If there is at least one scene in the foreground, pretend it transitioned
  // to its current activation level right now (because the event would have
  // been received before the profile initialisation was complete and ignored).
  // It does not really matter which scene is used, so pick the "first" one.
  if (SceneState* scene = profileState.foregroundScenes.firstObject) {
    [self sceneState:scene transitionedToActivationLevel:scene.activationLevel];
  }
}

#pragma mark - Private methods

- (void)handleSessionStart {
  DCHECK(self.profileState.profile);
  ProfileIOS* profile = self.profileState.profile;

  _sessionStartTimestamp = base::TimeTicks::Now();
  IOSProfileSessionDurationsServiceFactory::GetForProfile(profile)
      ->OnSessionStarted(_sessionStartTimestamp);
}

- (void)handleSessionEnd {
  DCHECK(self.profileState.profile);
  ProfileIOS* profile = self.profileState.profile;

  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta duration = now - _sessionStartTimestamp;

  // Records the session duration in two histograms, with different caps
  // to get precision for both medium and long sessions.
  base::UmaHistogramCustomTimes("Session.TotalDuration", duration,
                                base::Milliseconds(1), base::Hours(1), 50);
  base::UmaHistogramCustomTimes("Session.TotalDurationMax1Day", duration,
                                base::Milliseconds(1), base::Days(1), 50);

  _sessionStartTimestamp = base::TimeTicks();
  IOSProfileSessionDurationsServiceFactory::GetForProfile(profile)
      ->OnSessionEnded(duration);
}

@end
