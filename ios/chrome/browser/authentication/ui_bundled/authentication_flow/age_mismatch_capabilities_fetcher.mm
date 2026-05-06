// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/authentication_flow/age_mismatch_capabilities_fetcher.h"

#import "base/check.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/task/sequenced_task_runner.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"

using signin::CapabilityFetchCompletionCallback;
using signin::Tribool;

namespace {

// Fetch timeout for the `can_sign_in_to_chrome` capability, based on the
// existing timeout for fetching minor mode restrictions.
constexpr base::TimeDelta kCanSignInToChromeCapabilityFetchTimeout =
    kMinorModeRestrictionsFetchDeadline;

}  // namespace

@interface AgeMismatchCapabilitiesFetcher () <
    IdentityManagerObserverBridgeDelegate>
@end

@implementation AgeMismatchCapabilitiesFetcher {
  raw_ptr<signin::IdentityManager> _identityManager;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;
  absl::flat_hash_map<CoreAccountId, CapabilityFetchCompletionCallback>
      _completionCallbacks;
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
  _identityManagerObserver.reset();
  _identityManager = nullptr;
}

- (void)startFetchingCanSignInToChromeCapabilityWithCallback:
            (CapabilityFetchCompletionCallback)callback
                                                  forAccount:
                                                      (CoreAccountId)accountId {
  CHECK(_completionCallbacks.find(accountId) == _completionCallbacks.end());
  _completionCallbacks[accountId] = std::move(callback);

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
  if (it != _completionCallbacks.end()) {
    CapabilityFetchCompletionCallback callback = std::move(it->second);
    _completionCallbacks.erase(it);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), capability));
  }
}

@end
