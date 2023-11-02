// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/boringssl_trust_token_issuance_cryptographer.h"

#include "base/base64.h"
#include "base/numerics/safe_conversions.h"
#include "services/network/trust_tokens/scoped_boringssl_bytes.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/stack.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network {

namespace {

using UnblindedTokens =
    TrustTokenRequestIssuanceHelper::Cryptographer::UnblindedTokens;

}  // namespace

BoringsslTrustTokenIssuanceCryptographer::
    BoringsslTrustTokenIssuanceCryptographer() = default;

BoringsslTrustTokenIssuanceCryptographer::
    ~BoringsslTrustTokenIssuanceCryptographer() = default;

bool BoringsslTrustTokenIssuanceCryptographer::Initialize(
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
  return !!ctx_;
}

bool BoringsslTrustTokenIssuanceCryptographer::AddKey(base::StringPiece key) {
  if (!ctx_)
    return false;

  size_t key_index;
  if (!TRUST_TOKEN_CLIENT_add_key(ctx_.get(), &key_index,
                                  base::as_bytes(base::make_span(key)).data(),
                                  key.size())) {
    return false;
  }

  DCHECK(!keys_by_index_.count(key_index));
  keys_by_index_[key_index] = std::string(key);
  return true;
}

absl::optional<std::string>
BoringsslTrustTokenIssuanceCryptographer::BeginIssuance(size_t num_tokens) {
  if (!ctx_)
    return absl::nullopt;

  ScopedBoringsslBytes raw_issuance_request;
  if (!TRUST_TOKEN_CLIENT_begin_issuance(
          ctx_.get(), raw_issuance_request.mutable_ptr(),
          raw_issuance_request.mutable_len(), num_tokens)) {
    return absl::nullopt;
  }

  return base::Base64Encode(raw_issuance_request.as_span());
}

std::unique_ptr<UnblindedTokens>
BoringsslTrustTokenIssuanceCryptographer::ConfirmIssuance(
    base::StringPiece response_header) {
  if (!ctx_)
    return nullptr;

  std::string decoded_response;
  if (!base::Base64Decode(response_header, &decoded_response))
    return nullptr;

  size_t key_index;
  bssl::UniquePtr<STACK_OF(TRUST_TOKEN)> tokens(
      TRUST_TOKEN_CLIENT_finish_issuance(
          ctx_.get(), &key_index,
          base::as_bytes(base::make_span(decoded_response)).data(),
          decoded_response.size()));

  if (!tokens)
    return nullptr;

  auto ret = std::make_unique<UnblindedTokens>();

  DCHECK(keys_by_index_.count(key_index));
  ret->body_of_verifying_key = keys_by_index_[key_index];

  for (size_t i = 0; i < sk_TRUST_TOKEN_num(tokens.get()); ++i) {
    TRUST_TOKEN* token = sk_TRUST_TOKEN_value(tokens.get(), i);
    // Copy the token's contents.
    ret->tokens.emplace_back(reinterpret_cast<const char*>(token->data),
                             token->len);
  }

  return ret;
}

}  // namespace network
