// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/trust_token_access_details.h"

#include "services/network/public/mojom/trust_tokens.mojom.h"

namespace content {

TrustTokenAccessDetails::TrustTokenAccessDetails() = default;
TrustTokenAccessDetails::~TrustTokenAccessDetails() = default;

TrustTokenAccessDetails::TrustTokenAccessDetails(
    const url::Origin& origin,
    network::mojom::TrustTokenOperationType type,
    const std::optional<url::Origin>& issuer,
    bool blocked)
    : origin(origin), issuer(issuer), blocked(blocked) {}

TrustTokenAccessDetails::TrustTokenAccessDetails(
    const TrustTokenAccessDetails& details) = default;
TrustTokenAccessDetails& TrustTokenAccessDetails::operator=(
    const TrustTokenAccessDetails& details) = default;

TrustTokenAccessDetails::TrustTokenAccessDetails(
    const network::mojom::TrustTokenAccessDetailsPtr& details) {
  switch (details->which()) {
    case network::mojom::TrustTokenAccessDetails::Tag::kIssuance:
      type = network::mojom::TrustTokenOperationType::kIssuance;
      origin = details->get_issuance()->origin;
      issuer = details->get_issuance()->issuer;
      blocked = details->get_issuance()->blocked;
      break;
    case network::mojom::TrustTokenAccessDetails::Tag::kRedemption:
      type = network::mojom::TrustTokenOperationType::kRedemption;
      origin = details->get_redemption()->origin;
      issuer = details->get_redemption()->issuer;
      blocked = details->get_redemption()->blocked;
      break;
    case network::mojom::TrustTokenAccessDetails::Tag::kSigning:
      type = network::mojom::TrustTokenOperationType::kSigning;
      origin = details->get_signing()->origin;
      blocked = details->get_signing()->blocked;
      break;
  }
}

}  // namespace content
