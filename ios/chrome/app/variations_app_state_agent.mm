// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/variations_app_state_agent.h"

#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/ui/main/scene_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1372180): Implement
// IOSChromeFirstRunVariationsSeedManagerDelegate.
@interface VariationsAppStateAgent () {
  // Whether the variations seed has been fetched.
  BOOL _seedFetched;
}

@end

@implementation VariationsAppStateAgent

- (instancetype)init {
  self = [super init];
  if (self) {
    _seedFetched = NO;
    if ([self shouldFetchVariationsSeed]) {
      // TODO(crbug.com/1372180): start seed fetch and a timeout timer.
    }
  }
  return self;
}

#pragma mark - ObservingAppAgent

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (self.appState.initStage == InitStageVariationsSeed) {
    // Keep waiting for the seed if the app should have variations seed fetched
    // but hasn't.
    if (![self shouldFetchVariationsSeed] || _seedFetched) {
      [self.appState queueTransitionToNextInitStage];
    }
  }
  // Important: do not add code after this block because its purpose is to
  // clear `self` when not needed anymore.
  if (previousInitStage == InitStageVariationsSeed) {
    // Nothing left to do; clean up.
    [self.appState removeAgent:self];
  }
  [super appState:appState didTransitionFromInitStage:previousInitStage];
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  if ([self shouldFetchVariationsSeed]) {
    DCHECK_GE(self.appState.initStage, InitStageVariationsSeed);
    if (level == SceneActivationLevelForegroundActive &&
        self.appState.initStage == InitStageVariationsSeed) {
      [self showIntermediateUI];
    }
  }
  [super sceneState:sceneState transitionedToActivationLevel:level];
}

#pragma mark - IOSChromeFirstRunVariationsSeedManagerDelegate

- (void)didFetchSeedSuccess:(BOOL)succeeded {
  DCHECK_LE(self.appState.initStage, InitStageVariationsSeed);
  _seedFetched = YES;
  if (self.appState.initStage == InitStageVariationsSeed) {
    [self.appState queueTransitionToNextInitStage];
  }
}

#pragma mark - private

// Returns whether the variations seed should be fetched.
- (BOOL)shouldFetchVariationsSeed {
  // TODO(crbug.com/1372180): return whether the app is in first run AND enabled
  // the "dynamic FRE finching" feature.
  return NO;
}

// Show a view that mocks the splash screen. This should only be called when the
// scene is active on the foreground but the seed has not been fetched to
// initialize Chrome.
- (void)showIntermediateUI {
  // TODO(crbug.com/1372180): implement this method.
}

@end
