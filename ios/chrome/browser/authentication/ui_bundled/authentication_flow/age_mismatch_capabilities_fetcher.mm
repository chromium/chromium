// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/age_mismatch_capabilities_fetcher.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/not_fatal_until.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"

using signin::CapabilityFetchCompletionCallback;
using signin::Tribool;

@interface AgeMismatchCapabilitiesFetcher () <
    IdentityManagerObserverBridgeDelegate>
@end

@implementation AgeMismatchCapabilitiesFetcher {
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  absl::flat_hash_map<CoreAccountId, CapabilityFetchCompletionCallback>
      _completionCallbacks;
  absl::flat_hash_map<CoreAccountId, base::TimeTicks> _fetchStartTimes;
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
  _completionCallbacks.clear();
  _fetchStartTimes.clear();
  _identityManagerObserver.reset();
  _identityManager = nullptr;
}

- (void)startFetchingCanSignInToChromeCapabilityWithCallback:
            (CapabilityFetchCompletionCallback)callback
                                                  forAccount:
                                                      (CoreAccountId)accountId {
  CHECK(_completionCallbacks.find(accountId) == _completionCallbacks.end());
  _completionCallbacks[accountId] = std::move(callback);
  _fetchStartTimes[accountId] = base::TimeTicks::Now();

  Tribool capability = [self canSignInToChromeCapabilityForAccount:accountId];

  if (capability != Tribool::kUnknown) {
    [self onCapabilityReceived:capability forAccountId:accountId];
  } else {
    // Set timeout with fallback capability value.
    __weak __typeof(self) weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            [](__typeof(self) strong_self, CoreAccountId account_id) {
              [strong_self onCapabilityReceived:Tribool::kUnknown
                                   forAccountId:account_id];
            },
            weakSelf, accountId),
        kCanSignInToChromeCapabilityFetchTimeout);
  }
}

- (Tribool)canSignInToChromeCapabilityForAccount:
    (const CoreAccountId&)accountId {
  CHECK(_identityManager);
  AccountInfo accountInfo =
      _identityManager->FindExtendedAccountInfoByAccountId(accountId);
  return accountInfo.capabilities.can_sign_in_to_chrome();
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onIdentityManagerShutdown:(signin::IdentityManager*)identityManager {
  _identityManager = nullptr;
  _completionCallbacks.clear();
  _fetchStartTimes.clear();
}

- (void)onExtendedAccountInfoUpdated:(const AccountInfo&)accountInfo {
  auto it = _completionCallbacks.find(accountInfo.account_id);
  if (it == _completionCallbacks.end()) {
    return;
  }
  signin::Tribool capability = accountInfo.capabilities.can_sign_in_to_chrome();
  if (capability != Tribool::kUnknown) {
    [self onCapabilityReceived:capability forAccountId:accountInfo.account_id];
  }
}

#pragma mark - Private

- (void)onCapabilityReceived:(Tribool)capability
                forAccountId:(const CoreAccountId&)accountId {
  DCHECK_CALLED_ON_VALID_SEQUENCE(_sequenceChecker);

  auto it = _completionCallbacks.find(accountId);
  if (it == _completionCallbacks.end()) {
    // The callback is already executed, either because
    // onExtendedAccountInfoUpdated was called before timeout, or vice versa.
    return;
  }

  // Record fetch metrics.
  auto start_time_it = _fetchStartTimes.find(accountId);
  CHECK(start_time_it != _fetchStartTimes.end(), base::NotFatalUntil::M155);
  base::TimeDelta duration = base::TimeTicks::Now() - start_time_it->second;
  base::UmaHistogramTimes(
      "Signin.AccountCapabilities.CanSignInToChrome.FetchDuration", duration);
  _fetchStartTimes.erase(start_time_it);

  CanSignInToChromeCapabilityResult result;
  switch (capability) {
    case Tribool::kFalse:
      result = CanSignInToChromeCapabilityResult::kFalse;
      break;
    case Tribool::kTrue:
      result = CanSignInToChromeCapabilityResult::kTrue;
      break;
    case Tribool::kUnknown:
      result = CanSignInToChromeCapabilityResult::kTimeout;
      break;
  }
  base::UmaHistogramEnumeration(
      "Signin.AccountCapabilities.CanSignInToChrome.FetchResult", result);

  CapabilityFetchCompletionCallback callback = std::move(it->second);
  _completionCallbacks.erase(it);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), capability));
}

@end
