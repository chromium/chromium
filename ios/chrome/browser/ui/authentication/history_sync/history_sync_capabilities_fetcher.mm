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
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"

using signin::Tribool;

namespace {

// Fallback value for the capability
// CanShowHistorySyncOptInsWithoutMinorModeRestrictions if it is not available
// after `kMinorModeRestrictionsFetchDeadline`.
const Tribool kCanShowUnrestrictedOptInsFallbackValue = Tribool::kUnknown;

}  // namespace

@interface HistorySyncCapabilitiesFetcher () <
    IdentityManagerObserverBridgeDelegate>
@end

@implementation HistorySyncCapabilitiesFetcher {
  raw_ptr<signin::IdentityManager> _identityManager;
  // Observer for `IdentityManager`.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  CapabilityFetchCompletionCallback _completionCallback;
  std::unique_ptr<base::ElapsedTimer> _fetchLatencyTimer;
  // Check that `onRestrictionCapabilityReceived` is called from the correct
  // sequence.
  SEQUENCE_CHECKER(_sequenceChecker);
}

- (instancetype)initWithIdentityManager:
    (signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
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
  _identityManager = nullptr;
}

- (void)startFetchingRestrictionCapabilityWithCallback:
    (CapabilityFetchCompletionCallback)callback {
  // Existing non-null callback should not be replaced.
  CHECK(_completionCallback.is_null());
  _completionCallback = std::move(callback);

  // Manually fetch AccountInfo::capabilities. The capability might have been
  // available and onExtendedAccountInfoUpdated would not be triggered.
  Tribool capability = [self canShowUnrestrictedOptInsCapability];

  if (capability != Tribool::kUnknown) {
    [self onRestrictionCapabilityReceived:capability];
    // The AccountInfo capability was immediately available and the callback was
    // executed.
    base::UmaHistogramBoolean("Signin.AccountCapabilities.ImmediatelyAvailable",
                              true);
    base::UmaHistogramTimes("Signin.AccountCapabilities.UserVisibleLatency",
                            base::Seconds(0));
  } else {
    // The AccountInfo capability was not available; start the latency timer.
    base::UmaHistogramBoolean("Signin.AccountCapabilities.ImmediatelyAvailable",
                              false);
    _fetchLatencyTimer = std::make_unique<base::ElapsedTimer>();

    // Set timeout with fallback capability value.
    __weak __typeof(self) weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf onRestrictionCapabilityReceived:
                        kCanShowUnrestrictedOptInsFallbackValue];
        }),
        kMinorModeRestrictionsFetchDeadline);
  }
}

- (Tribool)canShowUnrestrictedOptInsCapability {
  DCHECK(_identityManager);
  CoreAccountInfo primaryAccount =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  AccountInfo accountInfo =
      _identityManager->FindExtendedAccountInfo(primaryAccount);
  return accountInfo.capabilities
      .can_show_history_sync_opt_ins_without_minor_mode_restrictions();
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)accountInfo {
  signin::Tribool capability =
      accountInfo.capabilities
          .can_show_history_sync_opt_ins_without_minor_mode_restrictions();
  // Only process known capability values.
  if (capability != Tribool::kUnknown) {
    [self onRestrictionCapabilityReceived:capability];
  }
}

#pragma mark - Private

- (void)onRestrictionCapabilityReceived:(Tribool)capability {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);
  if (!_completionCallback.is_null()) {
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
    std::move(_completionCallback).Run(capability);
  }
}

@end
