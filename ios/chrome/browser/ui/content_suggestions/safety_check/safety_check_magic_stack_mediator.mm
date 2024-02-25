// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_magic_stack_mediator.h"

#import <optional>

#import "base/memory/raw_ptr.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"

namespace {

// The Safety Check (Magic Stack) module runs (at minimum) once every 24 hours.
constexpr base::TimeDelta kSafetyCheckRunThreshold = base::Hours(24);

}  // namespace

@interface SafetyCheckMagicStackMediator () <AppStateObserver,
                                             PrefObserverDelegate,
                                             SafetyCheckManagerObserver>
@end

@implementation SafetyCheckMagicStackMediator {
  IOSChromeSafetyCheckManager* _safetyCheckManager;
  // Observer for Safety Check changes.
  std::unique_ptr<SafetyCheckObserverBridge> _safetyCheckManagerObserver;
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Local State prefs.
  raw_ptr<PrefService> _localState;
  AppState* _appState;
  // Used by the Safety Check (Magic Stack) module for the current Safety Check
  // state.
  SafetyCheckState* _safetyCheckState;
}

- (instancetype)initWithSafetyCheckManager:
                    (IOSChromeSafetyCheckManager*)safetyCheckManager
                                localState:(PrefService*)localState
                                  appState:(AppState*)appState {
  self = [super init];
  if (self) {
    _safetyCheckManager = safetyCheckManager;
    _localState = localState;
    _appState = appState;

    [_appState addObserver:self];

    if (!safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState)) {
      if (!_prefObserverBridge) {
        _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
      }

      _prefChangeRegistrar.Init(localState);

      // TODO(crbug.com/1481230): Stop observing
      // `kIosSettingsSafetyCheckLastRunTime` changes once the Settings Safety
      // Check is refactored to use the new Safety Check Manager.
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kIosSettingsSafetyCheckLastRunTime, &_prefChangeRegistrar);

      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult,
          &_prefChangeRegistrar);

      _safetyCheckState = [self initialSafetyCheckState];
      _safetyCheckState.commandhandler = _presentationDelegate;

      _safetyCheckManagerObserver =
          std::make_unique<SafetyCheckObserverBridge>(self, safetyCheckManager);

      if (_appState.initStage > InitStageNormalUI &&
          _appState.firstSceneHasInitializedUI &&
          _safetyCheckState.runningState == RunningSafetyCheckState::kRunning) {
        safetyCheckManager->StartSafetyCheck();
      }
    }
  }
  return self;
}

- (void)disconnect {
  _safetyCheckManagerObserver.reset();
  if (_prefObserverBridge) {
    _prefChangeRegistrar.RemoveAll();
    _prefObserverBridge.reset();
  }
  [_appState removeObserver:self];
}

- (SafetyCheckState*)safetyCheckState {
  return _safetyCheckState;
}

- (void)disableModule {
  safety_check_prefs::DisableSafetyCheckInMagicStack(_localState);
  [self.delegate removeSafetyCheckModule];
}

#pragma mark - SafetyCheckManagerObserver

- (void)passwordCheckStateChanged:(PasswordSafetyCheckState)state {
  _safetyCheckState.passwordState = state;

  std::vector<password_manager::CredentialUIEntry> insecureCredentials =
      _safetyCheckManager->GetInsecureCredentials();

  password_manager::InsecurePasswordCounts counts =
      password_manager::CountInsecurePasswordsPerInsecureType(
          insecureCredentials);

  _safetyCheckState.weakPasswordsCount = counts.weak_count;
  _safetyCheckState.reusedPasswordsCount = counts.reused_count;
  _safetyCheckState.compromisedPasswordsCount = counts.compromised_count;
}

- (void)safeBrowsingCheckStateChanged:(SafeBrowsingSafetyCheckState)state {
  _safetyCheckState.safeBrowsingState = state;
}

- (void)updateChromeCheckStateChanged:(UpdateChromeSafetyCheckState)state {
  _safetyCheckState.updateChromeState = state;
}

- (void)runningStateChanged:(RunningSafetyCheckState)state {
  _safetyCheckState.runningState = state;
  _safetyCheckState.shouldShowSeeMore = [_safetyCheckState numberOfIssues] > 2;

  if (safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState)) {
    // Safety Check can be disabled by long-pressing the module, so
    // SafetyCheckManager can still be running and returning results even after
    // disabling.
    return;
  }

  // Ensures the consumer gets the latest Safety Check state only when the
  // running state changes; this avoids calling the consumer every time an
  // individual check state changes.
  _safetyCheckState.commandhandler = self.presentationDelegate;
  [self.consumer showSafetyCheck:_safetyCheckState];
}

- (void)safetyCheckManagerWillShutdown {
  _safetyCheckManagerObserver.reset();
}

#pragma mark - AppStateObserver

// Conditionally starts the Safety Check if the upcoming init stage is
// `InitStageFinal` and the Safety Check state indicates it's running.
//
// NOTE: It's safe to call `StartSafetyCheck()` multiple times, because calling
// `StartSafetyCheck()` on an already-running Safety Check is a no-op.
- (void)appState:(AppState*)appState
    willTransitionToInitStage:(InitStage)nextInitStage {
  if (!safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState) &&
      nextInitStage == InitStageFinal && appState.firstSceneHasInitializedUI &&
      _safetyCheckState.runningState == RunningSafetyCheckState::kRunning) {
    _safetyCheckManager->StartSafetyCheck();
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kIosSettingsSafetyCheckLastRunTime ||
      preferenceName == prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult) {
    _safetyCheckState.lastRunTime = [self latestSafetyCheckRunTimestamp];

    _safetyCheckState.safeBrowsingState =
        SafeBrowsingSafetyCheckStateForName(
            _localState->GetString(
                prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult))
            .value_or(_safetyCheckState.safeBrowsingState);

    // Trigger a module update when the Last Run Time, or Safe Browsing state,
    // has changed.
    [self runningStateChanged:_safetyCheckState.runningState];
  }
}

#pragma mark - Private

// Creates the initial `SafetyCheckState` based on the previous check states
// stored in Prefs, or (for development builds) the overridden check states via
// Experimental settings.
- (SafetyCheckState*)initialSafetyCheckState {
  SafetyCheckState* state = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];

  // Update Chrome check.
  std::optional<UpdateChromeSafetyCheckState> overrideUpdateChromeState =
      experimental_flags::GetUpdateChromeSafetyCheckState();

  state.updateChromeState = overrideUpdateChromeState.value_or(
      _safetyCheckManager->GetUpdateChromeCheckState());

  // Password check.
  std::optional<PasswordSafetyCheckState> overridePasswordState =
      experimental_flags::GetPasswordSafetyCheckState();

  state.passwordState = overridePasswordState.value_or(
      _safetyCheckManager->GetPasswordCheckState());

  // Safe Browsing check.
  std::optional<SafeBrowsingSafetyCheckState> overrideSafeBrowsingState =
      experimental_flags::GetSafeBrowsingSafetyCheckState();

  state.safeBrowsingState = overrideSafeBrowsingState.value_or(
      _safetyCheckManager->GetSafeBrowsingCheckState());

  // Insecure credentials.
  std::optional<int> overrideWeakPasswordsCount =
      experimental_flags::GetSafetyCheckWeakPasswordsCount();

  std::optional<int> overrideReusedPasswordsCount =
      experimental_flags::GetSafetyCheckReusedPasswordsCount();

  std::optional<int> overrideCompromisedPasswordsCount =
      experimental_flags::GetSafetyCheckCompromisedPasswordsCount();

  bool passwordCountsOverride = overrideWeakPasswordsCount.has_value() ||
                                overrideReusedPasswordsCount.has_value() ||
                                overrideCompromisedPasswordsCount.has_value();

  // NOTE: If any password counts are overriden via Experimental
  // settings, all password counts will be considered overriden.
  if (passwordCountsOverride) {
    state.weakPasswordsCount = overrideWeakPasswordsCount.value_or(0);
    state.reusedPasswordsCount = overrideReusedPasswordsCount.value_or(0);
    state.compromisedPasswordsCount =
        overrideCompromisedPasswordsCount.value_or(0);
  } else {
    std::vector<password_manager::CredentialUIEntry> insecureCredentials =
        _safetyCheckManager->GetInsecureCredentials();

    password_manager::InsecurePasswordCounts counts =
        password_manager::CountInsecurePasswordsPerInsecureType(
            insecureCredentials);

    state.weakPasswordsCount = counts.weak_count;
    state.reusedPasswordsCount = counts.reused_count;
    state.compromisedPasswordsCount = counts.compromised_count;
  }

  state.lastRunTime = [self latestSafetyCheckRunTimestamp];

  state.runningState = CanRunSafetyCheck(state.lastRunTime)
                           ? RunningSafetyCheckState::kRunning
                           : RunningSafetyCheckState::kDefault;

  return state;
}

// Returns the last run time of the Safety Check, regardless if the check was
// started from the Safety Check (Magic Stack) module, or the Safety Check
// Settings UI.
- (std::optional<base::Time>)latestSafetyCheckRunTimestamp {
  base::Time lastRunTimeViaModule =
      _safetyCheckManager->GetLastSafetyCheckRunTime();

  base::Time lastRunTimeViaSettings =
      _localState->GetTime(prefs::kIosSettingsSafetyCheckLastRunTime);

  // Use the most recent Last Run Time—regardless of where the Safety Check was
  // run—to minimize user confusion.
  base::Time lastRunTime = lastRunTimeViaModule > lastRunTimeViaSettings
                               ? lastRunTimeViaModule
                               : lastRunTimeViaSettings;

  base::TimeDelta lastRunAge = base::Time::Now() - lastRunTime;

  // Only return the Last Run Time if the run happened within the last 24hr.
  return lastRunAge <= kSafetyCheckRunThreshold
             ? std::optional<base::Time>(lastRunTime)
             : std::nullopt;
}

@end
