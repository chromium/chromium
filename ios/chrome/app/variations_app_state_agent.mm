// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/variations_app_state_agent.h"

#import "base/mac/foundation_util.h"
#import "base/time/time.h"
#import "components/variations/service/variations_service_utils.h"
#import "components/variations/variations_seed_store.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/launch_screen_view_controller.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/ui/main/scene_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The NSUserDefault key to store the time the last seed is fetched.
NSString* kLastVariationsSeedFetchTimeKey = @"kLastVariationsSeedFetchTime";

// Record Variations.SeedFreshness metric according whether there is a seed in
// the variations seed store fetched by a previous run, and if there is, whether
// it is expired.
// TODO(crbug.com/1380164): Implement this method.
void RecordSeedFreshness() {
  double timestamp = [[NSUserDefaults standardUserDefaults]
      doubleForKey:kLastVariationsSeedFetchTimeKey];
  base::Time time = base::Time::FromDoubleT(timestamp);
  if (time.is_null()) {
    // TODO(crbug.com/1380164): Seed doesn't exist. Log metric.
  } else if (variations::HasSeedExpiredSinceTime(time)) {
    // TODO(crbug.com/1380164): Seed expired. Log metric.
  } else {
    // TODO(crbug.com/1380164): Seed unexpired. Log metric.
  }
}

// Retrieves the time the last variations seed is fetched from local state, and
// stores it into NSUserDefaults. It should be executed every time before the
// app shuts down, so the value could be used for the next startup, before
// PrefService is instantiated.
void SaveFetchTimeOfLatestSeedInLocalState() {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  const base::Time seedDate =
      variations::VariationsSeedStore::GetLastFetchTimeFromPrefService(
          localState);
  if (!seedDate.is_null()) {
    [[NSUserDefaults standardUserDefaults]
        setDouble:seedDate.ToDoubleT()
           forKey:kLastVariationsSeedFetchTimeKey];
  }
}

}  // namespace

// TODO(crbug.com/1372180): Implement
// IOSChromeVariationsSeedFetcherDelegate.
@interface VariationsAppStateAgent () {
  // Caches the previous activation level.
  SceneActivationLevel _previousActivationLevel;
  // Whether the variations seed has been fetched.
  BOOL _seedFetched;
  // Whether the extended launch screen is shown.
  BOOL _extendedLaunchScreenShown;
}

@end

@implementation VariationsAppStateAgent

- (instancetype)init {
  self = [super init];
  if (self) {
    _previousActivationLevel = SceneActivationLevelUnattached;
    _seedFetched = NO;
    _extendedLaunchScreenShown = NO;
    RecordSeedFreshness();
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
  [super appState:appState didTransitionFromInitStage:previousInitStage];
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // If the app would be showing UI before Chrome UI is ready, extend the launch
  // screen.
  if (self.appState.initStage == InitStageVariationsSeed &&
      [self shouldFetchVariationsSeed] &&
      level > SceneActivationLevelBackground && !_extendedLaunchScreenShown) {
    [self showExtendedLaunchScreen:sceneState];
    _extendedLaunchScreenShown = YES;
  }
  // Saves the fetch time to NSUserDefatuls when the app moves from foreground
  // to background.
  if (_previousActivationLevel > SceneActivationLevelBackground &&
      level == SceneActivationLevelBackground &&
      self.appState.initStage >= InitStageBrowserObjectsForUI) {
    SaveFetchTimeOfLatestSeedInLocalState();
  }
  _previousActivationLevel = level;
  [super sceneState:sceneState transitionedToActivationLevel:level];
}

#pragma mark - IOSChromeVariationsSeedFetcherDelegate

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
  return self.appState.startupInformation.isFirstRun &&
         [self shouldTurnOnFeature];
}

// TODO(crbug.com/1372180): Replace this method by actual method that sets up a
// custom client side experiment and return the value.
- (BOOL)shouldTurnOnFeature {
  return NO;
}

// Show a view that mocks the launch screen. This should only be called when the
// scene is active on the foreground but the seed has not been fetched to
// initialize Chrome.
- (void)showExtendedLaunchScreen:(SceneState*)sceneState {
  DCHECK(sceneState.window);
  // Set up view controller.
  UIViewController* controller = [[LaunchScreenViewController alloc] init];
  [sceneState.window setRootViewController:controller];
  [sceneState.window makeKeyAndVisible];
}

@end
