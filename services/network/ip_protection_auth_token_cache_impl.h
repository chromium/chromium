// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_IP_PROTECTION_AUTH_TOKEN_CACHE_IMPL_H_
#define SERVICES_NETWORK_IP_PROTECTION_AUTH_TOKEN_CACHE_IMPL_H_

#include <deque>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/ip_protection_auth_token_cache.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

// An implementation of IpProtectionAuthTokenCache that fills itself by making
// IPC calls to the IpProtectionAuthTokenGetter in the browser process.
class COMPONENT_EXPORT(NETWORK_SERVICE) IpProtectionAuthTokenCacheImpl
    : public IpProtectionAuthTokenCache {
 public:
  // If `auth_token_getter` is unbound, no tokens will be provided.
  explicit IpProtectionAuthTokenCacheImpl(
      mojo::PendingRemote<network::mojom::IpProtectionAuthTokenGetter>
          auth_token_getter);
  ~IpProtectionAuthTokenCacheImpl() override;

  // IpProtectionAuthTokenCache implementation.
  void MayNeedAuthTokenSoon() override;
  absl::optional<network::mojom::BlindSignedAuthTokenPtr> GetAuthToken()
      override;

  // Set a callback to occur when the cache has been refilled after a call to
  // `MayNeedAuthTokenSoon()`. Note that this callback won't be called when
  // using `FillCacheForTesting()`, which instead takes a callback as a
  // parameter.
  void SetOnCacheRefilledForTesting(
      base::OnceCallback<void()> on_cache_refilled) {
    on_cache_refilled_ = std::move(on_cache_refilled);
  }

  // Requests tokens from the browser process and executes the provided callback
  // when tokens are available.
  void FillCacheForTesting(base::OnceCallback<void()> on_cache_refilled);

 private:
  void OnGotAuthTokens(
      absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
          tokens,
      absl::optional<base::Time> try_again_after);
  void RemoveExpiredTokens();
  void OnFilledCacheForTesting(
      base::OnceCallback<void()> on_cache_refilled,
      absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
          tokens,
      absl::optional<base::Time> try_again_after);
  void MeasureTokenRates();

  // The last time token rates were measured and the counts since then.
  base::TimeTicks last_token_rate_measurement_;
  int64_t tokens_spent_ = 0;
  int64_t tokens_expired_ = 0;

  // Cache of blind-signed auth tokens.
  std::deque<network::mojom::BlindSignedAuthTokenPtr> cache_;

  // Source of blind-signed auth tokens, when needed.
  mojo::Remote<network::mojom::IpProtectionAuthTokenGetter> auth_token_getter_;

  // True if an invocation of `auth_token_getter_.TryGetAuthTokens()` is
  // outstanding.
  bool currently_getting_ = false;

  // If not null, this is the `try_again_after` time from the last call to
  // `TryGetAuthTokens()`, and no calls should be made until this time.
  base::Time try_get_auth_tokens_after_;

  // A callback triggered when the asynchronous cache refill is complete, for
  // use in testing `MayNeedAuthTokenSoon()`. Note that this won't be called
  // when using `FillCacheForTesting()`, which instead takes a callback as a
  // parameter.
  base::OnceCallback<void()> on_cache_refilled_;

  base::RepeatingTimer measurement_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IpProtectionAuthTokenCacheImpl> weak_ptr_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_IP_PROTECTION_AUTH_TOKEN_CACHE_IMPL_H_
