// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_protection/model/data_protection_scene_agent.h"

#import "base/scoped_observation.h"
#import "base/values.h"
#import "components/enterprise/connectors/core/connectors_prefs.h"
#import "components/enterprise/data_controls/core/browser/prefs.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service.h"
#import "ios/chrome/browser/enterprise/data_controls/model/ios_rules_service_factory.h"
#import "ios/chrome/browser/enterprise/data_controls/model/rules_service_observer_bridge.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/incognito_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/state/tab_grid_state.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/public/provider/chrome/browser/screenshot_protection/screenshot_protection_api.h"

namespace {

// Represents the current screenshot protection state applied to the window.
enum class ProtectionState {
  // Screenshot protection is currently applied to the window.
  kEnabled,
  // Screenshot protection is currently removed from the window.
  kDisabled,
};

// Returns whether real time enterprise lookups are enabled via Enterprise
// policies.
bool AreEnterpriseLookupsEnabled(const ProfileIOS& profile) {
  if (profile.IsOffTheRecord()) {
    return false;
  }

  const PrefService* prefs = profile.GetPrefs();
  CHECK(prefs);

  // Check lookups policy.
  enterprise_connectors::EnterpriseRealTimeUrlCheckMode mode =
      static_cast<enterprise_connectors::EnterpriseRealTimeUrlCheckMode>(
          prefs->GetInteger(
              enterprise_connectors::kEnterpriseRealTimeUrlCheckMode));

  return enterprise_connectors::IsEnterpriseUrlFilteringEnabled(mode);
}

}  // namespace

@interface DataProtectionSceneAgent () <IncognitoStateObserver,
                                        PrefObserverDelegate,
                                        ProfileStateObserver,
                                        RulesServiceObserving,
                                        TabGridStateObserver>
@end

@implementation DataProtectionSceneAgent {
  // Bridge to observe rules service changes.
  std::unique_ptr<data_controls::RulesServiceObserverBridge>
      _rulesObserverBridge;
  // Observation for rules service changes.
  std::unique_ptr<
      base::ScopedObservation<data_controls::RulesServiceBase,
                              data_controls::RulesServiceBase::Observer>>
      _rulesObservation;

  // Registrar for pref changes.
  PrefChangeRegistrar _prefRegistrar;
  // Bridge to observe pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;

  // The last protection state that was applied.
  ProtectionState _currentProtectionState;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _currentProtectionState = ProtectionState::kDisabled;
  }
  return self;
}

#pragma mark - ObservingSceneAgent

- (void)setSceneState:(SceneState*)sceneState {
  [super setSceneState:sceneState];
  CHECK(sceneState.profileState);
  [sceneState.tabGridState addObserver:self];
  [sceneState.profileState addObserver:self];
  [sceneState.incognitoState addObserver:self];

  // Observe prefs if profile was loaded.
  [self startObservingPrefs];
  [self updateScreenshotProtection];
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  [self updateScreenshotProtection];
}

- (void)sceneStateDidEnableUI:(SceneState*)sceneState {
  [self updateScreenshotProtection];
}

- (void)sceneStateDidDisableUI:(SceneState*)sceneState {
  // Restore the window's state.
  if (_currentProtectionState == ProtectionState::kEnabled) {
    _currentProtectionState = ProtectionState::kDisabled;
    [self applyScreenshotProtection:NO toWindow:self.sceneState.window];
  }
  [sceneState.profileState removeObserver:self];
  [sceneState.incognitoState removeObserver:self];
  [sceneState.tabGridState removeObserver:self];
  [sceneState removeObserver:self];

  [self stopObservingPrefs];
}

#pragma mark - ProfileStateObserver

- (void)profileState:(ProfileState*)profileState
    didTransitionToInitStage:(ProfileInitStage)nextInitStage
               fromInitStage:(ProfileInitStage)fromInitStage {
  // Profile might be loaded now.
  [self startObservingPrefs];
  [self updateScreenshotProtection];
}

#pragma mark - TabGridStateObserver

- (void)willEnterTabGrid {
  [self updateScreenshotProtection];
}

- (void)willExitTabGrid {
  [self updateScreenshotProtection];
}

#pragma mark - IncognitoStateObserver

- (void)willEnterIncognitoForState:(IncognitoState*)incognitoState {
  [self updateScreenshotProtection];
}

- (void)willExitIncognitoForState:(IncognitoState*)incognitoState {
  [self updateScreenshotProtection];
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  [self updateScreenshotProtection];
}

#pragma mark - RulesServiceObserving

- (void)onRulesUpdated {
  // Rules changes only trigger protection updates on the tab grid. The tab
  // helper observation drives updates while not in the tab grid.
  if (self.sceneState.tabGridState.tabGridVisible) {
    [self updateScreenshotProtection];
  }
}

#pragma mark - Private

// Whether the scene can be protected.
- (BOOL)isSceneStateReadyForProtection {
  // Only protect the scene when in the foreground, the profile has been loaded
  // and the UI is enabled.
  return self.sceneState.activationLevel >=
             SceneActivationLevelForegroundInactive &&
         self.sceneState.profileState.initStage >=
             ProfileInitStage::kProfileLoaded &&
         self.sceneState.UIEnabled;
}

// Stops tracking pref and data control rules.
- (void)stopObservingPrefs {
  _rulesObservation.reset();
  _rulesObserverBridge.reset();
  _prefObserverBridge.reset();
  _prefRegistrar.RemoveAll();
}

// Determines the current screenshot protection state and applies it to the
// scene's window. If the target state is identical to the current active state,
// this method acts as a no-op. The method is also a no-op if the scene is not
// ready yet for protection (e.g. UI is not enabled).
- (void)updateScreenshotProtection {
  if (![self isSceneStateReadyForProtection]) {
    return;
  }

  UIWindow* window = self.sceneState.window;
  if (!window) {
    return;
  }

  BOOL shouldProtect = NO;
  if (self.sceneState.tabGridState.tabGridVisible) {
    shouldProtect = [self isTabGridProtectionEnabled];
  } else {
    // TODO(crbug.com/485585995): Implement protection based on
    // DataProtectionTabHelper.
  }

  ProtectionState targetState =
      shouldProtect ? ProtectionState::kEnabled : ProtectionState::kDisabled;

  // No-op if the protection state did not change.
  if (_currentProtectionState == targetState) {
    return;
  }

  _currentProtectionState = targetState;

  [self applyScreenshotProtection:shouldProtect toWindow:window];
}

// Tracks pref and rule service changes for updating screenshot protection.
// No-op if the scene is not ready for protection. Also no-op if observers were
// already set up.
- (void)startObservingPrefs {
  if (![self isSceneStateReadyForProtection]) {
    return;
  }

  // No-op if observation was previously set up.
  if (!_prefRegistrar.IsEmpty()) {
    return;
  }

  ProfileIOS* profile = self.sceneState.profileState.profile;
  CHECK(profile);

  data_controls::IOSRulesService* rulesService =
      data_controls::IOSRulesServiceFactory::GetForProfile(profile);
  CHECK(rulesService);

  CHECK(!_rulesObserverBridge);
  CHECK(!_rulesObservation);
  _rulesObserverBridge =
      std::make_unique<data_controls::RulesServiceObserverBridge>(self);
  _rulesObservation = std::make_unique<
      base::ScopedObservation<data_controls::RulesServiceBase,
                              data_controls::RulesServiceBase::Observer>>(
      _rulesObserverBridge.get());
  _rulesObservation->Observe(rulesService);

  CHECK(!_prefObserverBridge);
  _prefRegistrar.Init(profile->GetPrefs());
  _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);

  _prefObserverBridge->ObserveChangesForPreference(
      enterprise_connectors::kEnterpriseRealTimeUrlCheckMode, &_prefRegistrar);
}

// Returns whether the overall Tab Grid should be shielded based on enterprise
// policies configured on the current active profile.
- (BOOL)isTabGridProtectionEnabled {
  CHECK([self isSceneStateReadyForProtection]);

  ProfileIOS* profile = [self currentProfile];

  data_controls::IOSRulesService* rulesService =
      data_controls::IOSRulesServiceFactory::GetForProfile(profile);
  CHECK(rulesService);
  // Protect the tab grid if there is at least one screenshot data control rule.
  // We could go through each tab checking if its URL matches a blocking rule.
  // However, we can't do the same for real time lookups as most tabs will be
  // unrealized. We chose to keep the logic simple and just check for the
  // presence of a blocking rule or lookups enabled.
  return rulesService->HasBlockingScreenshotRule() ||
         AreEnterpriseLookupsEnabled(*profile);
}

// Returns the profile corresponding to the currently active UI (Main or
// Incognito).
- (ProfileIOS*)currentProfile {
  CHECK([self isSceneStateReadyForProtection]);

  ProfileIOS* profile = self.sceneState.profileState.profile;
  CHECK(profile);

  return self.sceneState.incognitoState.incognitoContentVisible
             ? profile->GetOffTheRecordProfile()
             : profile;
}

// Applies or removes screenshot protection on the given window.
- (void)applyScreenshotProtection:(BOOL)isProtected toWindow:(UIWindow*)window {
  // Only apply a protection when the scene is ready or disable it when the UI
  // is being tear down.
  CHECK((!isProtected && !self.sceneState.UIEnabled) ||
        [self isSceneStateReadyForProtection]);
  // Applied protection must match internal state.
  CHECK(isProtected || _currentProtectionState == ProtectionState::kDisabled);

  ios::provider::ScreenshotProtectionOptions options;
  options.obfuscate_screenshots = isProtected;
  options.obfuscate_screen_recordings = isProtected;
  ios::provider::SetScreenshotProtection(window, options);
}

@end
