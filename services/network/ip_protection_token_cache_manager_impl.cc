// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection_token_cache_manager_impl.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace network {

namespace {

// Additional time beyond which the token must be valid to be considered
// not "expired" by `RemoveExpiredTokens`.
const base::TimeDelta kFreshnessConstant = base::Seconds(5);

// Interval between measurements of the token rates.
const base::TimeDelta kTokenRateMeasurementInterval = base::Minutes(5);

}  // namespace

IpProtectionTokenCacheManagerImpl::IpProtectionTokenCacheManagerImpl(
    mojo::Remote<network::mojom::IpProtectionConfigGetter>* config_getter,
    network::mojom::IpProtectionProxyLayer proxy_layer,
    bool disable_cache_management_for_testing)
    : batch_size_(net::features::kIpPrivacyAuthTokenCacheBatchSize.Get()),
      cache_low_water_mark_(
          net::features::kIpPrivacyAuthTokenCacheLowWaterMark.Get()),
      config_getter_(config_getter),
      proxy_layer_(proxy_layer),
      disable_cache_management_for_testing_(
          disable_cache_management_for_testing) {
  last_token_rate_measurement_ = base::TimeTicks::Now();
  // Start the timer. The timer is owned by `this` and thus cannot outlive it.
  measurement_timer_.Start(
      FROM_HERE, kTokenRateMeasurementInterval, this,
      &IpProtectionTokenCacheManagerImpl::MeasureTokenRates);

  if (!disable_cache_management_for_testing_) {
    // Schedule a call to `MaybeRefillCache()`. This will occur soon, since the
    // cache is empty.
    ScheduleMaybeRefillCache();
  }
}

IpProtectionTokenCacheManagerImpl::~IpProtectionTokenCacheManagerImpl() =
    default;

bool IpProtectionTokenCacheManagerImpl::IsAuthTokenAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RemoveExpiredTokens();
  return cache_.size() > 0;
}

// If this is a good time to request another batch of tokens, do so. This
// method is idempotent, and can be called at any time.
void IpProtectionTokenCacheManagerImpl::MaybeRefillCache() {
  RemoveExpiredTokens();
  if (fetching_auth_tokens_ || !config_getter_ ||
      disable_cache_management_for_testing_) {
    return;
  }

  if (!try_get_auth_tokens_after_.is_null() &&
      base::Time::Now() < try_get_auth_tokens_after_) {
    // We must continue to wait before calling `TryGetAuthTokens()` again, so
    // there is nothing we can do to refill the cache at this time. The
    // `next_maybe_refill_cache_` timer is probably already set, but an extra
    // call to `ScheduleMaybeRefillCache()` doesn't hurt.
    ScheduleMaybeRefillCache();
    return;
  }

  if (cache_.size() < cache_low_water_mark_) {
    fetching_auth_tokens_ = true;
    VLOG(2) << "IPPATC::MaybeRefillCache calling TryGetAuthTokens";
    config_getter_->get()->TryGetAuthTokens(
        batch_size_, proxy_layer_,
        base::BindOnce(&IpProtectionTokenCacheManagerImpl::OnGotAuthTokens,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  ScheduleMaybeRefillCache();
}

void IpProtectionTokenCacheManagerImpl::InvalidateTryAgainAfterTime() {
  try_get_auth_tokens_after_ = base::Time();
  ScheduleMaybeRefillCache();
}

// Schedule the next timed call to `MaybeRefillCache()`. This method is
// idempotent, and may be called at any time.
void IpProtectionTokenCacheManagerImpl::ScheduleMaybeRefillCache() {
  // If currently getting tokens, the call will be rescheduled when that
  // completes. If there's no getter, there's nothing to do.
  if (fetching_auth_tokens_ || !config_getter_ ||
      disable_cache_management_for_testing_) {
    next_maybe_refill_cache_.Stop();
    return;
  }

  base::Time now = base::Time::Now();
  base::TimeDelta delay;
  if (cache_.size() < cache_low_water_mark_) {
    // If the cache is below the low-water mark, call now or (more likely) at
    // the requested backoff time.
    if (try_get_auth_tokens_after_.is_null()) {
      delay = base::TimeDelta();
    } else {
      delay = try_get_auth_tokens_after_ - now;
    }
  } else {
    // Call when the next token expires.
    delay = cache_[0]->expiration - kFreshnessConstant - now;
  }

  if (delay.is_negative()) {
    delay = base::TimeDelta();
  }

  next_maybe_refill_cache_.Start(
      FROM_HERE, delay,
      base::BindOnce(&IpProtectionTokenCacheManagerImpl::MaybeRefillCache,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IpProtectionTokenCacheManagerImpl::OnGotAuthTokens(
    absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>> tokens,
    absl::optional<base::Time> try_again_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  fetching_auth_tokens_ = false;
  if (tokens.has_value()) {
    VLOG(2) << "IPPATC::OnGotAuthTokens got " << tokens->size() << " tokens";
    try_get_auth_tokens_after_ = base::Time();
    cache_.insert(cache_.end(), std::make_move_iterator(tokens->begin()),
                  std::make_move_iterator(tokens->end()));
    std::sort(cache_.begin(), cache_.end(),
              [](network::mojom::BlindSignedAuthTokenPtr& a,
                 network::mojom::BlindSignedAuthTokenPtr& b) {
                return a->expiration < b->expiration;
              });
  } else {
    VLOG(2) << "IPPATC::OnGotAuthTokens back off until " << *try_again_after;
    try_get_auth_tokens_after_ = *try_again_after;
  }

  if (on_try_get_auth_tokens_completed_for_testing_) {
    std::move(on_try_get_auth_tokens_completed_for_testing_).Run();
  }

  ScheduleMaybeRefillCache();
}

absl::optional<network::mojom::BlindSignedAuthTokenPtr>
IpProtectionTokenCacheManagerImpl::GetAuthToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveExpiredTokens();

  base::UmaHistogramBoolean("NetworkService.IpProtection.GetAuthTokenResult",
                            cache_.size() > 0);
  VLOG(2) << "IPPATC::GetAuthToken with " << cache_.size()
          << " tokens available";

  absl::optional<network::mojom::BlindSignedAuthTokenPtr> result;
  if (cache_.size() > 0) {
    result = std::move(cache_.front());
    cache_.pop_front();
    tokens_spent_++;
  }
  MaybeRefillCache();
  return result;
}

void IpProtectionTokenCacheManagerImpl::RemoveExpiredTokens() {
  base::Time fresh_after = base::Time::Now() + kFreshnessConstant;
  // Tokens are sorted, so only the first (soonest to expire) is important.
  while (cache_.size() > 0 && cache_[0]->expiration <= fresh_after) {
    cache_.pop_front();
    tokens_expired_++;
  }
  // Note that all uses of this method also generate a call to
  // `MaybeRefillCache()`, so there is no need to do so here.
}

void IpProtectionTokenCacheManagerImpl::MeasureTokenRates() {
  auto now = base::TimeTicks::Now();
  auto interval = now - last_token_rate_measurement_;
  auto interval_ms = interval.InMilliseconds();

  auto denominator = base::Hours(1).InMilliseconds();
  if (interval_ms != 0) {
    last_token_rate_measurement_ = now;

    auto spend_rate = tokens_spent_ * denominator / interval_ms;
    // A maximum of 1000 would correspond to a spend rate of about 16/min,
    // which is higher than we expect to see.
    base::UmaHistogramCounts1000("NetworkService.IpProtection.TokenSpendRate",
                                 spend_rate);

    auto expiration_rate = tokens_expired_ * denominator / interval_ms;
    // Entire batches of tokens are likely to expire within a single 5-minute
    // measurement interval. 1024 tokens in 5 minutes is equivalent to 12288
    // tokens per hour, comfortably under 100,000.
    base::UmaHistogramCounts100000(
        "NetworkService.IpProtection.TokenExpirationRate", expiration_rate);
  }

  last_token_rate_measurement_ = now;
  tokens_spent_ = 0;
  tokens_expired_ = 0;
}

void IpProtectionTokenCacheManagerImpl::DisableCacheManagementForTesting(
    base::OnceClosure on_cache_management_disabled) {
  disable_cache_management_for_testing_ = true;
  ScheduleMaybeRefillCache();

  if (fetching_auth_tokens_) {
    // If a `TryGetAuthTokens()` call is underway (due to active cache
    // management), wait for it to finish.
    SetOnTryGetAuthTokensCompletedForTesting(  // IN-TEST
        std::move(on_cache_management_disabled));
    return;
  }
  std::move(on_cache_management_disabled).Run();
}

// Call `TryGetAuthTokens()`, which will call
// `on_try_get_auth_tokens_completed_for_testing_` when complete.
void IpProtectionTokenCacheManagerImpl::CallTryGetAuthTokensForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config_getter_);
  CHECK(on_try_get_auth_tokens_completed_for_testing_);
  config_getter_->get()->TryGetAuthTokens(
      batch_size_, proxy_layer_,
      base::BindOnce(&IpProtectionTokenCacheManagerImpl::OnGotAuthTokens,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace network
