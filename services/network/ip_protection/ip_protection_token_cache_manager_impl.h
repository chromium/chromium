// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_TOKEN_CACHE_MANAGER_IMPL_H_
#define SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_TOKEN_CACHE_MANAGER_IMPL_H_

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "services/network/ip_protection/ip_protection_config_cache.h"
#include "services/network/ip_protection/ip_protection_config_getter.h"
#include "services/network/ip_protection/ip_protection_data_types.h"
#include "services/network/ip_protection/ip_protection_token_cache_manager.h"

namespace network {

// An implementation of IpProtectionTokenCacheManager that populates itself
// using a passed in IpProtectionConfigGetter pointer from the cache.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionTokenCacheManagerImpl
    : public IpProtectionTokenCacheManager {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(AuthTokenResultForGeo)
  enum class AuthTokenResultForGeo {
    kUnavailableCacheEmpty = 0,
    kUnavailableButCacheContainsTokens = 1,
    kAvailableForCurrentGeo = 2,
    kAvailableForOtherCachedGeo = 3,
    kMaxValue = kAvailableForOtherCachedGeo,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/network/enums.xml:IpProtectionGetAuthTokenResultForGeo)

  explicit IpProtectionTokenCacheManagerImpl(
      IpProtectionConfigCache* config_cache,
      IpProtectionConfigGetter* config_getter,
      IpProtectionProxyLayer proxy_layer,
      bool disable_cache_management_for_testing = false);
  ~IpProtectionTokenCacheManagerImpl() override;

  // IpProtectionTokenCacheManager implementation.
  bool IsAuthTokenAvailable() override;
  bool IsAuthTokenAvailable(const std::string& geo_id) override;
  std::optional<BlindSignedAuthToken> GetAuthToken() override;
  std::optional<BlindSignedAuthToken> GetAuthToken(
      const std::string& geo_id) override;
  std::string CurrentGeo() const override;
  void SetCurrentGeo(const std::string& geo_id) override;
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

  void EnableTokenExpirationFuzzingForTesting(bool enable);

  // Requests tokens from the browser process and executes the provided callback
  // after the response is received.
  void CallTryGetAuthTokensForTesting();

  base::Time try_get_auth_tokens_after_for_testing() {
    return try_get_auth_tokens_after_;
  }

  bool fetching_auth_tokens_for_testing() { return fetching_auth_tokens_; }

 private:
  void OnGotAuthTokens(base::TimeTicks attempt_start_time_for_metrics,
                       std::optional<std::vector<BlindSignedAuthToken>> tokens,
                       std::optional<base::Time> try_again_after);
  void RemoveExpiredTokens();
  void MeasureTokenRates();
  void MaybeRefillCache();
  void ScheduleMaybeRefillCache();
  bool NeedsRefill(const std::string& geo_id) const;
  bool IsTokenLimitExceeded(const std::string& geo_id) const;

  // Current geo of the client.
  // This value should only be set by the `IpProtectionConfigCache` using the
  // `IpProtectionTokenCacheManager::SetCurrentGeo()` function.
  std::string current_geo_id_ = "";

  // Batch size and cache low-water mark as determined from feature params at
  // construction time.
  const int batch_size_;
  const size_t cache_low_water_mark_;

  // Feature flag to safely introduce token caching by geo.
  bool enable_token_caching_by_geo_ = false;

  // The last time token rates were measured and the counts since then.
  base::TimeTicks last_token_rate_measurement_;
  int64_t tokens_spent_ = 0;
  int64_t tokens_expired_ = 0;

  // Map for caches of tokens keyed by geo id. For each geo entry, tokens are
  // sorted by their expiration time.
  std::map<std::string, std::deque<BlindSignedAuthToken>> cache_by_geo_;

  // Source of proxy list, when needed.
  raw_ptr<IpProtectionConfigGetter> config_getter_;

  // The proxy layer which the cache of tokens will be used for.
  IpProtectionProxyLayer proxy_layer_;

  // Pointer to the `IpProtectionConfigCache` that holds the proxy list and
  // tokens. Required to observe geo changes from retrieved tokens.
  // The lifetime of the `IpProtectionConfigCache` object WILL ALWAYS outlive
  // this class b/c `ip_protection_config_cache_` owns this (at least outside of
  // testing).
  const raw_ptr<IpProtectionConfigCache> ip_protection_config_cache_;

  // True if an invocation of `config_getter_.TryGetAuthTokens()` is
  // outstanding.
  bool fetching_auth_tokens_ = false;

  // True if the "NetworkService.IpProtection.GeoChangeTokenPresence" metric
  // needs to be sampled. False if the presence has already been sampled. This
  // value should be reset to true after `SetCurrentGeo` is called after a token
  // refill.
  // A boolean flag is crucial to prevent duplicate or incorrect histogram
  // measurements. By tracking whether a histogram has already been logged for a
  // given geo change, we can avoid redundant or misleading data which could be
  // caused b/c a cache is filled with tokens before a call to `SetCurrentGeo`
  // is made.
  bool emitted_geo_presence_histogram_before_refill_ = true;

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

  // If false, token expiration is not fuzzed.
  bool enable_token_expiration_fuzzing_for_testing_ = true;

  base::RepeatingTimer measurement_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IpProtectionTokenCacheManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_IP_PROTECTION_TOKEN_CACHE_MANAGER_IMPL_H_
