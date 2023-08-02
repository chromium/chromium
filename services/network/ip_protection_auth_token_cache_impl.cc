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

// The first draft of this class gives the behavior planned for phase 0: fetch a
// batch of tokens on first use, and refresh that batch as necessary.
//
// The class API is designed to allow the implementation to get smarter without
// modifying the consumers of the API.

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
        auth_token_getter) {
  if (auth_token_getter.is_valid()) {
    auth_token_getter_.Bind(std::move(auth_token_getter));
  }

  last_token_rate_measurement_ = base::TimeTicks::Now();
  // Start the timer. The timer is owned by `this` and thus cannot outlive it.
  measurement_timer_.Start(FROM_HERE, kTokenRateMeasurementInterval, this,
                           &IpProtectionAuthTokenCacheImpl::MeasureTokenRates);
}

IpProtectionAuthTokenCacheImpl::~IpProtectionAuthTokenCacheImpl() = default;

void IpProtectionAuthTokenCacheImpl::MayNeedAuthTokenSoon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (currently_getting_ || !auth_token_getter_) {
    return;
  }

  if (!try_get_auth_tokens_after_.is_null() &&
      base::Time::Now() < try_get_auth_tokens_after_) {
    // We must continue to wait before calling `TryGetAuthTokens()` again,
    // so there is nothing we can do to refill the cache at this time.
    return;
  }

  RemoveExpiredTokens();
  if (cache_.size() < kCacheLowWaterMark) {
    currently_getting_ = true;
    auth_token_getter_->TryGetAuthTokens(
        kBatchSize,
        base::BindOnce(&IpProtectionAuthTokenCacheImpl::OnGotAuthTokens,
                       weak_ptr_factory_.GetWeakPtr()));
  }
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
  } else {
    try_get_auth_tokens_after_ = *try_again_after;
  }

  if (on_cache_refilled_) {
    std::move(on_cache_refilled_).Run();
  }
}

absl::optional<network::mojom::BlindSignedAuthTokenPtr>
IpProtectionAuthTokenCacheImpl::GetAuthToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveExpiredTokens();

  base::UmaHistogramBoolean("NetworkService.IpProtection.GetAuthTokenResult",
                            cache_.size() > 0);

  if (cache_.size() > 0) {
    auto result = std::move(cache_.front());
    cache_.pop_front();
    tokens_spent_++;
    return result;
  }
  return absl::nullopt;
}

void IpProtectionAuthTokenCacheImpl::RemoveExpiredTokens() {
  base::Time fresh_after = base::Time::Now() + kFreshnessConstant;
  auto size_before = cache_.size();
  cache_.erase(
      std::remove_if(
          cache_.begin(), cache_.end(),
          [&fresh_after](const network::mojom::BlindSignedAuthTokenPtr& token) {
            return token->expiration <= fresh_after;
          }),
      cache_.end());
  tokens_expired_ += size_before - cache_.size();
}

void IpProtectionAuthTokenCacheImpl::MeasureTokenRates() {
  RemoveExpiredTokens();

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

void IpProtectionAuthTokenCacheImpl::FillCacheForTesting(
    base::OnceCallback<void()> on_cache_refilled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(auth_token_getter_);
  auth_token_getter_->TryGetAuthTokens(
      kBatchSize,
      base::BindOnce(&IpProtectionAuthTokenCacheImpl::OnFilledCacheForTesting,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(on_cache_refilled)));
}

void IpProtectionAuthTokenCacheImpl::OnFilledCacheForTesting(
    base::OnceCallback<void()> on_cache_refilled,
    absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>> tokens,
    absl::optional<base::Time> try_again_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // For testing purposes, tokens should always be supplied.
  CHECK(tokens);
  cache_.insert(cache_.end(), std::make_move_iterator(tokens->begin()),
                std::make_move_iterator(tokens->end()));
  std::move(on_cache_refilled).Run();
}

}  // namespace network
