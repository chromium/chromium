// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/history_sync/history_sync_capabilities_fetcher.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/timer/elapsed_timer.h"
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

// Short timeout to wait for asynchronously fetching already available system
// capabilities.
constexpr base::TimeDelta kFetchImmediatelyAvailableCapabilityDeadline =
    base::Milliseconds(20);

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
  CapabilityFetchCompletionCallback _completionCallback;
  std::unique_ptr<base::ElapsedTimer> _fetchLatencyTimer;
  // Whether capabilities fetch latency metrics should be recorded.
  BOOL _shouldRecordLatencyMetrics;
  // Check that `onRestrictionCapabilityReceived` is called from the correct
  // sequence.
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                                self);
  }
  return self;
}

- (void)shutdown {
  _completionCallback = CapabilityFetchCompletionCallback();
  _identityManagerObserver.reset();
  _authenticationService = nullptr;
  _identityManager = nullptr;
}

- (void)startFetchingRestrictionCapabilityWithCallback:
    (CapabilityFetchCompletionCallback)callback {
  // Existing non-null callback should not be replaced.
  CHECK(_completionCallback.is_null());
  _completionCallback = std::move(callback);
  _shouldRecordLatencyMetrics = YES;
  [self fetchRestrictionCapabilityWithTimeout:
            base::Milliseconds(
                switches::kMinorModeRestrictionsFetchDeadlineMs.Get())];
}

- (void)fetchImmediatelyAvailableRestrictionCapabilityWithCallback:
    (CapabilityFetchCompletionCallback)callback {
  // Existing non-null callback should not be replaced.
  CHECK(_completionCallback.is_null());
  _completionCallback = std::move(callback);
  _shouldRecordLatencyMetrics = NO;
  // System capabilities cannot be fetched synchronously. A short, non-zero
  // timeout is used to terminate async system capabilities fetch.
  [self fetchRestrictionCapabilityWithTimeout:
            kFetchImmediatelyAvailableCapabilityDeadline];
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

- (void)fetchRestrictionCapabilityWithTimeout:(base::TimeDelta)timeout {
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

  if (_completionCallback.is_null()) {
    // The AccountInfo capability was immediately available and the callback was
    // executed.
    [self recordImmediateAvailabilityMetrics];
  } else {
    // The AccountInfo capability was not yet available.
    [self startLatencyTimer];

    // Start fetching system capabilities.
    if (base::FeatureList::IsEnabled(
            switches::kUseSystemCapabilitiesForMinorModeRestrictions)) {
      [self fetchSystemCapabilities];
    }
    // Set timeout with fallback capability value.
    __weak __typeof(self) weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf onRestrictionCapabilityReceived:
                        kCanShowUnrestrictedOptInsFallbackValue];
        }),
        timeout);
  }
}

- (void)fetchSystemCapabilities {
  __weak __typeof(self) weakSelf = self;
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  CHECK(identity);
  GetApplicationContext()
      ->GetSystemIdentityManager()
      ->CanShowHistorySyncOptInsWithoutMinorModeRestrictions(
          identity, base::BindOnce(^(SystemIdentityCapabilityResult result) {
            [weakSelf onRestrictionCapabilityReceived:
                          signin::TriboolFromCapabilityResult(result)];
          }));
}

- (void)recordImmediateAvailabilityMetrics {
  if (_shouldRecordLatencyMetrics) {
    base::UmaHistogramBoolean("Signin.AccountCapabilities.ImmediatelyAvailable",
                              true);
    base::UmaHistogramTimes("Signin.AccountCapabilities.UserVisibleLatency",
                            base::Seconds(0));
  }
}

- (void)startLatencyTimer {
  if (_shouldRecordLatencyMetrics) {
    // Start the latency timer.
    base::UmaHistogramBoolean("Signin.AccountCapabilities.ImmediatelyAvailable",
                              false);
    _fetchLatencyTimer = std::make_unique<base::ElapsedTimer>();
  }
}

- (void)onRestrictionCapabilityReceived:(Tribool)capability {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (capability != Tribool::kUnknown && !_completionCallback.is_null()) {
    // Stop listening to AccountInfo updates.
    _identityManagerObserver.reset();
    // Record latency metrics
    if (_fetchLatencyTimer) {
      base::TimeDelta elapsed = _fetchLatencyTimer->Elapsed();
      base::UmaHistogramTimes("Signin.AccountCapabilities.UserVisibleLatency",
                              elapsed);
      base::UmaHistogramTimes("Signin.AccountCapabilities.FetchLatency",
                              elapsed);
    }
    // Convert capability to boolean value.
    std::move(_completionCallback).Run(capability == Tribool::kTrue);
  }
}

@end
