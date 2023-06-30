// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ip_protection_auth_token_cache_impl.h"
#include "base/time/time.h"

namespace network {

namespace {

// The first draft of this class gives the behavior planned for phase 0: fetch a
// batch of tokens on first use, and refresh that batch as necessary.
//
// The class API is designed to allow the implementation to get smarter without
// modifying the consumers of the API.

// Size of a "batch" of tokens to request in one attempt.
const int kBatchSize = 1024;

// Cache size under which we will request new tokens.
const int kCacheLowWaterMark = 256;

// Additional time beyond which the token must be valid to be considered
// not "expired" by `RemoveExpiredTokens`.
const base::TimeDelta kFreshnessConstant = base::Seconds(5);

}  // namespace

IpProtectionAuthTokenCacheImpl::IpProtectionAuthTokenCacheImpl(
    mojo::PendingRemote<network::mojom::IpProtectionAuthTokenGetter>
        auth_token_getter) {
  if (auth_token_getter.is_valid()) {
    auth_token_getter_.Bind(std::move(auth_token_getter));
  }
}

IpProtectionAuthTokenCacheImpl::~IpProtectionAuthTokenCacheImpl() = default;

void IpProtectionAuthTokenCacheImpl::MayNeedAuthTokenSoon() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (currently_getting_ || !auth_token_getter_) {
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
    absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>
        tokens) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  currently_getting_ = false;
  if (tokens) {
    cache_.insert(cache_.end(), std::make_move_iterator(tokens->begin()),
                  std::make_move_iterator(tokens->end()));
  }

  if (on_cache_refilled_) {
    std::move(on_cache_refilled_).Run();
  }
}

absl::optional<network::mojom::BlindSignedAuthTokenPtr>
IpProtectionAuthTokenCacheImpl::GetAuthToken() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RemoveExpiredTokens();
  if (cache_.size() > 0) {
    auto result = std::move(cache_.front());
    cache_.pop_front();
    return result;
  }
  return absl::nullopt;
}

void IpProtectionAuthTokenCacheImpl::RemoveExpiredTokens() {
  base::Time fresh_after = base::Time::Now() + kFreshnessConstant;
  cache_.erase(
      std::remove_if(
          cache_.begin(), cache_.end(),
          [&fresh_after](const network::mojom::BlindSignedAuthTokenPtr& token) {
            return token->expiration <= fresh_after;
          }),
      cache_.end());
}

}  // namespace network
