// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_capabilities_fetcher.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/timer/timer.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/tribool.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/capabilities_types.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"

using signin::Tribool;

namespace {

// Fallback value for the capability
// CanShowHistorySyncOptInsWithoutMinorModeRestrictions if it is not available
// after `kMinorModeRestrictionsFetchDeadlineMs`.
const Tribool kCanShowUnrestrictedOptInsFallbackValue = Tribool::kFalse;

}  // namespace

@interface HistorySyncCapabilitiesFetcher () <
    IdentityManagerObserverBridgeDelegate>
@end

@implementation HistorySyncCapabilitiesFetcher {
  raw_ptr<AuthenticationService> _authenticationService;
  raw_ptr<signin::IdentityManager> _identityManager;
  // Observer for `IdentityManager`.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  CapabilityFetchCompletionCallback _callback;
  base::OneShotTimer _timer;
  BOOL _restrictionCapabilityReceived;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
                         callback:(CapabilityFetchCompletionCallback)callback {
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
    _callback = std::move(callback);
    CHECK(!_callback.is_null());
  }
  return self;
}

- (void)shutdown {
  _timer.Stop();
  _identityManagerObserver.reset();
  _authenticationService = nullptr;
  _identityManager = nullptr;
}

- (void)startFetchingRestrictionCapability {
  // Set timer with fallback capability value.
  __weak __typeof(self) weakSelf = self;
  _timer.Start(
      FROM_HERE,
      base::Milliseconds(switches::kMinorModeRestrictionsFetchDeadlineMs.Get()),
      base::BindOnce(^{
        [weakSelf onRestrictionCapabilityReceived:
                      kCanShowUnrestrictedOptInsFallbackValue];
      }));

  // Manually fetch AccountInfo::capabilities. The capability might have been
  // available and onExtendedAccountInfoUpdated would not be triggered.
  CoreAccountInfo primaryAccount =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo accountInfo =
      _identityManager->FindExtendedAccountInfo(primaryAccount);
  [self
      onRestrictionCapabilityReceived:
          accountInfo.capabilities
              .can_show_history_sync_opt_ins_without_minor_mode_restrictions()];

  if (!_restrictionCapabilityReceived) {
    // AccountInfo::capabilities is not immediately avaiable.
    // Start fetching system capabilities.
    id<SystemIdentity> identity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    CHECK(identity);
    GetApplicationContext()
        ->GetSystemIdentityManager()
        ->CanShowHistorySyncOptInsWithoutMinorModeRestrictions(
            identity, base::BindOnce(^(SystemIdentityCapabilityResult result) {
              [weakSelf onRestrictionCapabilityReceived:
                            signin::TriboolFromCapabilityResult(result)];
            }));
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)accountInfo {
  if (base::FeatureList::IsEnabled(
          switches::kMinorModeRestrictionsForHistorySyncOptIn)) {
    [self
        onRestrictionCapabilityReceived:
            accountInfo.capabilities
                .can_show_history_sync_opt_ins_without_minor_mode_restrictions()];
  }
}

#pragma mark - Private

- (void)onRestrictionCapabilityReceived:(Tribool)capability {
  if (capability != Tribool::kUnknown && !_restrictionCapabilityReceived) {
    _timer.Stop();
    _restrictionCapabilityReceived = YES;
    // Convert capability to boolean value.
    _callback.Run(capability == Tribool::kTrue);
  }
}

- (BOOL)useMinorModeRestrictions {
  return base::FeatureList::IsEnabled(
      switches::kMinorModeRestrictionsForHistorySyncOptIn);
}

@end
