// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/boringssl_trust_token_redemption_cryptographer.h"

#include "base/base64.h"
#include "base/callback_helpers.h"
#include "net/http/structured_headers.h"
#include "services/network/trust_tokens/scoped_boringssl_bytes.h"
#include "services/network/trust_tokens/trust_token_client_data_canonicalization.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network {

BoringsslTrustTokenRedemptionCryptographer::
    BoringsslTrustTokenRedemptionCryptographer() = default;

BoringsslTrustTokenRedemptionCryptographer::
    ~BoringsslTrustTokenRedemptionCryptographer() = default;

bool BoringsslTrustTokenRedemptionCryptographer::Initialize(
    mojom::TrustTokenProtocolVersion issuer_configured_version,
    int issuer_configured_batch_size) {
  if (!base::IsValueInRangeForNumericType<size_t>(issuer_configured_batch_size))
    return false;

  const TRUST_TOKEN_METHOD* method = nullptr;
  switch (issuer_configured_version) {
    case mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb:
      method = TRUST_TOKEN_experiment_v2_pmb();
      break;
    case mojom::TrustTokenProtocolVersion::kTrustTokenV3Voprf:
      method = TRUST_TOKEN_experiment_v2_voprf();
      break;
  }

  ctx_ = bssl::UniquePtr<TRUST_TOKEN_CLIENT>(TRUST_TOKEN_CLIENT_new(
      method, static_cast<size_t>(issuer_configured_batch_size)));
  if (!ctx_)
    return false;

  return true;
}

absl::optional<std::string>
BoringsslTrustTokenRedemptionCryptographer::BeginRedemption(
    TrustToken token,
    const url::Origin& top_level_origin) {
  if (!ctx_)
    return absl::nullopt;

  // It's unclear if BoringSSL expects the exact same value in the client data
  // and as the |time| argument to TRUST_TOKEN_CLIENT_begin_redemption; play
  // it safe by using a single timestamp.
  base::Time redemption_timestamp = base::Time::Now();

  absl::optional<std::vector<uint8_t>> maybe_client_data =
      CanonicalizeTrustTokenClientDataForRedemption(redemption_timestamp,
                                                    top_level_origin);
  if (!maybe_client_data)
    return absl::nullopt;

  ScopedBoringsslBytes raw_redemption_request;

  bssl::UniquePtr<TRUST_TOKEN> boringssl_token(
      TRUST_TOKEN_new(base::as_bytes(base::make_span(token.body())).data(),
                      token.body().size()));
  if (!boringssl_token)
    return absl::nullopt;

  if (!TRUST_TOKEN_CLIENT_begin_redemption(
          ctx_.get(), raw_redemption_request.mutable_ptr(),
          raw_redemption_request.mutable_len(), boringssl_token.get(),
          maybe_client_data->data(), maybe_client_data->size(),
          (redemption_timestamp - base::Time::UnixEpoch()).InSeconds())) {
    return absl::nullopt;
  }

  return base::Base64Encode(raw_redemption_request.as_span());
}

absl::optional<std::string>
BoringsslTrustTokenRedemptionCryptographer::ConfirmRedemption(
    base::StringPiece response_header) {
  if (!ctx_)
    return absl::nullopt;

  std::string decoded_response;
  if (!base::Base64Decode(response_header, &decoded_response))
    return absl::nullopt;

  // In TrustTokenV3, the entire RR is stored in the body field, and the
  // signature field is unused in finish_redemption.
  ScopedBoringsslBytes rr;
  ScopedBoringsslBytes unused;
  if (!TRUST_TOKEN_CLIENT_finish_redemption(
          ctx_.get(), rr.mutable_ptr(), rr.mutable_len(), unused.mutable_ptr(),
          unused.mutable_len(),
          base::as_bytes(base::make_span(decoded_response)).data(),
          decoded_response.size())) {
    return absl::nullopt;
  }

  if (!rr.is_valid())
    return "";
  return std::string(reinterpret_cast<const char*>(rr.as_span().data()),
                     rr.as_span().size());
}

}  // namespace network
