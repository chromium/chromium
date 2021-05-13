// Copyright 2020 The Chromium Authors. All rights reserved.
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

base::Optional<SuitableTrustTokenOrigin> SuitableTrustTokenOrigin::Create(
    url::Origin origin) {
  if (origin.scheme() != url::kHttpsScheme &&
      origin.scheme() != url::kHttpScheme)
    return base::nullopt;
  if (!IsOriginPotentiallyTrustworthy(origin))
    return base::nullopt;

  return base::Optional<SuitableTrustTokenOrigin>(
      base::in_place, base::PassKey<SuitableTrustTokenOrigin>(),
      std::move(origin));
}

base::Optional<SuitableTrustTokenOrigin> SuitableTrustTokenOrigin::Create(
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
