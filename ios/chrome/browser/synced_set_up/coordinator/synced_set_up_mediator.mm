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
#import "components/sync_preferences/cross_device_pref_tracker/cross_device_pref_tracker.h"
#import "ios/chrome/app/app_startup_parameters.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/synced_set_up/coordinator/synced_set_up_mediator_delegate.h"
#import "ios/chrome/browser/synced_set_up/ui/synced_set_up_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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
  }

  return self;
}

- (void)disconnect {
  _identityManagerObserverBridge.reset();
  _authenticationService = nullptr;
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _primaryIdentity = nil;
  self.consumer = nil;
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
      _accountManagerService->GetIdentityAvatarWithIdentityOnDevice(
          _primaryIdentity, IdentityAvatarSize::Large);

  [_consumer setWelcomeMessage:
                 l10n_util::GetNSStringF(
                     IDS_IOS_SYNCED_SET_UP_WELCOME_MESSAGE_WITH_USER_NAME_TITLE,
                     base::SysNSStringToUTF16(_primaryIdentity.userGivenName))];
  [_consumer setAvatarImage:avatar];
}

@end
