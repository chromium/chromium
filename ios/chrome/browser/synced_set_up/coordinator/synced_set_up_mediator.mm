// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/not_fatal_until.h"
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
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/avatar_provider.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator_delegate.h"
#import "ios/chrome/browser/synced_set_up/ui/synced_set_up_consumer.h"
#import "ios/chrome/browser/synced_set_up/utils/utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

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
  // Caches profile prefs that differ between this device and a remote device.
  // The map stores the pref name and a pair of values
  std::map<std::string_view, PrefValueToApply> _profilePrefsToApply;
  // Caches local-state prefs that differ between this device and a remote
  // device. The map stores the pref name and a pair of values
  std::map<std::string_view, PrefValueToApply> _localStatePrefsToApply;
  // Parameters relevant to understanding the app startup, used to determine
  // how the Synced Set Up flow should be presented.
  AppStartupParameters* _startupParameters;
  // The current primary signed-in identity.
  id<SystemIdentity> _primaryIdentity;
}

#pragma mark - Public methods

- (instancetype)
      initWithPrefTracker:(sync_preferences::CrossDevicePrefTracker*)tracker
    authenticationService:(AuthenticationService*)authenticationService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
    deviceInfoSyncService:(syncer::DeviceInfoSyncService*)deviceInfoSyncService
       profilePrefService:(PrefService*)profilePrefService
        startupParameters:(AppStartupParameters*)startupParameters
          identityManager:(signin::IdentityManager*)identityManager {
  if ((self = [super init])) {
    CHECK(tracker);
    CHECK(authenticationService);
    CHECK(accountManagerService);
    CHECK(deviceInfoSyncService);
    CHECK(profilePrefService);
    CHECK(startupParameters, base::NotFatalUntil::M147);
    CHECK(identityManager);

    _prefTracker = tracker;
    _authenticationService = authenticationService;
    _accountManagerService = accountManagerService;
    _deviceInfoSyncService = deviceInfoSyncService;
    _profilePrefService = profilePrefService;
    _startupParameters = startupParameters;
    _identityManager = identityManager;

    _identityManagerObserverBridge =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);

    [self updatePrimaryIdentity];
    [self cachePrefs];
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
  _startupParameters = nil;
  _primaryIdentity = nil;
}

- (void)setConsumer:(id<SyncedSetUpConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  [self updateConsumer];
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
    return;
  }

  // Cache profile and local-state prefs.
  CachePrefs(kCrossDeviceToProfilePrefMap, _profilePrefService,
             _profilePrefsToApply, remoteDevicePrefs);
  CachePrefs(kCrossDeviceToLocalStatePrefMap,
             GetApplicationContext()->GetLocalState(), _localStatePrefsToApply,
             remoteDevicePrefs);
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

@end
