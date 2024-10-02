// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/variations_app_state_agent.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/field_trial.h"
#import "base/metrics/histogram_functions.h"
#import "base/rand_util.h"
#import "base/time/time.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/variations/service/variations_field_trial_creator.h"
#import "components/variations/service/variations_service_utils.h"
#import "components/variations/variations_seed_store.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/launch_screen_view_controller.h"
#import "ios/chrome/app/variations_app_state_agent+testing.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_fetcher.h"
#import "ios/chrome/browser/variations/model/ios_chrome_variations_seed_store.h"

// Name of trial and experiment groups.
const char kIOSChromeVariationsTrialName[] = "kIOSChromeVariationsTrial";
const char kIOSChromeVariationsTrialDefaultGroup[] = "Default";
const char kIOSChromeVariationsTrialControlGroup[] = "Control-v1";
const char kIOSChromeVariationsTrialEnabledGroup[] = "Enabled-v1";
// Histogram name for seed expiry.
const char kIOSSeedExpiryHistogram[] = "IOS.Variations.CreateTrials.SeedExpiry";

namespace {

using ::variations::HasSeedExpiredSinceTime;
using ::variations::SeedApplicationStage;
using ::variations::VariationsSeedExpiry;
using ::variations::VariationsSeedStore;

// The NSUserDefault key to store the time the last seed is fetched.
NSString* kLastVariationsSeedFetchTimeKey = @"kLastVariationsSeedFetchTime";

// Local state key of experiment group assigned, persisted for subsequent runs.
const char kFirstRunSeedFetchExperimentGroupPref[] = "ios.variations.first_run";

// Experiment group for the iOS variations trial. It will correspond to the
// trial group activated in the respective FieldTrial object, once the latter is
// setup.
enum class IOSChromeVariationsGroup {
  kNotAssigned = 0,
  kNotFirstRun,
  kDefault,
  kControl,
  kEnabled,
};

#pragma mark - Helpers

// Returns the fetch time of the variations seed store fetched by a previous
// run, and null if such seed doesn't exist.
base::Time GetLastVariationsSeedFetchTime() {
  double timestamp = [[NSUserDefaults standardUserDefaults]
      doubleForKey:kLastVariationsSeedFetchTimeKey];
  return base::Time::FromSecondsSinceUnixEpoch(timestamp);
}

// Records metric for `kIOSSeedExpiryHistogram` according whether there is a
// seed in the variations seed store fetched by a previous run, and if there is,
// whether it is expired.
void RecordSeedExpiry(base::Time time) {
  VariationsSeedExpiry expiry;
  if (time.is_null()) {
    expiry = VariationsSeedExpiry::kFetchTimeMissing;
  } else if (HasSeedExpiredSinceTime(time)) {
    expiry = VariationsSeedExpiry::kExpired;
  } else {
    expiry = VariationsSeedExpiry::kNotExpired;
  }
  base::UmaHistogramEnumeration(kIOSSeedExpiryHistogram, expiry);
}

// Creates and returns a one-time randomized trial group assignment with regards
// to given group weights.
// NOTE: `enabled_weight` and `control_weight` should be the same unless
// overriden by test cases.
IOSChromeVariationsGroup CreateOneTimeExperimentGroupAssignment(
    int enabled_weight,
    int control_weight) {
  DCHECK_LE(enabled_weight + control_weight, 100);
  double rand = base::RandDouble() * 100;
  if (rand < enabled_weight) {
    return IOSChromeVariationsGroup::kEnabled;
  } else if (rand < enabled_weight + control_weight) {
    return IOSChromeVariationsGroup::kControl;
  } else {
    return IOSChromeVariationsGroup::kDefault;
  }
}

// Creates and activates the client side field trial. First run users would be
// assigned to a group that corresponds to the parameter `group`, while others
// would be assigned to their previous groups in the same version of the
// experiment, if exists, or be excluded out of the experiment. This is called
// when local state is ready, and will save the field trial group name in the
// local state as well.
void ActivateFieldTrialForGroup(IOSChromeVariationsGroup group) {
  // Check if the group name exists in the local state.
  // This is to cover the scenario when the app has been previously installed
  // but crashes before first run experience completes. In this case, the seed
  // would be fetched but not used by variations service, and so the field trial
  // group would be assigned to the previous one.
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  std::string group_name;
  switch (group) {
    case IOSChromeVariationsGroup::kNotAssigned:
      NOTREACHED_IN_MIGRATION();
      break;
    case IOSChromeVariationsGroup::kNotFirstRun:
      // First run completed before the experiment is setup. Use group
      // name from previous launches if exists, or leave empty if not.
      group_name =
          local_state->GetString(kFirstRunSeedFetchExperimentGroupPref);
      break;
    case IOSChromeVariationsGroup::kEnabled:
      group_name = kIOSChromeVariationsTrialEnabledGroup;
      break;
    case IOSChromeVariationsGroup::kControl:
      group_name = kIOSChromeVariationsTrialControlGroup;
      break;
    case IOSChromeVariationsGroup::kDefault:
      group_name = kIOSChromeVariationsTrialDefaultGroup;
      break;
  }
  local_state->SetString(kFirstRunSeedFetchExperimentGroupPref, group_name);
  if (!group_name.empty()) {
    base::FieldTrial* trial = base::FieldTrialList::CreateFieldTrial(
        kIOSChromeVariationsTrialName, group_name);
    trial->Activate();
  }
}

// Retrieves the time the last variations seed is fetched from local state, and
// stores it into NSUserDefaults. It should be executed every time before the
// app shuts down, so the value could be used for the next startup, before
// PrefService is instantiated.
void SaveFetchTimeOfLatestSeedInLocalState() {
  PrefService* local_state = GetApplicationContext()->GetLocalState();
  const base::Time seed_fetch_time =
      variations::VariationsSeedStore::GetLastFetchTimeFromPrefService(
          local_state);
  if (!seed_fetch_time.is_null()) {
    [[NSUserDefaults standardUserDefaults]
        setDouble:seed_fetch_time.InSecondsFSinceUnixEpoch()
           forKey:kLastVariationsSeedFetchTimeKey];
  }
}

}  // namespace

#pragma mark - VariationsAppStateAgent

@interface VariationsAppStateAgent () <IOSChromeVariationsSeedFetcherDelegate> {
  // Caches the previous activation level.
  SceneActivationLevel _previousActivationLevel;
  // Whether the variations seed fetch has completed.
  BOOL _seedFetchCompleted;
  // Whether the extended launch screen is shown.
  BOOL _extendedLaunchScreenShown;
  // The fetcher object used to fetch the seed.
  IOSChromeVariationsSeedFetcher* _fetcher;
  // Group assignment of the iOS variations trial.
  IOSChromeVariationsGroup _group;
}

@end

@implementation VariationsAppStateAgent

- (instancetype)init {
  // Note: `ShouldPresentFirstRunExperience()` will return YES as long as the
  // user has not completed a first run experience.
  return [self
      initWithFirstRunExperience:ShouldPresentFirstRunExperience()
               lastSeedFetchTime:GetLastVariationsSeedFetchTime()
                         fetcher:[[IOSChromeVariationsSeedFetcher alloc] init]
              enabledGroupWeight:100
              controlGroupWeight:0];
}

- (instancetype)initWithFirstRunExperience:(BOOL)shouldPresentFRE
                         lastSeedFetchTime:(base::Time)lastSeedFetchTime
                                   fetcher:
                                       (IOSChromeVariationsSeedFetcher*)fetcher
                        enabledGroupWeight:(int)enabledGroupWeight
                        controlGroupWeight:(int)controlGroupWeight {
  DCHECK_LE(enabledGroupWeight + controlGroupWeight, 100);
  self = [super init];
  if (self) {
    _previousActivationLevel = SceneActivationLevelUnattached;
    _seedFetchCompleted = NO;
    _extendedLaunchScreenShown = NO;
    // By checking last fetch time from NSUserDefaults, `firstRunStatus` covers
    // the scenario when a user relaunches after existing the app during FRE;
    // however, if the app crashes during FRE, the value will still be YES in
    // the subsequent launch.
    // TODO(crbug.com/40241640): Import crash helper and take into account
    // previous crash statistics into account.
    BOOL firstRun = shouldPresentFRE && lastSeedFetchTime.is_null();
    _group = firstRun ? CreateOneTimeExperimentGroupAssignment(
                            enabledGroupWeight, controlGroupWeight)
                      : IOSChromeVariationsGroup::kNotFirstRun;
    RecordSeedExpiry(lastSeedFetchTime);
    if (_group == IOSChromeVariationsGroup::kEnabled) {
      _fetcher = fetcher;
      _fetcher.delegate = self;
      [_fetcher startSeedFetch];
    }
  }
  return self;
}

+ (void)registerLocalState:(PrefRegistrySimple*)registry {
  registry->RegisterStringPref(kFirstRunSeedFetchExperimentGroupPref,
                               std::string());
}

#pragma mark - AppAgentObserver

- (void)appState:(AppState*)appState
    willTransitionToInitStage:(AppInitStage)nextInitStage {
  if (self.appState.initStage ==
      AppInitStage::kBrowserObjectsForBackgroundHandlers) {
    // Records whether the fetched seed for first run has been applied, and if
    // not, which stage has the seed application process reached.
    //
    // Note that this check is used to makes sure this metric only gets logged
    // on first run, so that subsequent runs in the `Enabled` group would not
    // contaminate the data. There is NO field trial group for `kNotFirstRun`.
    if (_group != IOSChromeVariationsGroup::kNotFirstRun) {
      base::UmaHistogramEnumeration(
          "IOS.Variations.FirstRun.SeedApplicationStage",
          [IOSChromeVariationsSeedStore seedApplicationStage]);
    }
    ActivateFieldTrialForGroup(_group);
  }
}

#pragma mark - ObservingAppAgent

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(AppInitStage)previousInitStage {
  if (self.appState.initStage == AppInitStage::kVariationsSeed) {
    // Keep waiting for the seed if the app should have variations seed fetched
    // but hasn't.
    if (_group != IOSChromeVariationsGroup::kEnabled || _seedFetchCompleted) {
      [self.appState queueTransitionToNextInitStage];
    }
  }
  [super appState:appState didTransitionFromInitStage:previousInitStage];
}

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  // If the app would be showing UI before Chrome UI is ready, extend the launch
  // screen.
  if (self.appState.initStage == AppInitStage::kVariationsSeed &&
      _group == IOSChromeVariationsGroup::kEnabled &&
      level > SceneActivationLevelBackground && !_extendedLaunchScreenShown) {
    [self showExtendedLaunchScreen:sceneState];
    _extendedLaunchScreenShown = YES;
  }
  // Saves the fetch time to NSUserDefaults when the app moves from foreground
  // to background.
  if (_previousActivationLevel > SceneActivationLevelBackground &&
      level == SceneActivationLevelBackground &&
      self.appState.initStage >
          AppInitStage::kBrowserObjectsForBackgroundHandlers) {
    SaveFetchTimeOfLatestSeedInLocalState();
  }
  _previousActivationLevel = level;
  [super sceneState:sceneState transitionedToActivationLevel:level];
}

#pragma mark - IOSChromeVariationsSeedFetcherDelegate

- (void)variationsSeedFetcherDidCompleteFetchWithSuccess:(BOOL)success {
  DCHECK_EQ(_group, IOSChromeVariationsGroup::kEnabled);
  DCHECK_LE(self.appState.initStage, AppInitStage::kVariationsSeed);
  _seedFetchCompleted = YES;
  _fetcher.delegate = nil;
  if (self.appState.initStage == AppInitStage::kVariationsSeed) {
    [self.appState queueTransitionToNextInitStage];
  }
}

#pragma mark - private

// Show a view that mocks the launch screen. This should only be called when the
// scene will be active on the foreground but the seed has not been fetched to
// initialize Chrome.
- (void)showExtendedLaunchScreen:(SceneState*)sceneState {
  DCHECK(sceneState.window);
  // Set up view controller.
  UIViewController* controller = [[LaunchScreenViewController alloc] init];
  [sceneState.window setRootViewController:controller];
  [sceneState.window makeKeyAndVisible];
}

@end
