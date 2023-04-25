// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/boringssl_trust_token_state.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network {

std::unique_ptr<BoringsslTrustTokenState> BoringsslTrustTokenState::Create(
    mojom::TrustTokenProtocolVersion issuer_configured_version,
    int issuer_configured_batch_size) {
  if (!base::IsValueInRangeForNumericType<size_t>(
          issuer_configured_batch_size)) {
    return nullptr;
  }

  const TRUST_TOKEN_METHOD* method = nullptr;
  switch (issuer_configured_version) {
    case mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb:
      method = TRUST_TOKEN_experiment_v2_pmb();
      break;
    case mojom::TrustTokenProtocolVersion::kTrustTokenV3Voprf:
      method = TRUST_TOKEN_experiment_v2_voprf();
      break;
    case mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Pmb:
      method = TRUST_TOKEN_pst_v1_pmb();
      break;
    case mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Voprf:
      method = TRUST_TOKEN_pst_v1_voprf();
      break;
  }

  auto ctx = bssl::UniquePtr<TRUST_TOKEN_CLIENT>(TRUST_TOKEN_CLIENT_new(
      /*method=*/method,
      /*max_batchsize=*/static_cast<size_t>(issuer_configured_batch_size)));
  if (!ctx) {
    return nullptr;
  }

  return base::WrapUnique(new BoringsslTrustTokenState(std::move(ctx)));
}

BoringsslTrustTokenState::~BoringsslTrustTokenState() = default;

BoringsslTrustTokenState::BoringsslTrustTokenState(
    bssl::UniquePtr<TRUST_TOKEN_CLIENT> ctx)
    : ctx_(std::move(ctx)) {}

TRUST_TOKEN_CLIENT* BoringsslTrustTokenState::Get() const {
  return ctx_.get();
}

}  // namespace network
