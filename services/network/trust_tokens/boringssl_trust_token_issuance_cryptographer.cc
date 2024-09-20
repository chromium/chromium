// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/boringssl_trust_token_issuance_cryptographer.h"

#include <memory>
#include <optional>
#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "services/network/trust_tokens/boringssl_trust_token_state.h"
#include "services/network/trust_tokens/scoped_boringssl_bytes.h"
#include "third_party/boringssl/src/include/openssl/base.h"
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
  state_ = BoringsslTrustTokenState::Create(issuer_configured_version,
                                            issuer_configured_batch_size);
  return !!state_;
}

bool BoringsslTrustTokenIssuanceCryptographer::AddKey(std::string_view key) {
  if (!state_) {
    return false;
  }

  size_t key_index;
  if (!TRUST_TOKEN_CLIENT_add_key(state_->Get(), &key_index,
                                  base::as_bytes(base::make_span(key)).data(),
                                  key.size())) {
    return false;
  }

  DCHECK(!keys_by_index_.count(key_index));
  keys_by_index_[key_index] = std::string(key);
  return true;
}

std::optional<std::string>
BoringsslTrustTokenIssuanceCryptographer::BeginIssuance(size_t num_tokens) {
  if (!state_) {
    return std::nullopt;
  }

  ScopedBoringsslBytes raw_issuance_request;
  if (!TRUST_TOKEN_CLIENT_begin_issuance(
          state_->Get(),
          &raw_issuance_request.mutable_ptr()->AsEphemeralRawAddr(),
          raw_issuance_request.mutable_len(), num_tokens)) {
    return std::nullopt;
  }

  return base::Base64Encode(raw_issuance_request.as_span());
}

std::unique_ptr<UnblindedTokens>
BoringsslTrustTokenIssuanceCryptographer::ConfirmIssuance(
    std::string_view response_header) {
  if (!state_) {
    return nullptr;
  }

  std::string decoded_response;
  if (!base::Base64Decode(response_header, &decoded_response)) {
    return nullptr;
  }

  size_t key_index;
  bssl::UniquePtr<STACK_OF(TRUST_TOKEN)> tokens(
      TRUST_TOKEN_CLIENT_finish_issuance(
          state_->Get(), &key_index,
          base::as_bytes(base::make_span(decoded_response)).data(),
          decoded_response.size()));

  if (!tokens) {
    return nullptr;
  }

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
