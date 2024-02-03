// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/url_constants.h"

namespace network {

SuitableTrustTokenOrigin::~SuitableTrustTokenOrigin() = default;
SuitableTrustTokenOrigin::SuitableTrustTokenOrigin(
    const SuitableTrustTokenOrigin& rhs) = default;
SuitableTrustTokenOrigin& SuitableTrustTokenOrigin::operator=(
    const SuitableTrustTokenOrigin& rhs) = default;
SuitableTrustTokenOrigin::SuitableTrustTokenOrigin(
    SuitableTrustTokenOrigin&& rhs) = default;
SuitableTrustTokenOrigin& SuitableTrustTokenOrigin::operator=(
    SuitableTrustTokenOrigin&& rhs) = default;

std::optional<SuitableTrustTokenOrigin> SuitableTrustTokenOrigin::Create(
    url::Origin origin) {
  if (origin.scheme() != url::kHttpsScheme &&
      origin.scheme() != url::kHttpScheme)
    return std::nullopt;
  if (!IsOriginPotentiallyTrustworthy(origin))
    return std::nullopt;

  return std::optional<SuitableTrustTokenOrigin>(
      std::in_place, base::PassKey<SuitableTrustTokenOrigin>(),
      std::move(origin));
}

std::optional<SuitableTrustTokenOrigin> SuitableTrustTokenOrigin::Create(
    const GURL& url) {
  return Create(url::Origin::Create(url));
}

std::string SuitableTrustTokenOrigin::Serialize() const {
  return origin_.Serialize();
}

SuitableTrustTokenOrigin::SuitableTrustTokenOrigin(
    base::PassKey<SuitableTrustTokenOrigin>,
    url::Origin&& origin)
    : origin_(std::move(origin)) {}

}  // namespace network
