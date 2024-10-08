// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_magic_stack_mediator.h"

#import <optional>

#import "base/memory/raw_ptr.h"
#import "base/values.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/app_state_observer.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_observer_bridge.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_audience.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_consumer_source.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_magic_stack_consumer.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_prefs.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_state.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/utils.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_observer.h"

namespace {

// Returns the number of times the Safety Check module with the
// notifications opt-in button has been shown to the user in the Magic Stack.
//
// If `only_include_top_module` is `true`, only impressions where the module
// was shown at the top of the Magic Stack are counted.
int ImpressionsCount(const base::Value::List& impressions,
                     bool only_include_top_module) {
  int count = 0;

  for (const base::Value& impression : impressions) {
    std::optional<int> index = impression.GetIfInt();

    if (index.has_value() && (!only_include_top_module || index.value() == 0)) {
      count++;
    }
  }

  return count;
}

}  // namespace

@interface SafetyCheckMagicStackMediator () <
    AppStateObserver,
    MagicStackModuleDelegate,
    NotificationsSettingsObserverDelegate,
    PrefObserverDelegate,
    SafetyCheckAudience,
    SafetyCheckConsumerSource,
    SafetyCheckManagerObserver>
@end

@implementation SafetyCheckMagicStackMediator {
  raw_ptr<IOSChromeSafetyCheckManager> _safetyCheckManager;
  // Observer for Safety Check changes.
  std::unique_ptr<SafetyCheckObserverBridge> _safetyCheckManagerObserver;
  // Bridge to listen to pref changes.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for local pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Registrar for user pref changes notifications.
  PrefChangeRegistrar _userPrefChangeRegistrar;
  // Local State prefs.
  raw_ptr<PrefService> _localState;
  // User prefs.
  raw_ptr<PrefService> _userState;
  AppState* _appState;
  // Used by the Safety Check (Magic Stack) module for the current Safety Check
  // state.
  SafetyCheckState* _safetyCheckState;
  id<SafetyCheckMagicStackConsumer> _safetyCheckConsumer;
  // An observer that tracks whether push notification permission settings have
  // been modified.
  NotificationsSettingsObserver* _notificationsObserver;
}

- (instancetype)initWithSafetyCheckManager:
                    (IOSChromeSafetyCheckManager*)safetyCheckManager
                                localState:(PrefService*)localState
                                 userState:(PrefService*)userState
                                  appState:(AppState*)appState {
  self = [super init];
  if (self) {
    _safetyCheckManager = safetyCheckManager;
    _localState = localState;
    _userState = userState;
    _appState = appState;

    [_appState addObserver:self];

    if (IsSafetyCheckNotificationsEnabled()) {
      _notificationsObserver = [[NotificationsSettingsObserver alloc]
          initWithPrefService:userState
                   localState:localState];

      _notificationsObserver.delegate = self;
    }

    if (!safety_check_prefs::IsSafetyCheckInMagicStackDisabled(
            IsHomeCustomizationEnabled() ? _userState : _localState)) {
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
      _prefObserverBridge->ObserveChangesForPreference(
          safety_check_prefs::kSafetyCheckInMagicStackDisabledPref,
          &_prefChangeRegistrar);

      if (IsHomeCustomizationEnabled()) {
        _userPrefChangeRegistrar.Init(userState);
        _prefObserverBridge->ObserveChangesForPreference(
            prefs::kHomeCustomizationMagicStackSafetyCheckEnabled,
            &_userPrefChangeRegistrar);
      }

      _safetyCheckState = [self initialSafetyCheckState];

      _safetyCheckState.delegate = self;

      if (ShouldHideSafetyCheckModuleIfNoIssues()) {
        [self updateIssueCount:[_safetyCheckState numberOfIssues]
               withPrefService:localState];
      }

      _safetyCheckManagerObserver =
          std::make_unique<SafetyCheckObserverBridge>(self, safetyCheckManager);

      if (_appState.initStage > AppInitStage::kNormalUI &&
          _appState.firstSceneHasInitializedUI &&
          _safetyCheckState.runningState == RunningSafetyCheckState::kRunning) {
        // When the Safety Check Notifications feature is enabled, the Magic
        // Stack should never initiate a Safety Check run.
        //
        // TODO(crbug.com/354727175): Remove `StartSafetyCheck()` from the Magic
        // Stack once Safety Check Notifications fully launches.
        if (!IsSafetyCheckNotificationsEnabled()) {
          safetyCheckManager->StartSafetyCheck();
        }
      }
    }
  }
  return self;
}

- (void)disconnect {
  _notificationsObserver.delegate = nil;
  [_notificationsObserver disconnect];
  _notificationsObserver = nil;
  _safetyCheckManagerObserver.reset();
  if (_prefObserverBridge) {
    _prefChangeRegistrar.RemoveAll();
    if (IsHomeCustomizationEnabled()) {
      _userPrefChangeRegistrar.RemoveAll();
    }

    _prefObserverBridge.reset();
  }
  [_appState removeObserver:self];
}

- (SafetyCheckState*)safetyCheckState {
  return _safetyCheckState;
}

- (void)disableModule {
  safety_check_prefs::DisableSafetyCheckInMagicStack(
      IsHomeCustomizationEnabled() ? _userState : _localState);
}

- (void)reset {
  _safetyCheckState = [[SafetyCheckState alloc]
      initWithUpdateChromeState:UpdateChromeSafetyCheckState::kDefault
                  passwordState:PasswordSafetyCheckState::kDefault
              safeBrowsingState:SafeBrowsingSafetyCheckState::kDefault
                   runningState:RunningSafetyCheckState::kDefault];

  if (IsSafetyCheckNotificationsEnabled()) {
    _safetyCheckState.showNotificationsOptIn =
        [self shouldShowNotificationsOptIn];
  }

  _safetyCheckState.delegate = self;
  _safetyCheckState.audience = self;
  _safetyCheckState.safetyCheckConsumerSource = self;
}

#pragma mark - SafetyCheckConsumerSource

- (void)addConsumer:(id<SafetyCheckMagicStackConsumer>)consumer {
  _safetyCheckConsumer = consumer;
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
  _safetyCheckState.passwordState = state;
  _safetyCheckState.weakPasswordsCount = insecurePasswordCounts.weak_count;
  _safetyCheckState.reusedPasswordsCount = insecurePasswordCounts.reused_count;
  _safetyCheckState.compromisedPasswordsCount =
      insecurePasswordCounts.compromised_count;
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

  if (ShouldHideSafetyCheckModuleIfNoIssues()) {
    [self updateIssueCount:[_safetyCheckState numberOfIssues]
           withPrefService:_localState];
  }

  if (safety_check_prefs::IsSafetyCheckInMagicStackDisabled(
          IsHomeCustomizationEnabled() ? _userState : _localState)) {
    // Safety Check can be disabled by long-pressing the module, so
    // SafetyCheckManager can still be running and returning results even after
    // disabling.
    return;
  }

  // Ensures the consumer gets the latest Safety Check state only when the
  // running state changes; this avoids calling the consumer every time an
  // individual check state changes.
  _safetyCheckState.audience = self;
  [_safetyCheckConsumer safetyCheckStateDidChange:_safetyCheckState];
}

- (void)safetyCheckManagerWillShutdown {
  _safetyCheckManagerObserver.reset();
}

#pragma mark - AppStateObserver

// Conditionally starts the Safety Check if the upcoming init stage is
// `AppInitStage::kFinal` and the Safety Check state indicates it's running.
//
// NOTE: It's safe to call `StartSafetyCheck()` multiple times, because calling
// `StartSafetyCheck()` on an already-running Safety Check is a no-op.
- (void)appState:(AppState*)appState
    willTransitionToInitStage:(AppInitStage)nextInitStage {
  if (!safety_check_prefs::IsSafetyCheckInMagicStackDisabled(
          IsHomeCustomizationEnabled() ? _userState : _localState) &&
      nextInitStage == AppInitStage::kFinal &&
      appState.firstSceneHasInitializedUI &&
      _safetyCheckState.runningState == RunningSafetyCheckState::kRunning) {
    // When the Safety Check Notifications feature is enabled, the Magic
    // Stack should never initiate a Safety Check run.
    //
    // TODO(crbug.com/354727175): Remove `StartSafetyCheck()` from the Magic
    // Stack once Safety Check Notifications fully launches.
    if (!IsSafetyCheckNotificationsEnabled()) {
      _safetyCheckManager->StartSafetyCheck();
    }
  }
}

#pragma mark - MagicStackModuleDelegate

// Stores the index at which the Safety Check module (with notifications
// opt-in button) was displayed in the Magic Stack. This is used to track
// impressions for the Safety Check Notifications feature.
- (void)magicStackModule:(MagicStackModule*)magicStackModule
     wasDisplayedAtIndex:(NSUInteger)index {
  if (magicStackModule.type != ContentSuggestionsModuleType::kSafetyCheck ||
      !magicStackModule.showNotificationsOptIn) {
    return;
  }

  if (IsSafetyCheckNotificationsEnabled()) {
    CHECK(_localState);

    base::Value::List impressions =
        _localState->GetList(prefs::kMagicStackSafetyCheckNotificationsShown)
            .Clone();

    impressions.Append(static_cast<int>(index));

    _localState->SetList(prefs::kMagicStackSafetyCheckNotificationsShown,
                         std::move(impressions));
  }
}

#pragma mark - NotificationsSettingsObserverDelegate

- (void)notificationsSettingsDidChangeForClient:
    (PushNotificationClientId)clientID {
  CHECK(IsSafetyCheckNotificationsEnabled());

  if (clientID == PushNotificationClientId::kSafetyCheck) {
    // When Safety Check notification permissions change, refresh the Magic
    // Stack module. This ensures the Safety Check container accurately reflects
    // the user's notification settings.
    _safetyCheckState.showNotificationsOptIn =
        [self shouldShowNotificationsOptIn];

    [_safetyCheckConsumer safetyCheckStateDidChange:_safetyCheckState];
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
  } else if (preferenceName ==
             safety_check_prefs::kSafetyCheckInMagicStackDisabledPref) {
    if (safety_check_prefs::IsSafetyCheckInMagicStackDisabled(_localState)) {
      [self.delegate removeSafetyCheckModule];
    }
  } else if (preferenceName ==
                 prefs::kHomeCustomizationMagicStackSafetyCheckEnabled &&
             !_userState->GetBoolean(
                 prefs::kHomeCustomizationMagicStackSafetyCheckEnabled)) {
    CHECK(IsHomeCustomizationEnabled());
    [self.delegate removeSafetyCheckModule];
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

  if (IsSafetyCheckNotificationsEnabled()) {
    state.showNotificationsOptIn = [self shouldShowNotificationsOptIn];
  }

  state.audience = self;
  state.safetyCheckConsumerSource = self;

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
  if (lastRunAge <= TimeDelayForSafetyCheckAutorun()) {
    return lastRunTime;
  }

  return std::nullopt;
}

// Persists the current number of Safety Check issues, `issuesCount`, to
// `localPrefService`.
- (void)updateIssueCount:(NSUInteger)issuesCount
         withPrefService:(PrefService*)localPrefService {
  CHECK(localPrefService);
  CHECK(ShouldHideSafetyCheckModuleIfNoIssues());

  localPrefService->SetInteger(
      prefs::kHomeCustomizationMagicStackSafetyCheckIssuesCount, issuesCount);
}

// Returns `YES` if the notifications opt-in button should be displayed.
- (BOOL)shouldShowNotificationsOptIn {
  CHECK(IsSafetyCheckNotificationsEnabled());

  BOOL isOptedIn = push_notification_settings::
      GetMobileNotificationPermissionStatusForClient(
          PushNotificationClientId::kSafetyCheck, "");

  if (isOptedIn) {
    return NO;
  }

  base::Value::List impressions =
      _localState->GetList(prefs::kMagicStackSafetyCheckNotificationsShown)
          .Clone();

  SafetyCheckNotificationsImpressionTrigger trigger =
      SafetyCheckNotificationsImpressionTriggerEnabled();

  int impressionsCount = ImpressionsCount(
      impressions,
      trigger == SafetyCheckNotificationsImpressionTrigger::kOnlyWhenTopModule);

  int impressionsLimit = SafetyCheckNotificationsImpressionLimit();

  return impressionsCount < impressionsLimit;
}

@end
