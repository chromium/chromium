// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "components/sync_device_info/device_info_sync_service.h"
#import "components/sync_device_info/device_info_tracker.h"
#import "components/sync_device_info/local_device_info_provider.h"
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/utils/first_run_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message_action.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator_delegate.h"
#import "ios/chrome/browser/synced_set_up/public/synced_set_up_metrics.h"
#import "ios/chrome/browser/synced_set_up/ui/synced_set_up_consumer.h"
#import "ios/chrome/browser/synced_set_up/utils/utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

// Logs the result of comparing a remote device's prefs (profile and
// local-state) against the local device's prefs (profile and local-state).
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(SyncedSetUpRemotePrefDifference)
enum class SyncedSetUpRemotePrefDifference {
  // No pref differences were found between the remote and local device.
  kNoDifference = 0,
  // Only remote profile prefs differed from the local device.
  kProfilePrefsDiffered = 1,
  // Only remote local-state prefs differed from the local device.
  kLocalStatePrefsDiffered = 2,
  // Both remote profile and remote local-state prefs differed from the local
  // device.
  kBothPrefsDiffered = 3,
  kMaxValue = kBothPrefsDiffered,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/ios/enums.xml:SyncedSetUpRemotePrefDifference)

// Struct for caching the pref values that differ between a remote and local
// device.
struct PrefValueToApply {
  base::Value local_value;
  base::Value remote_value;
};

// Helper for `-cachePrefs`. Caches the pref values that differ between the
// remote device and local device for a given `pref_service` and corresponding
// `pref_map`.
template <size_t N>
void CachePrefs(
    const base::fixed_flat_map<std::string_view, std::string_view, N>& pref_map,
    PrefService* pref_service,
    std::map<std::string_view, PrefValueToApply>& prefs_to_apply,
    const std::map<std::string_view, base::Value>& remote_device_prefs) {
  CHECK(pref_service);

  for (const auto& [cross_device_pref_name, remote_pref_value] :
       remote_device_prefs) {
    if (auto pref_iterator = pref_map.find(cross_device_pref_name);
        pref_iterator != pref_map.end() &&
        remote_pref_value != pref_service->GetValue(pref_iterator->second)) {
      // Pref has a different value than the local device.
      std::string_view pref_name = pref_iterator->second;
      PrefValueToApply value = {
          .local_value = pref_service->GetValue(pref_name).Clone(),
          .remote_value = remote_pref_value.Clone(),
      };
      prefs_to_apply.insert({pref_name, std::move(value)});
    }
  }
}

// Helper to log pref differences found between a remote device and the local
// device.
void LogRemotePrefDifference(bool has_profile_prefs,
                             bool has_local_state_prefs) {
  SyncedSetUpRemotePrefDifference state =
      SyncedSetUpRemotePrefDifference::kNoDifference;

  if (has_profile_prefs && has_local_state_prefs) {
    state = SyncedSetUpRemotePrefDifference::kBothPrefsDiffered;
  } else if (has_profile_prefs) {
    state = SyncedSetUpRemotePrefDifference::kProfilePrefsDiffered;
  } else if (has_local_state_prefs) {
    state = SyncedSetUpRemotePrefDifference::kLocalStatePrefsDiffered;
  }

  base::UmaHistogramEnumeration(
      "IOS.SyncedSetUp.Interstitial.RemotePrefDifference", state);
}

// The type of snackbar interaction.
enum class SnackbarInteractionType {
  kShown,
  kClicked,
  kDismissed,
};

// Helper to log interactions for the "Suggestion" snackbar.
void LogSuggestionInteraction(SnackbarInteractionType interaction_type) {
  SyncedSetUpSnackbarInteraction event;

  switch (interaction_type) {
    case SnackbarInteractionType::kShown:
      event = SyncedSetUpSnackbarInteraction::kShownSuggestion;
      break;
    case SnackbarInteractionType::kClicked:
      event = SyncedSetUpSnackbarInteraction::kClickedApply;
      break;
    case SnackbarInteractionType::kDismissed:
      event = SyncedSetUpSnackbarInteraction::kDismissedSuggestion;
      break;
  }

  LogSyncedSetUpSnackbarInteraction(event);
}

// Helper to log interactions for the "Applied" confirmation snackbar.
void LogAppliedConfirmationInteraction(
    SnackbarInteractionType interaction_type) {
  SyncedSetUpSnackbarInteraction event;

  switch (interaction_type) {
    case SnackbarInteractionType::kShown:
      event = SyncedSetUpSnackbarInteraction::kShownAppliedConfirmation;
      break;
    case SnackbarInteractionType::kClicked:
      event = SyncedSetUpSnackbarInteraction::kClickedUndo;
      break;
    case SnackbarInteractionType::kDismissed:
      event = SyncedSetUpSnackbarInteraction::kDismissedAppliedConfirmation;
      break;
  }

  LogSyncedSetUpSnackbarInteraction(event);
}

// Helper to log interactions for the "Undone" confirmation snackbar.
void LogUndoneConfirmationInteraction(
    SnackbarInteractionType interaction_type) {
  SyncedSetUpSnackbarInteraction event;

  switch (interaction_type) {
    case SnackbarInteractionType::kShown:
      event = SyncedSetUpSnackbarInteraction::kShownUndoneConfirmation;
      break;
    case SnackbarInteractionType::kClicked:
      event = SyncedSetUpSnackbarInteraction::kClickedRedo;
      break;
    case SnackbarInteractionType::kDismissed:
      event = SyncedSetUpSnackbarInteraction::kDismissedUndoneConfirmation;
      break;
  }

  LogSyncedSetUpSnackbarInteraction(event);
}

// Internal states for the `SyncedSetUpMediator` representing pending pref
// write actions.
enum class SyncedSetUpState {
  // Not pending any actions.
  kIdle = 0,
  // Has prefs initially available to apply, and will apply them
  // automatically.
  kPendingAutoApply = 1,
  // Has prefs initially available to apply, and will apply them if
  // the apply action is taken in the UI.
  kPendingApplyAction = 2,
  // Applied a set of remote prefs, and is pending a possible
  // "undo" action to revert them.
  kPendingUndoAction = 3,
  // Reverted a set of applied remote prefs, and is pending a
  // possible "redo" action to apply them again.
  kPendingRedoAction = 4,
  kMaxValue = kPendingRedoAction
};

// Logs the appropriate snackbar interaction metric based on the given `state`.
void LogSnackbarInteraction(SyncedSetUpState state,
                            SnackbarInteractionType interaction_type) {
  switch (state) {
    case SyncedSetUpState::kPendingApplyAction:
      LogSuggestionInteraction(interaction_type);
      break;
    case SyncedSetUpState::kPendingAutoApply:
    case SyncedSetUpState::kPendingUndoAction:
      LogAppliedConfirmationInteraction(interaction_type);
      break;
    case SyncedSetUpState::kPendingRedoAction:
      LogUndoneConfirmationInteraction(interaction_type);
      break;
    case SyncedSetUpState::kIdle:
      NOTREACHED();
  }
}

}  // namespace

@implementation SyncedSetUpMediator {
  // Tracker for retrieving cross device preferences.
  raw_ptr<sync_preferences::CrossDevicePrefTracker> _prefTracker;
  // Service for account authentication.
  raw_ptr<AuthenticationService> _authenticationService;
  // Service for account information (i.e., user name, user avatar).
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  // Service for identity information and change notifications.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Bridge to observe `IdentityManager::Observer` events.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  // Service for retrieving device info.
  raw_ptr<syncer::DeviceInfoSyncService> _deviceInfoSyncService;
  // The profile Pref service.
  raw_ptr<PrefService> _profilePrefService;
  // List of current app web states.
  raw_ptr<WebStateList> _webStateList;
  // Parameters relevant to understanding the app startup, used to determine
  // how the Synced Set Up flow should be presented.
  AppStartupParameters* _startupParameters;
  // Caches profile prefs that differ between this device and a remote device.
  // The map stores the pref name and a pair of values
  std::map<std::string_view, PrefValueToApply> _profilePrefsToApply;
  // Caches local-state prefs that differ between this device and a remote
  // device. The map stores the pref name and a pair of values
  std::map<std::string_view, PrefValueToApply> _localStatePrefsToApply;
  // The current primary signed-in identity.
  id<SystemIdentity> _primaryIdentity;
  // Command handler for snackbar commands.
  id<SnackbarCommands> _snackbarCommandsHandler;
  // Current state of the the Synced Set Up flow.
  SyncedSetUpState _state;
  // Number of active Snackbars. This helps determine when this mediator is
  // finished.
  size_t _snackbarCount;
}

#pragma mark - Public methods

- (instancetype)
        initWithPrefTracker:(sync_preferences::CrossDevicePrefTracker*)tracker
      authenticationService:(AuthenticationService*)authenticationService
      accountManagerService:(ChromeAccountManagerService*)accountManagerService
      deviceInfoSyncService:
          (syncer::DeviceInfoSyncService*)deviceInfoSyncService
         profilePrefService:(PrefService*)profilePrefService
            identityManager:(signin::IdentityManager*)identityManager
               webStateList:(WebStateList*)webStateList
          startupParameters:(AppStartupParameters*)startupParameters
    snackbarCommandsHandler:(id<SnackbarCommands>)handler {
  if ((self = [super init])) {
    CHECK(tracker);
    CHECK(authenticationService);
    CHECK(accountManagerService);
    CHECK(deviceInfoSyncService);
    CHECK(profilePrefService);
    CHECK(identityManager);
    CHECK(webStateList);
    CHECK(handler);

    _prefTracker = tracker;
    _authenticationService = authenticationService;
    _accountManagerService = accountManagerService;
    _deviceInfoSyncService = deviceInfoSyncService;
    _profilePrefService = profilePrefService;
    _webStateList = webStateList;
    _startupParameters = startupParameters;
    _state = SyncedSetUpState::kIdle;
    _snackbarCount = 0;
    _snackbarCommandsHandler = handler;
    _identityManager = identityManager;

    _identityManagerObserverBridge =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
  }
  return self;
}

- (void)disconnect {
  _identityManagerObserverBridge.reset();
  _profilePrefsToApply.clear();
  _localStatePrefsToApply.clear();
  _prefTracker = nullptr;
  _authenticationService = nullptr;
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _deviceInfoSyncService = nullptr;
  _profilePrefService = nullptr;
  _webStateList = nullptr;
  _primaryIdentity = nil;
  _startupParameters = nil;
  _snackbarCommandsHandler = nil;
}

- (void)setConsumer:(id<SyncedSetUpConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [self updatePrimaryIdentity];
  [self updateConsumer];
}

- (void)setDelegate:(id<SyncedSetUpMediatorDelegate>)delegate {
  _delegate = delegate;
  if (!_delegate) {
    [self notifyMediatorIsFinished];
    return;
  }

  [self cachePrefs];
  [self configureInitialState];

  switch (_state) {
    case SyncedSetUpState::kPendingAutoApply:
      [self.delegate mediatorWillStartPostFirstRunFlow:self];
      return;
    case SyncedSetUpState::kPendingApplyAction:
      [self.delegate mediatorWillStartFromUrlPage:self];
      break;
    case SyncedSetUpState::kIdle:
      [self notifyMediatorIsFinished];
      break;
    case SyncedSetUpState::kPendingRedoAction:
    case SyncedSetUpState::kPendingUndoAction:
      NOTREACHED();
  }
}

- (void)applyPrefs {
  // No preferences to apply.
  if (_profilePrefsToApply.empty() && _localStatePrefsToApply.empty()) {
    [self notifyMediatorIsFinished];
    return;
  }

  switch (_state) {
    case SyncedSetUpState::kPendingAutoApply:
      [self applyPrefsFromRemoteDevice];
      _state = SyncedSetUpState::kPendingUndoAction;
      return;
    case SyncedSetUpState::kPendingApplyAction:
      if ([self maybeShowSnackbar]) {
        return;
      }
      break;
    case SyncedSetUpState::kPendingRedoAction:
      if ([self maybeShowSnackbar]) {
        _state = SyncedSetUpState::kPendingUndoAction;
        return;
      }
      break;
    case SyncedSetUpState::kPendingUndoAction:
      if ([self maybeShowSnackbar]) {
        _state = SyncedSetUpState::kPendingRedoAction;
        return;
      }
      break;
    case SyncedSetUpState::kIdle:
      break;
  }

  // No prefs were applied and no Snackbar was shown.
  [self notifyMediatorIsFinished];
}

- (BOOL)maybeShowSnackbar {
  if ([self shouldShowSnackbar]) {
    _snackbarCount++;
    LogSnackbarInteraction(_state, SnackbarInteractionType::kShown);
    [_snackbarCommandsHandler showSnackbarMessage:[self snackbarMessage]];
    [self.delegate recordSyncedSetUpShown:self];
    return YES;
  }
  return NO;
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when the primary account changes (e.g., sign-in, sign-out, account
// switch).
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  [self updatePrimaryIdentity];
}

// Called when the extended account info (i.e., name and avatar) is
// updated/fetched.
- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  if (!_primaryIdentity || _primaryIdentity.gaiaId != info.gaia) {
    return;
  }

  // The primary identity's info has been updated (e.g., avatar finished
  // loading).
  [self updateConsumer];
}

#pragma mark - Private methods

// Caches remote profile and local-state pref values that differ from the local
// device.
- (void)cachePrefs {
  syncer::LocalDeviceInfoProvider* localDeviceInfoProvider =
      _deviceInfoSyncService->GetLocalDeviceInfoProvider();
  CHECK(localDeviceInfoProvider);

  std::map<std::string_view, base::Value> remoteDevicePrefs =
      GetCrossDevicePrefsFromRemoteDevice(
          _prefTracker, _deviceInfoSyncService->GetDeviceInfoTracker(),
          localDeviceInfoProvider->GetLocalDeviceInfo());

  // No remote prefs to apply.
  if (remoteDevicePrefs.empty()) {
    LogRemotePrefDifference(/*has_profile_prefs=*/false,
                            /*has_local_state_prefs=*/false);
    return;
  }

  // Cache profile and local-state prefs.
  CachePrefs(kCrossDeviceToProfilePrefMap, _profilePrefService,
             _profilePrefsToApply, remoteDevicePrefs);
  CachePrefs(kCrossDeviceToLocalStatePrefMap,
             GetApplicationContext()->GetLocalState(), _localStatePrefsToApply,
             remoteDevicePrefs);

  LogRemotePrefDifference(!_profilePrefsToApply.empty(),
                          !_localStatePrefsToApply.empty());
}

// Sets the initial state of this mediator.
- (void)configureInitialState {
  if (_profilePrefsToApply.empty() && _localStatePrefsToApply.empty()) {
    _state = SyncedSetUpState::kIdle;
    return;
  }

  if (IsFirstRun()) {
    _state = SyncedSetUpState::kPendingAutoApply;
    return;
  }

  _state = SyncedSetUpState::kPendingApplyAction;
}

// Updates the cached primary identity based on the current state.
// If the identity has changed, this method also triggers an update to the
// consumer.
- (void)updatePrimaryIdentity {
  id<SystemIdentity> newPrimaryIdentity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);

  if (newPrimaryIdentity == _primaryIdentity) {
    return;
  }
  _primaryIdentity = newPrimaryIdentity;
  [self updateConsumer];
}

// Fetches the latest name and avatar for the current `_primaryIdentity` and
// updates the consumer.
- (void)updateConsumer {
  if (!_consumer) {
    return;
  }

  if (!_primaryIdentity) {
    // Handle signed-out state.
    [_consumer
        setWelcomeMessage:l10n_util::GetNSString(
                              IDS_IOS_SYNCED_SET_UP_WELCOME_MESSAGE_TITLE)];
    [_consumer setAvatarImage:nil];
    return;
  }

  // Get the avatar. `GetIdentityAvatarWithIdentityOnDevice()` handles
  // asynchronous loading. It returns a cached image or a placeholder
  // immediately and initiates a fetch in the background if necessary. When the
  // fetch completes,
  // `-onExtendedAccountInfoUpdated:` will be called.
  UIImage* avatar =
      GetApplicationContext()->GetIdentityAvatarProvider()->GetIdentityAvatar(
          _primaryIdentity, IdentityAvatarSize::Large);

  [_consumer setWelcomeMessage:
                 l10n_util::GetNSStringF(
                     IDS_IOS_SYNCED_SET_UP_WELCOME_MESSAGE_WITH_USER_NAME_TITLE,
                     base::SysNSStringToUTF16(_primaryIdentity.userGivenName))];
  [_consumer setAvatarImage:avatar];
}

// Applies profile and local-state prefs from a remote device.
- (void)applyPrefsFromRemoteDevice {
  int count = _profilePrefsToApply.size() + _localStatePrefsToApply.size();
  LogSyncedSetUpRemoteAppliedPrefCount(count);

  PrefService* localState = GetApplicationContext()->GetLocalState();

  for (const auto& [pref_name, remote_pref_value] : _profilePrefsToApply) {
    LogSyncedSetUpPrefApplied(pref_name);
    _profilePrefService->Set(pref_name, remote_pref_value.remote_value);
  }
  for (const auto& [pref_name, remote_pref_value] : _localStatePrefsToApply) {
    LogSyncedSetUpPrefApplied(pref_name);
    localState->Set(pref_name, remote_pref_value.remote_value);
  }
}

// Applies the cached profile and local-state prefs from the local device. This
// is used to reverse the application of prefs from a remote device.
- (void)applyPrefsFromLocalDevice {
  PrefService* localState = GetApplicationContext()->GetLocalState();

  for (const auto& [pref_name, local_pref_value] : _profilePrefsToApply) {
    _profilePrefService->Set(pref_name, local_pref_value.local_value);
  }
  for (const auto& [pref_name, local_pref_value] : _localStatePrefsToApply) {
    localState->Set(pref_name, local_pref_value.local_value);
  }
}

// Returns the appropriate Snackbar configuration for the current state of this
// mediator.
- (SnackbarMessage*)snackbarMessage {
  NSString* snackbarMessageText;
  NSString* snackbarButtonText;

  switch (_state) {
    case SyncedSetUpState::kPendingApplyAction: {
      snackbarMessageText = l10n_util::GetNSString(
          IDS_IOS_SYNCED_SET_UP_SNACKBAR_PROMO_MESSAGE_NTP);
      snackbarButtonText =
          l10n_util::GetNSString(IDS_IOS_SYNCED_SET_UP_SNACKBAR_PROMO_ACTION);
      break;
    }
    case SyncedSetUpState::kPendingAutoApply:
    case SyncedSetUpState::kPendingUndoAction: {
      snackbarMessageText = l10n_util::GetNSString(
          IDS_IOS_SYNCED_SET_UP_SNACKBAR_APPLIED_MESSAGE_NTP);
      snackbarButtonText =
          l10n_util::GetNSString(IDS_IOS_SYNCED_SET_UP_SNACKBAR_APPLIED_ACTION);
      break;
    }
    case SyncedSetUpState::kPendingRedoAction: {
      snackbarMessageText = l10n_util::GetNSString(
          IDS_IOS_SYNCED_SET_UP_SNACKBAR_REMOVED_MESSAGE_NTP);
      snackbarButtonText =
          l10n_util::GetNSString(IDS_IOS_SYNCED_SET_UP_SNACKBAR_REMOVED_ACTION);
      break;
    }
    case SyncedSetUpState::kIdle:
      NOTREACHED();
  }

  SnackbarMessage* message =
      [[SnackbarMessage alloc] initWithTitle:snackbarMessageText];
  SnackbarMessageAction* button = [[SnackbarMessageAction alloc] init];
  button.title = snackbarButtonText;

  __weak __typeof(self) weakSelf = self;

  // Capture the state at the time the snackbar is shown. This is important
  // because `_state` may change by the time the button handler or completion
  // handler runs.
  SyncedSetUpState state = _state;

  button.handler = ^{
    LogSnackbarInteraction(state, SnackbarInteractionType::kClicked);
    [weakSelf handleSnackbarButtonAction];
  };
  message.action = button;
  message.completionHandler = ^(BOOL userInteracted) {
    if (!userInteracted) {
      LogSnackbarInteraction(state, SnackbarInteractionType::kDismissed);
    }
    [weakSelf handleSnackbarDismissal];
  };
  return message;
}

// Returns whether a Snackbar should be presented based on the Synced Set Up
// state, available preference changes, and the currently visible page.
- (BOOL)shouldShowSnackbar {
  switch (_state) {
    case SyncedSetUpState::kPendingApplyAction:
      return [self shouldPromptToApplyPrefs];
    case SyncedSetUpState::kPendingAutoApply:
    case SyncedSetUpState::kPendingUndoAction:
    case SyncedSetUpState::kPendingRedoAction:
      return YES;
    case SyncedSetUpState::kIdle:
      return NO;
  }
}

// Returns whether the flow should give the user an initial Snackbar prompt to
// apply remote preferences. The prompt should appear on a URL page if there is
// a pending pref change that would be affect the appearance of the URL page
// (e.g. omnibox position). The prompt should appear on the NTP if there is a
// pending pref change that would be affect the appearance of the NTP (e.g. home
// customizations).
- (BOOL)shouldPromptToApplyPrefs {
  // Determine whether the prompt should appear according to whether the current
  // page is the NTP and what pref changes are available.
  return [self shouldPromptToApplyPrefsOnNewTabPage] ||
         [self shouldPromptToApplyPrefsOnUrlPage];
}

// Helper for `-shouldPromptToApplyPrefs`. Returns YES if the conditions are met
// to show a prompt to apply prefs from the New Tab Page.
- (BOOL)shouldPromptToApplyPrefsOnNewTabPage {
  if (_profilePrefsToApply.empty()) {
    // There are no NTP-visible prefs to apply.
    return NO;
  }

  if (!IsVisibleURLNewTabPage(_webStateList->GetActiveWebState())) {
    // The NTP is not visible.
    return NO;
  }

  if (!_startupParameters) {
    // The app started without external intent and landed on the NTP without UI
    // obstructions.
    return YES;
  }

  if (!_startupParameters.externalURL.is_valid()) {
    // The app started with external intent and landed on an unknown page.
    return NO;
  }

  if (_startupParameters.externalURL != GURL(kChromeUINewTabURL)) {
    // The app started with external intent did not land on the NTP.
    return NO;
  }

  if (_startupParameters.postOpeningAction !=
      TabOpeningPostOpeningAction::NO_ACTION) {
    // The app started with external intent and landed on an NTP that will be
    // obstructed by another UI element.
    return NO;
  }

  // Did not find an NTP-visible pref value to apply.
  return NO;
}

// Helper for `-shouldPromptToApplyPrefs`. Returns YES if the conditions are met
// to show a prompt to apply prefs from a URL page.
- (BOOL)shouldPromptToApplyPrefsOnUrlPage {
  if (_localStatePrefsToApply.find(omnibox::kIsOmniboxInBottomPosition) ==
      _localStatePrefsToApply.end()) {
    // There are no URL page-visible prefs to apply.
    return NO;
  }

  if (!_startupParameters &&
      !IsVisibleURLNewTabPage(_webStateList->GetActiveWebState())) {
    // The app started without external intent and landed on a URL page.
    return YES;
  }

  if (!_startupParameters) {
    // The app started without external intent and landed on the NTP.
    return NO;
  }

  if (!_startupParameters.externalURL.is_valid()) {
    // The app started with external intent and landed on an unknown page.
    return NO;
  }

  if (_startupParameters.externalURL == GURL(kChromeUINewTabURL)) {
    // The app started with external intent and landed on the NTP.
    return NO;
  }

  if (_startupParameters.postOpeningAction !=
      TabOpeningPostOpeningAction::NO_ACTION) {
    // The app started with external intent and landed on a URL page that will
    // be obsructed by another UI element.
    return NO;
  }

  // A URL page-visible pref value is available to apply from a remote device.
  return YES;
}

// Handles Synced Set Up Snackbar button presses.
- (void)handleSnackbarButtonAction {
  switch (_state) {
    case SyncedSetUpState::kPendingApplyAction:
    case SyncedSetUpState::kPendingRedoAction:
      [self applyPrefsFromRemoteDevice];
      _state = SyncedSetUpState::kPendingUndoAction;
      [self maybeShowSnackbar];
      return;
    case SyncedSetUpState::kPendingAutoApply:
    case SyncedSetUpState::kPendingUndoAction:
      // States `kPendingAutoApply` and `kPendingUndoAction` both show the same
      // Snackbar, prompting a user to undo pref changes and revert to the local
      // device prefs.
      [self applyPrefsFromLocalDevice];
      _state = SyncedSetUpState::kPendingRedoAction;
      [self maybeShowSnackbar];
      return;
    case SyncedSetUpState::kIdle:
      NOTREACHED();
  }
}

// Handles the dismissal of a Synced Set Up Snackbar. If there is no visible
// Snackbar after the current one dismisses, the dismissal ends this mediator.
- (void)handleSnackbarDismissal {
  _snackbarCount--;
  if (!_snackbarCount) {
    [self notifyMediatorIsFinished];
  }
}

// Called when this mediator is finished.
- (void)notifyMediatorIsFinished {
  [self.delegate syncedSetUpMediatorDidComplete:self];
}

@end
