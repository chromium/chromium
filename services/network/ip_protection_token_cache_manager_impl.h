// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_TOKEN_CACHE_MANAGER_IMPL_H_
#define SERVICES_NETWORK_IP_PROTECTION_TOKEN_CACHE_MANAGER_IMPL_H_

#include <deque>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/ip_protection_token_cache_manager.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

// An implementation of IpProtectionTokenCacheManager that populates itself
// using a passed in IpProtectionConfigGetter pointer from the cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionTokenCacheManagerImpl
    : public IpProtectionTokenCacheManager {
 public:
  explicit IpProtectionTokenCacheManagerImpl(
      mojo::Remote<network::mojom::IpProtectionConfigGetter>* config_getter,
      network::mojom::IpProtectionProxyLayer proxy_layer,
      bool disable_cache_management_for_testing = false);
  ~IpProtectionTokenCacheManagerImpl() override;

  // IpProtectionTokenCacheManager implementation.
  bool IsAuthTokenAvailable() override;
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> GetAuthToken()
      override;
  void InvalidateTryAgainAfterTime() override;

  // Set a callback that will be run after the next call to `TryGetAuthTokens()`
  // has completed.
  void SetOnTryGetAuthTokensCompletedForTesting(
      base::OnceClosure on_try_get_auth_tokens_completed) {
    on_try_get_auth_tokens_completed_for_testing_ =
        std::move(on_try_get_auth_tokens_completed);
  }

  // Enable active cache management in the background, if it was disabled
  // (either via the constructor or via a call to
  // `DisableCacheManagementForTesting()`).
  void EnableCacheManagementForTesting() {
    disable_cache_management_for_testing_ = false;
    ScheduleMaybeRefillCache();
  }

  bool IsCacheManagementEnabledForTesting() {
    return !disable_cache_management_for_testing_;
  }

  void DisableCacheManagementForTesting(
      base::OnceClosure on_cache_management_disabled);

  // Requests tokens from the browser process and executes the provided callback
  // after the response is received.
  void CallTryGetAuthTokensForTesting();

  base::Time try_get_auth_tokens_after_for_testing() {
    return try_get_auth_tokens_after_;
  }

  bool fetching_auth_tokens_for_testing() { return fetching_auth_tokens_; }

 private:
  void OnGotAuthTokens(
      absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
          tokens,
      absl::optional<base::Time> try_again_after);
  void RemoveExpiredTokens();
  void MeasureTokenRates();
  void MaybeRefillCache();
  void ScheduleMaybeRefillCache();

  // Batch size and cache low-water mark as determined from feature params at
  // construction time.
  const int batch_size_;
  const size_t cache_low_water_mark_;

  // The last time token rates were measured and the counts since then.
  base::TimeTicks last_token_rate_measurement_;
  int64_t tokens_spent_ = 0;
  int64_t tokens_expired_ = 0;

  // Cache of blind-signed auth tokens. Tokens are sorted by their expiration
  // time.
  std::deque<network::mojom::BlindSignedAuthTokenPtr> cache_;

  // Source of proxy list, when needed.
  raw_ptr<mojo::Remote<network::mojom::IpProtectionConfigGetter>>
      config_getter_;

  // The proxy layer which the cache of tokens will be used for.
  network::mojom::IpProtectionProxyLayer proxy_layer_;

  // True if an invocation of `config_getter_.TryGetAuthTokens()` is
  // outstanding.
  bool fetching_auth_tokens_ = false;

  // If not null, this is the `try_again_after` time from the last call to
  // `TryGetAuthTokens()`, and no calls should be made until this time.
  base::Time try_get_auth_tokens_after_;
  // A timer to run `MaybeRefillCache()` when necessary, such as when the next
  // token expires or the cache is able to fetch more tokens.
  base::OneShotTimer next_maybe_refill_cache_;

  // A callback triggered when the next call to `TryGetAuthTokens()` occurs, for
  // use in testing.
  base::OnceClosure on_try_get_auth_tokens_completed_for_testing_;

  // If true, do not try to automatically refill the cache.
  bool disable_cache_management_for_testing_ = false;

  base::RepeatingTimer measurement_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IpProtectionTokenCacheManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_TOKEN_CACHE_MANAGER_IMPL_H_
