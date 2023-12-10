// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/incognito_usage_app_state_agent.h"

#import "base/metrics/histogram_functions.h"
#import "base/time/time.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"

namespace {
// Minimum amount of time for a normal/incognito transition to be considered.
constexpr base::TimeDelta kMinimumDelay = base::Seconds(10);
}

@interface IncognitoUsageAppStateAgent () <AppStateObserver, SceneStateObserver>

// Observed app state.
@property(nonatomic, weak) AppState* appState;

@property(nonatomic, assign) BOOL incognitoContentVisible;

@property(nonatomic, assign) base::TimeTicks incognitoUsageStart;
@property(nonatomic, assign) base::TimeTicks incognitoUsageEnd;

@end

@implementation IncognitoUsageAppStateAgent

- (instancetype)init {
  self = [super init];
  if (self) {
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(applicationWillTerminate)
               name:UIApplicationWillTerminateNotification
             object:nil];
  }
  return self;
}

- (BOOL)checkIncognitoContentVisible {
  for (SceneState* scene in self.appState.connectedScenes) {
    if (scene.incognitoContentVisible &&
        scene.activationLevel >= SceneActivationLevelForegroundInactive) {
      return YES;
    }
  }
  return NO;
}

- (void)reportIncognitoUsageTime {
  DCHECK(!self.incognitoUsageStart.is_null());
  DCHECK(!self.incognitoUsageEnd.is_null());
  base::TimeDelta duration = self.incognitoUsageEnd - self.incognitoUsageStart;
  if (duration < kMinimumDelay) {
    return;
  }
  base::UmaHistogramCustomTimes("IOS.Incognito.TimeSpent", duration,
                                base::Seconds(1),
                                base::Seconds(86400 /* secs per day */), 50);
  self.incognitoUsageStart = base::TimeTicks();
  self.incognitoUsageEnd = base::TimeTicks();
}

- (void)updateIncognitoContentVisible {
  BOOL incognitoContentVisible = [self checkIncognitoContentVisible];
  if (self.incognitoContentVisible == incognitoContentVisible) {
    return;
  }

  self.incognitoContentVisible = incognitoContentVisible;
  if (incognitoContentVisible) {
    base::TimeTicks now = base::TimeTicks::Now();
    if (!self.incognitoUsageEnd.is_null() &&
        (now - self.incognitoUsageEnd) < kMinimumDelay) {
      // The pausing of incognito is too short, resume session.
      self.incognitoUsageEnd = base::TimeTicks();
    } else {
      // Incognito has been paused for a long time. This is a new session.
      if (!self.incognitoUsageEnd.is_null() &&
          !self.incognitoUsageStart.is_null() &&
          (self.incognitoUsageEnd - self.incognitoUsageStart) >=
              kMinimumDelay) {
        // There was a previous session to report.
        [self reportIncognitoUsageTime];
      }
      self.incognitoUsageStart = base::TimeTicks::Now();
    }
  } else {
    base::TimeTicks now = base::TimeTicks::Now();
    if (!self.incognitoUsageStart.is_null() &&
        (now - self.incognitoUsageStart) >= kMinimumDelay) {
      self.incognitoUsageEnd = now;
    } else {
      // This incognito session was too short.
      self.incognitoUsageStart = base::TimeTicks();
    }
  }
}

- (void)applicationWillTerminate {
  if (self.incognitoContentVisible) {
    self.incognitoUsageEnd = base::TimeTicks::Now();
  }
  if (!self.incognitoUsageEnd.is_null() &&
      !self.incognitoUsageStart.is_null() &&
      (self.incognitoUsageEnd - self.incognitoUsageStart) >= kMinimumDelay) {
    [self reportIncognitoUsageTime];
  }
}

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];

  for (SceneState* scene in appState.connectedScenes) {
    [scene addObserver:self];
  }
  [self updateIncognitoContentVisible];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState sceneConnected:(SceneState*)sceneState {
  [sceneState addObserver:self];
  [self updateIncognitoContentVisible];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if (sceneState.incognitoContentVisible) {
    [self updateIncognitoContentVisible];
  }
}

- (void)sceneState:(SceneState*)sceneState
    isDisplayingIncognitoContent:(BOOL)level {
  if (sceneState.activationLevel >= SceneActivationLevelForegroundInactive) {
    [self updateIncognitoContentVisible];
  }
}

@end
