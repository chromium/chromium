// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_CONFIG_CACHE_IMPL_H_
#define SERVICES_NETWORK_IP_PROTECTION_CONFIG_CACHE_IMPL_H_

#include <deque>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/ip_protection_config_cache.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

// An implementation of IpProtectionConfigCache that fills itself by making
// IPC calls to the IpProtectionConfigGetter in the browser process.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionConfigCacheImpl
    : public IpProtectionConfigCache {
 public:
  // If `auth_token_getter` is unbound, no tokens will be provided.
  explicit IpProtectionConfigCacheImpl(
      mojo::PendingRemote<network::mojom::IpProtectionConfigGetter>
          auth_token_getter,
      bool disable_background_tasks_for_testing = false);
  ~IpProtectionConfigCacheImpl() override;

  // IpProtectionConfigCache implementation.
  bool IsAuthTokenAvailable() override;
  bool IsProxyListAvailable() override;
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> GetAuthToken()
      override;
  const std::vector<std::string>& ProxyList() override;
  void RequestRefreshProxyList() override;

  // Set a callback to occur when the cache has been refilled after a call to
  // `MayNeedAuthTokenSoon()`. Note that this callback won't be called when
  // using `FillCacheForTesting()`, which instead takes a callback as a
  // parameter.
  void SetOnCacheRefilledForTesting(base::OnceClosure on_cache_refilled) {
    on_cache_refilled_for_testing_ = std::move(on_cache_refilled);
  }

  // Set a callback to occur when the proxy list has been refreshed.
  void SetOnProxyListRefreshedForTesting(
      base::OnceClosure on_proxy_list_refreshed) {
    on_proxy_list_refreshed_for_testing_ = std::move(on_proxy_list_refreshed);
  }

  // Enable active cache management in the background, if it was disabled in the
  // constructor.
  void EnableCacheManagementForTesting() {
    disable_cache_management_for_testing_ = false;
    ScheduleMaybeRefillCache();
  }

  void EnableProxyListRefreshingForTesting() {
    disable_proxy_refreshing_for_testing_ = false;
    RefreshProxyList();
  }

  // Requests tokens from the browser process and executes the provided callback
  // when tokens are available.
  void FillCacheForTesting(base::OnceClosure on_cache_refilled);

 private:
  void OnGotAuthTokens(
      absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
          tokens,
      absl::optional<base::Time> try_again_after);
  void RemoveExpiredTokens();
  void MeasureTokenRates();
  void MaybeRefillCache();
  void ScheduleMaybeRefillCache();

  void RefreshProxyList();
  void OnGotProxyList(const absl::optional<std::vector<std::string>>&);

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

  // Latest fetched proxy list.
  std::vector<std::string> proxy_list_;

  // Source of auth tokens and proxy list, when needed.
  mojo::Remote<network::mojom::IpProtectionConfigGetter> auth_token_getter_;

  // True if an invocation of `auth_token_getter_.TryGetAuthTokens()` is
  // outstanding.
  bool fetching_auth_tokens_ = false;

  // True if an invocation of `auth_token_getter_.GetProxyList()` is
  // outstanding.
  bool fetching_proxy_list_ = false;

  // True if the proxy list has been fetched at least once.
  bool have_fetched_proxy_list_ = false;

  // If not null, this is the `try_again_after` time from the last call to
  // `TryGetAuthTokens()`, and no calls should be made until this time.
  base::Time try_get_auth_tokens_after_;

  // The last time this instance began refreshing the proxy list.
  base::Time last_proxy_list_refresh_;

  // A timer to run `MaybeRefillCache()` when necessary, such as when the next
  // token expires or the cache is able to fetch more tokens.
  base::OneShotTimer next_maybe_refill_cache_;

  // A timer to run `RefreshProxyList()` when necessary.
  base::OneShotTimer next_refresh_proxy_list_;

  // A callback triggered when an asynchronous cache refill is complete, for use
  // in testing.
  base::OnceClosure on_cache_refilled_for_testing_;

  // A callback triggered when an asynchronous proxy-list refresh is complete,
  // for use in testing.
  base::OnceClosure on_proxy_list_refreshed_for_testing_;

  // If true, do not try to automatically refill the cache.
  bool disable_cache_management_for_testing_ = false;

  // If true, do not try to automatically refresh the proxy list.
  bool disable_proxy_refreshing_for_testing_ = false;

  base::RepeatingTimer measurement_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IpProtectionConfigCacheImpl> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_CONFIG_CACHE_IMPL_H_
