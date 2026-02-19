// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/safety_check/coordinator/safety_check_magic_stack_mediator.h"

#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/values.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_service.h"
#import "components/safety_check/safety_check_pref_names.h"
#import "ios/chrome/app/profile/profile_init_stage.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/app/profile/profile_state_observer.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/safety_check/coordinator/safety_check_magic_stack_mediator_delegate.h"
#import "ios/chrome/browser/content_suggestions/safety_check/model/safety_check_prefs.h"
#import "ios/chrome/browser/content_suggestions/safety_check/model/safety_check_utils.h"
#import "ios/chrome/browser/content_suggestions/safety_check/public/safety_check_constants.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_audience.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_config.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_item_type.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_consumer.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"

@interface SafetyCheckMagicStackMediator () <
    ProfileStateObserver,
    PrefObserverDelegate,
    SafetyCheckAudience,
    SafetyCheckManagerObserver>
@end

@implementation SafetyCheckMagicStackMediator {
  raw_ptr<IOSChromeSafetyCheckManager, DanglingUntriaged> _safetyCheckManager;
  // Observer for Safety Check changes.
  std::unique_ptr<SafetyCheckObserverBridge> _safetyCheckManagerObserver;
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for local pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Registrar for user pref changes notifications.
  PrefChangeRegistrar _userPrefChangeRegistrar;
  // Local State prefs.
  raw_ptr<PrefService, DanglingUntriaged> _localState;
  // User prefs.
  raw_ptr<PrefService, DanglingUntriaged> _userState;
  ProfileState* _profileState;
  // Used by the Safety Check (Magic Stack) module for the current Safety Check
  // state.
  SafetyCheckConfig* _safetyCheckConfig;
}

- (instancetype)initWithSafetyCheckManager:
                    (IOSChromeSafetyCheckManager*)safetyCheckManager
                                localState:(PrefService*)localState
                                 userState:(PrefService*)userState
                              profileState:(ProfileState*)profileState {
  if ((self = [super init])) {
    _safetyCheckManager = safetyCheckManager;
    _localState = localState;
    _userState = userState;
    _profileState = profileState;
    [_profileState addObserver:self];

    if (!safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_userState)) {
      if (!_prefObserverBridge) {
        _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
      }

      _prefChangeRegistrar.Init(localState);

      // TODO(crbug.com/40930653): Stop observing
      // `kIosSettingsSafetyCheckLastRunTime` changes once the Settings Safety
      // Check is refactored to use the new Safety Check Manager.
      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kIosSettingsSafetyCheckLastRunTime, &_prefChangeRegistrar);

      _prefObserverBridge->ObserveChangesForPreference(
          prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult,
          &_prefChangeRegistrar);

      _userPrefChangeRegistrar.Init(userState);

      _prefObserverBridge->ObserveChangesForPreference(
          safety_check::prefs::kSafetyCheckHomeModuleEnabled,
          &_userPrefChangeRegistrar);

      _safetyCheckConfig = [self initialSafetyCheckState];

      if (ShouldHideSafetyCheckModuleIfNoIssues()) {
        [self updateIssueCount:[_safetyCheckConfig numberOfIssues]];
      }

      _safetyCheckManagerObserver =
          std::make_unique<SafetyCheckObserverBridge>(self, safetyCheckManager);

      if (_profileState.initStage > ProfileInitStage::kUIReady &&
          _profileState.firstSceneHasInitializedUI &&
          _safetyCheckConfig.runningState ==
              RunningSafetyCheckState::kRunning) {
        // When the Safety Check Manager can automatically trigger Safety
        // Checks, the Magic Stack should never initiate a Safety Check run.
        //
        // TODO(crbug.com/354727175): Remove `StartSafetyCheck()` from the Magic
        // Stack once the Safety Check Manager can reliably automatically
        // trigger runs.
        if (!IsSafetyCheckAutorunByManagerEnabled()) {
          safetyCheckManager->StartSafetyCheck();
        }
      }
    }
  }
  return self;
}

- (void)disconnect {
  _safetyCheckConfig.audience = nil;

  _safetyCheckManagerObserver.reset();

  if (_prefObserverBridge) {
    _prefChangeRegistrar.RemoveAll();
    _userPrefChangeRegistrar.RemoveAll();
    _prefObserverBridge.reset();
  }

  [_profileState removeObserver:self];
  _profileState = nil;
}

- (SafetyCheckConfig*)safetyCheckConfig {
  return _safetyCheckConfig;
}

- (void)disableModule {
  safety_check_prefs::DisableSafetyCheckInMagicStack(_userState);
}

- (void)reset {
  _safetyCheckConfig = [[SafetyCheckConfig alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];
  _safetyCheckConfig.audience = self;
}

#pragma mark - SafetyCheckAudience

// Called when a Safety Check item is selected by the user.
- (void)didSelectSafetyCheckItem:(SafetyCheckItemType)type {
  [self.presentationAudience didSelectSafetyCheckItem:type];
}

#pragma mark - SafetyCheckManagerObserver

- (void)passwordCheckStateChanged:(PasswordSafetyCheckState)state
           insecurePasswordCounts:(password_manager::InsecurePasswordCounts)
                                      insecurePasswordCounts {
  _safetyCheckConfig.passwordState = state;
  _safetyCheckConfig.weakPasswordsCount = insecurePasswordCounts.weak_count;
  _safetyCheckConfig.reusedPasswordsCount = insecurePasswordCounts.reused_count;
  _safetyCheckConfig.compromisedPasswordsCount =
      insecurePasswordCounts.compromised_count;
}

- (void)safeBrowsingCheckStateChanged:(SafeBrowsingSafetyCheckState)state {
  _safetyCheckConfig.safeBrowsingState = state;
}

- (void)updateChromeCheckStateChanged:(UpdateChromeSafetyCheckState)state {
  _safetyCheckConfig.updateChromeState = state;
}

- (void)runningStateChanged:(RunningSafetyCheckState)state {
  _safetyCheckConfig.runningState = state;
  _safetyCheckConfig.shouldShowSeeMore =
      [_safetyCheckConfig numberOfIssues] > 2;

  if (ShouldHideSafetyCheckModuleIfNoIssues()) {
    [self updateIssueCount:[_safetyCheckConfig numberOfIssues]];
  }

  if (safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_userState)) {
    // Safety Check can be disabled by long-pressing the module, so
    // SafetyCheckManager can still be running and returning results even after
    // disabling.
    return;
  }

  // Ensures the delegate gets the latest Safety Check state only when the
  // running state changes; this avoids calling the delegate every time an
  // individual check state changes.
  _safetyCheckConfig.audience = self;
  [self safetyCheckStateDidChange:_safetyCheckConfig];
}

- (void)safetyCheckManagerWillShutdown {
  _safetyCheckManagerObserver.reset();
}

#pragma mark - ProfileStateObserver

// Conditionally starts the Safety Check if the upcoming init stage is
// `ProfileInitStage::kFinal` and the Safety Check state indicates it's running.
//
// NOTE: It's safe to call `StartSafetyCheck()` multiple times, because calling
// `StartSafetyCheck()` on an already-running Safety Check is a no-op.
- (void)profileState:(ProfileState*)profileState
    willTransitionToInitStage:(ProfileInitStage)nextInitStage
                fromInitStage:(ProfileInitStage)fromInitStage {
  if (!safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_userState) &&
      nextInitStage == ProfileInitStage::kFinal &&
      profileState.firstSceneHasInitializedUI &&
      _safetyCheckConfig.runningState == RunningSafetyCheckState::kRunning) {
    // When the Safety Check Manager can automatically trigger Safety Checks,
    // the Magic Stack should never initiate a Safety Check run.
    //
    // TODO(crbug.com/354727175): Remove `StartSafetyCheck()` from the Magic
    // Stack once the Safety Check Manager can reliably automatically trigger
    // runs.
    if (!IsSafetyCheckAutorunByManagerEnabled()) {
      _safetyCheckManager->StartSafetyCheck();
    }
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kIosSettingsSafetyCheckLastRunTime ||
      preferenceName == prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult) {
    _safetyCheckConfig.lastRunTime = [self latestSafetyCheckRunTimestamp];

    _safetyCheckConfig.safeBrowsingState =
        SafeBrowsingSafetyCheckStateForName(
            _localState->GetString(
                prefs::kIosSafetyCheckManagerSafeBrowsingCheckResult))
            .value_or(_safetyCheckConfig.safeBrowsingState);

    // Trigger a module update when the Last Run Time, or Safe Browsing state,
    // has changed.
    [self runningStateChanged:_safetyCheckConfig.runningState];
  } else if (preferenceName ==
                 safety_check::prefs::kSafetyCheckHomeModuleEnabled &&
             !_userState->GetBoolean(
                 safety_check::prefs::kSafetyCheckHomeModuleEnabled)) {
    [self.delegate removeSafetyCheckModule];
  }
}

#pragma mark - Private

// Creates the initial `SafetyCheckConfig` based on the previous check states
// stored in Prefs, or (for development builds) the overridden check states via
// Experimental settings.
- (SafetyCheckConfig*)initialSafetyCheckState {
  SafetyCheckConfig* config = [[SafetyCheckConfig alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];

  // Update Chrome check.
  std::optional<UpdateChromeSafetyCheckState> overrideUpdateChromeState =
      experimental_flags::GetUpdateChromeSafetyCheckState();

  config.updateChromeState = overrideUpdateChromeState.value_or(
      _safetyCheckManager->GetUpdateChromeCheckState());

  // Password check.
  std::optional<PasswordSafetyCheckState> overridePasswordState =
      experimental_flags::GetPasswordSafetyCheckState();

  config.passwordState = overridePasswordState.value_or(
      _safetyCheckManager->GetPasswordCheckState());

  // Safe Browsing check.
  std::optional<SafeBrowsingSafetyCheckState> overrideSafeBrowsingState =
      experimental_flags::GetSafeBrowsingSafetyCheckState();

  config.safeBrowsingState = overrideSafeBrowsingState.value_or(
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
    config.weakPasswordsCount = overrideWeakPasswordsCount.value_or(0);
    config.reusedPasswordsCount = overrideReusedPasswordsCount.value_or(0);
    config.compromisedPasswordsCount =
        overrideCompromisedPasswordsCount.value_or(0);
  } else {
    std::vector<password_manager::CredentialUIEntry> insecureCredentials =
        _safetyCheckManager->GetInsecureCredentials();

    password_manager::InsecurePasswordCounts counts =
        password_manager::CountInsecurePasswordsPerInsecureType(
            insecureCredentials);

    config.weakPasswordsCount = counts.weak_count;
    config.reusedPasswordsCount = counts.reused_count;
    config.compromisedPasswordsCount = counts.compromised_count;
  }

  config.lastRunTime = [self latestSafetyCheckRunTimestamp];
  config.runningState = CanRunSafetyCheck(config.lastRunTime)
                            ? RunningSafetyCheckState::kRunning
                            : RunningSafetyCheckState::kDefault;
  config.audience = self;
  config.itemType = [config isRunning] ? SafetyCheckItemType::kRunning
                                       : SafetyCheckItemType::kDefault;

  return config;
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
  if (lastRunAge <= safety_check::kTimeDelayForSafetyCheckAutorun) {
    return lastRunTime;
  }

  return std::nullopt;
}

// Persists the current number of Safety Check issues, `issuesCount`, to
// `_userState`.
- (void)updateIssueCount:(NSUInteger)issuesCount {
  CHECK(ShouldHideSafetyCheckModuleIfNoIssues());

  _userState->SetInteger(
      prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount, issuesCount);
}

// Informs this mediator's delegate that the Safety Check state did change.
- (void)safetyCheckStateDidChange:(SafetyCheckConfig*)config {
  (void)config;
  [self.delegate safetyCheckMagicStackMediatorDidReconfigureItem];
}

@end
