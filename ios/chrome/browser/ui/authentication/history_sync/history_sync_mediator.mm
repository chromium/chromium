// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_mediator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_capabilities_fetcher.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

// Mediator that handles the sync operations.
@interface HistorySyncMediator () <ChromeAccountManagerServiceObserver,
                                   IdentityManagerObserverBridgeDelegate>
@end

@implementation HistorySyncMediator {
  raw_ptr<AuthenticationService> _authenticationService;
  // Account manager service with observer.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
  std::unique_ptr<ChromeAccountManagerServiceObserverBridge>
      _accountManagerServiceObserver;
  raw_ptr<signin::IdentityManager> _identityManager;
  // Observer for `IdentityManager`.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  // Sync service.
  raw_ptr<syncer::SyncService> _syncService;
  // `YES` if the user's email should be shown in the footer text.
  BOOL _showUserEmail;
  // Capabilities fetcher to determine minor mode restriction.
  HistorySyncCapabilitiesFetcher* _capabilitiesFetcher;
  // This boolean should help to understand CHECK failure with
  // crbug.com/366198713. This variable can be removed once the bug is fixed.
  BOOL _signoutNotificationCalled;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
      chromeAccountManagerService:
          (ChromeAccountManagerService*)chromeAccountManagerService
                  identityManager:(signin::IdentityManager*)identityManager
                      syncService:(syncer::SyncService*)syncService
                    showUserEmail:(BOOL)showUserEmail {
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
    _accountManagerService = chromeAccountManagerService;
    _accountManagerServiceObserver =
        std::make_unique<ChromeAccountManagerServiceObserverBridge>(
            self, _accountManagerService);
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _syncService = syncService;
    _showUserEmail = showUserEmail;
    _capabilitiesFetcher = [[HistorySyncCapabilitiesFetcher alloc]
        initWithIdentityManager:identityManager];
  }

  return self;
}

- (void)disconnect {
  _accountManagerServiceObserver.reset();
  _identityManagerObserver.reset();
  [_capabilitiesFetcher shutdown];
  _capabilitiesFetcher = nil;
  _authenticationService = nullptr;
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _syncService = nullptr;
}

- (void)enableHistorySyncOptin {
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  bool hasPrimaryAccount =
      _identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  // It is possible to have no identity from AuthenticationService here
  // (see crbug.com/366198713).
  // The mediator listens for IdentityManagerObserverBridgeDelegate to know
  // if the user is signed out. If it happens, the dialog is supposed to be
  // dissmissed automatically.
  // to understand if there is a difference between AuthenticationService and
  // IdentityManager, the CHECK logs if there is primary identity
  // from AuthenticationService and from IdentityManager.
  CHECK(identity) << "IdentityManager has primary identity: "
                  << hasPrimaryAccount << ", _signoutNotificationCalled: "
                  << _signoutNotificationCalled;
  // TODO(crbug.com/40068130): Record the history sync opt-in when the new
  // consent type will be available.
  syncer::SyncUserSettings* syncUserSettings = _syncService->GetUserSettings();
  syncUserSettings->SetSelectedType(syncer::UserSelectableType::kHistory, true);
  syncUserSettings->SetSelectedType(syncer::UserSelectableType::kTabs, true);
}

#pragma mark - Properties

- (void)setConsumer:(id<HistorySyncConsumer>)consumer {
  _consumer = consumer;
  if (!_consumer) {
    return;
  }
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!identity) {
    // This can happen if identity is removed from the device while the history
    // sync screen is open. There is no point in updating the UI since the
    // dialog will be automatically closed.
    return;
  }

  [self updateAvatarImageWithIdentity:identity];
  NSString* footerText =
      _showUserEmail
          ? l10n_util::GetNSStringF(
                IDS_IOS_HISTORY_SYNC_FOOTER_WITH_EMAIL,
                base::SysNSStringToUTF16(identity.userEmail))
          : l10n_util::GetNSString(IDS_IOS_HISTORY_SYNC_FOOTER_WITHOUT_EMAIL);
  [_consumer setFooterText:footerText];

  [self.consumer
      displayButtonsWithRestrictionCapability:
          [_capabilitiesFetcher canShowUnrestrictedOptInsCapability]];
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
  [self updateAvatarImageWithIdentity:identity];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/40284086): Remove `[self disconnect]`.
  [self disconnect];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    _signoutNotificationCalled = YES;
    [self.delegate historySyncMediatorPrimaryAccountCleared:self];
  }
}

#pragma mark - Private

// Updates the avatar image for the consumer from `identity`.
- (void)updateAvatarImageWithIdentity:(id<SystemIdentity>)identity {
  UIImage* image = _accountManagerService->GetIdentityAvatarWithIdentity(
      identity, IdentityAvatarSize::Large);
  [self.consumer setPrimaryIdentityAvatarImage:image];

  NSString* accessibilityLabel = nil;
  if (identity.userFullName.length == 0) {
    accessibilityLabel = identity.userEmail;
  } else {
    accessibilityLabel = [NSString
        stringWithFormat:@"%@ %@", identity.userFullName, identity.userEmail];
  }
  [self.consumer setPrimaryIdentityAvatarAccessibilityLabel:accessibilityLabel];
}

@end
