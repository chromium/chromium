// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection_auth_token_cache_impl.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"

namespace network {

namespace {

// Size of a "batch" of tokens to request in one attempt.
const int kBatchSize = 64;

// Cache size under which we will request new tokens.
const int kCacheLowWaterMark = 16;

// Additional time beyond which the token must be valid to be considered
// not "expired" by `RemoveExpiredTokens`.
const base::TimeDelta kFreshnessConstant = base::Seconds(5);

// Interval between measurements of the token rates.
const base::TimeDelta kTokenRateMeasurementInterval = base::Minutes(5);

}  // namespace

IpProtectionAuthTokenCacheImpl::IpProtectionAuthTokenCacheImpl(
    mojo::PendingRemote<network::mojom::IpProtectionAuthTokenGetter>
        auth_token_getter,
    bool disable_cache_management_for_testing)
    : disable_cache_management_for_testing_(
          disable_cache_management_for_testing) {
  if (auth_token_getter.is_valid()) {
    auth_token_getter_.Bind(std::move(auth_token_getter));
  }

  last_token_rate_measurement_ = base::TimeTicks::Now();
  // Start the timer. The timer is owned by `this` and thus cannot outlive it.
  measurement_timer_.Start(FROM_HERE, kTokenRateMeasurementInterval, this,
                           &IpProtectionAuthTokenCacheImpl::MeasureTokenRates);

  if (!disable_cache_management_for_testing) {
    // Schedule a call to `MaybeRefillCache()`. This will occur soon, since the
    // cache is empty.
    ScheduleMaybeRefillCache();
  }
}

IpProtectionAuthTokenCacheImpl::~IpProtectionAuthTokenCacheImpl() = default;

bool IpProtectionAuthTokenCacheImpl::IsAuthTokenAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RemoveExpiredTokens();
  return cache_.size() > 0;
}

// If this is a good time to request another batch of tokens, do so. This
// method is idempotent, and can be called at any time.
void IpProtectionAuthTokenCacheImpl::MaybeRefillCache() {
  RemoveExpiredTokens();
  if (currently_getting_ || !auth_token_getter_ ||
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

  if (cache_.size() < kCacheLowWaterMark) {
    currently_getting_ = true;
    auth_token_getter_->TryGetAuthTokens(
        kBatchSize,
        base::BindOnce(&IpProtectionAuthTokenCacheImpl::OnGotAuthTokens,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  ScheduleMaybeRefillCache();
}

// Schedule the next timed call to `MaybeRefillCache()`. This method is
// idempotent, and may be called at any time.
void IpProtectionAuthTokenCacheImpl::ScheduleMaybeRefillCache() {
  // If currently getting tokens, the call will be rescheduled when that
  // completes. If there's no getter, there's nothing to do.
  if (currently_getting_ || !auth_token_getter_ ||
      disable_cache_management_for_testing_) {
    next_maybe_refill_cache_.Stop();
    return;
  }

  base::Time now = base::Time::Now();
  base::TimeDelta delay;
  if (cache_.size() < kCacheLowWaterMark) {
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
      base::BindOnce(&IpProtectionAuthTokenCacheImpl::MaybeRefillCache,
                     weak_ptr_factory_.GetWeakPtr()));
}

void IpProtectionAuthTokenCacheImpl::OnGotAuthTokens(
    absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>> tokens,
    absl::optional<base::Time> try_again_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  currently_getting_ = false;
  if (tokens.has_value()) {
    try_get_auth_tokens_after_ = base::Time();
    cache_.insert(cache_.end(), std::make_move_iterator(tokens->begin()),
                  std::make_move_iterator(tokens->end()));
    std::sort(cache_.begin(), cache_.end(),
              [](network::mojom::BlindSignedAuthTokenPtr& a,
                 network::mojom::BlindSignedAuthTokenPtr& b) {
                return a->expiration < b->expiration;
              });
  } else {
    try_get_auth_tokens_after_ = *try_again_after;
  }

  if (on_cache_refilled_) {
    std::move(on_cache_refilled_).Run();
  }

  ScheduleMaybeRefillCache();
}

absl::optional<network::mojom::BlindSignedAuthTokenPtr>
IpProtectionAuthTokenCacheImpl::GetAuthToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveExpiredTokens();

  base::UmaHistogramBoolean("NetworkService.IpProtection.GetAuthTokenResult",
                            cache_.size() > 0);

  absl::optional<network::mojom::BlindSignedAuthTokenPtr> result;
  if (cache_.size() > 0) {
    result = std::move(cache_.front());
    cache_.pop_front();
    tokens_spent_++;
  }
  MaybeRefillCache();
  return result;
}

void IpProtectionAuthTokenCacheImpl::RemoveExpiredTokens() {
  base::Time fresh_after = base::Time::Now() + kFreshnessConstant;
  // Tokens are sorted, so only the first (soonest to expire) is important.
  while (cache_.size() > 0 && cache_[0]->expiration <= fresh_after) {
    cache_.pop_front();
    tokens_expired_++;
  }
  // Note that all uses of this method also generate a call to
  // `MaybeRefillCache()`, so there is no need to do so here.
}

void IpProtectionAuthTokenCacheImpl::MeasureTokenRates() {
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

// Call the getter's `TryGetAuthTokens()` and handle the result, calling
// `on_cache_refilled` when complete.
void IpProtectionAuthTokenCacheImpl::FillCacheForTesting(
    base::OnceClosure on_cache_refilled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(auth_token_getter_);
  CHECK(!on_cache_refilled_);
  on_cache_refilled_ = std::move(on_cache_refilled);
  auth_token_getter_->TryGetAuthTokens(
      kBatchSize,
      base::BindOnce(&IpProtectionAuthTokenCacheImpl::OnGotAuthTokens,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace network
