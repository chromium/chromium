// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/safe_mode_app_state_agent.h"

#include "base/check.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SafeModeAppAgent () <AppStateObserver>

// The app state for the app.
@property(nonatomic, weak, readonly) AppState* appState;

@end

@implementation SafeModeAppAgent

#pragma mark - AppStateAgent

- (void)setAppState:(AppState*)appState {
  // This should only be called once!
  DCHECK(!_appState);

  _appState = appState;
  [appState addObserver:self];
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionToInitStage:(InitStage)initStage {
  if (initStage == InitStageStart) {
    // TODO(crbug.com/1178809): Move this responsibility to a specific agent.
    // Handle the transition out of the Start stage.
    [self.appState queueTransitionToNextInitStage];
  } else if (initStage == InitStageSafeMode) {
    // TODO(crbug.com/1178809): Trigger safe mode from here.
    [self.appState queueTransitionToNextInitStage];
  }
}

@end
