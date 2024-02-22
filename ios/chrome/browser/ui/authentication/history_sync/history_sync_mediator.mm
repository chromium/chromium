// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_mediator.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/unified_consent/unified_consent_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_observer_bridge.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/ui/authentication/account_capabilities_latency_tracker.h"
#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_consumer.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using signin::Tribool;

namespace {

// Fallback value for the capability
// CanShowHistorySyncOptInsWithoutMinorModeRestrictions if it is not available
// after `kMinorModeRestrictionsFetchDeadlineMs`.
const Tribool kCanShowUnrestrictedOptInsFallbackValue = Tribool::kFalse;

}  // namespace

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
  // Records the latency of capabilities fetch for this view.
  std::unique_ptr<signin::AccountCapabilitiesLatencyTracker>
      _accountCapabilitiesLatencyTracker;
  // Whether the history sync view buttons are updated.
  BOOL _actionButtonsUpdated;
  base::OneShotTimer _capabilitiesFetchTimer;
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
    _actionButtonsUpdated = NO;

    if (![self useMinorModeRestrictions]) {
      _accountCapabilitiesLatencyTracker =
          std::make_unique<signin::AccountCapabilitiesLatencyTracker>(
              identityManager);
    }
  }

  return self;
}

- (void)disconnect {
  _capabilitiesFetchTimer.Stop();
  _accountCapabilitiesLatencyTracker.reset();
  _accountManagerServiceObserver.reset();
  _identityManagerObserver.reset();
  _authenticationService = nullptr;
  _accountManagerService = nullptr;
  _identityManager = nullptr;
  _syncService = nullptr;
}

- (void)enableHistorySyncOptin {
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  CHECK(identity);
  // TODO(crbug.com/1467853): Record the history sync opt-in when the new
  // consent type will be available.
  syncer::SyncUserSettings* syncUserSettings = _syncService->GetUserSettings();
  syncUserSettings->SetSelectedType(syncer::UserSelectableType::kHistory, true);
  syncUserSettings->SetSelectedType(syncer::UserSelectableType::kTabs, true);
}

- (void)startFetchingCapabilities {
  if (![self useMinorModeRestrictions]) {
    return;
  }

  // The consumer must be present to receive updates based on successful
  // capabilities fetches.
  CHECK(self.consumer);

  // Manually fetch AccountInfo::capabilities and attempt to update buttons. The
  // capability might have been available and
  // onExtendedAccountInfoUpdated would not be triggered.
  CoreAccountInfo primaryAccount =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo accountInfo =
      _identityManager->FindExtendedAccountInfo(primaryAccount);
  [self
      processCanShowUnrestrictedOptInsCapability:
          accountInfo.capabilities
              .can_show_history_sync_opt_ins_without_minor_mode_restrictions()];

  if (!_actionButtonsUpdated) {
    // AccountInfo::capabilities is not immediately avaiable and might be
    // received through onExtendedAccountInfoUpdated. Start fetching system
    // capabilities.
    id<SystemIdentity> identity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    CHECK(identity);
    __weak __typeof(self) weakSelf = self;
    GetApplicationContext()
        ->GetSystemIdentityManager()
        ->CanShowHistorySyncOptInsWithoutMinorModeRestrictions(
            identity, base::BindOnce(^(SystemIdentityCapabilityResult result) {
              [weakSelf processCanShowUnrestrictedOptInsCapability:
                            signin::TriboolFromCapabilityResult(result)];
            }));
  }
}

#pragma mark - HistorySyncViewControllerAudience

- (void)viewAppearedWithHiddenButtons {
  // Set timeout with fallback capability value corresponding to fallback button
  // style.
  __weak __typeof(self) weakSelf = self;
  _capabilitiesFetchTimer.Start(
      FROM_HERE,
      base::Milliseconds(switches::kMinorModeRestrictionsFetchDeadlineMs.Get()),
      base::BindOnce(^{
        [weakSelf processCanShowUnrestrictedOptInsCapability:
                      kCanShowUnrestrictedOptInsFallbackValue];
      }));
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
}

#pragma mark - ChromeAccountManagerServiceObserver

- (void)identityUpdated:(id<SystemIdentity>)identity {
  [self updateAvatarImageWithIdentity:identity];
}

- (void)onChromeAccountManagerServiceShutdown:
    (ChromeAccountManagerService*)accountManagerService {
  // TODO(crbug.com/1489595): Remove `[self disconnect]`.
  [self disconnect];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kCleared) {
    [self.delegate historySyncMediatorPrimaryAccountCleared:self];
  }
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)info {
  if ([self useMinorModeRestrictions]) {
    [self
        processCanShowUnrestrictedOptInsCapability:
            info.capabilities
                .can_show_history_sync_opt_ins_without_minor_mode_restrictions()];
  } else {
    _accountCapabilitiesLatencyTracker->OnExtendedAccountInfoUpdated(info);
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

// Process the capability given by either the SystemIdentityManager,
// the IdentityManagerObserverBridge, or a fallback value.
- (void)processCanShowUnrestrictedOptInsCapability:(Tribool)capability {
  // With known capability value, update buttons visibility if not already
  // updated.
  if (capability != Tribool::kUnknown && !_actionButtonsUpdated) {
    // Stop timer.
    _capabilitiesFetchTimer.Stop();
    _actionButtonsUpdated = YES;
    // Equally weighted buttons are used for the restricted opt-in screen.
    BOOL isRestricted = (capability == Tribool::kFalse);
    [self.consumer displayButtonsWithRestrictionStatus:isRestricted];
  }
}

- (BOOL)useMinorModeRestrictions {
  return base::FeatureList::IsEnabled(
      switches::kMinorModeRestrictionsForHistorySyncOptIn);
}

@end
