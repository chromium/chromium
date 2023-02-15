// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/boringssl_attestation_cryptographer.h"

#include <string>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "services/network/trust_tokens/boringssl_trust_token_state.h"
#include "services/network/trust_tokens/scoped_boringssl_bytes.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/stack.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network {

BoringsslAttestationCryptographer::BoringsslAttestationCryptographer() =
    default;

BoringsslAttestationCryptographer::~BoringsslAttestationCryptographer() =
    default;

bool BoringsslAttestationCryptographer::Initialize(
    mojom::TrustTokenProtocolVersion issuer_configured_version) {
  state_ = BoringsslTrustTokenState::Create(
      issuer_configured_version,
      /*issuer_configured_batch_size=*/kIssuanceCount);
  return !!state_;
}

bool BoringsslAttestationCryptographer::AddKey(base::StringPiece key) {
  if (!state_) {
    return false;
  }

  size_t key_index;
  if (!TRUST_TOKEN_CLIENT_add_key(
          /*ctx=*/state_->Get(), /*out_key_index=*/&key_index,
          /*key=*/base::as_bytes(base::make_span(key)).data(),
          /*key_len=*/key.size())) {
    return false;
  }

  return true;
}

absl::optional<std::string> BoringsslAttestationCryptographer::BeginIssuance(
    base::StringPiece message) {
  if (!state_) {
    return absl::nullopt;
  }

  ScopedBoringsslBytes raw_issuance_request;
  if (!TRUST_TOKEN_CLIENT_begin_issuance_over_message(
          /*ctx=*/state_->Get(), /*out=*/raw_issuance_request.mutable_ptr(),
          /*out_len=*/raw_issuance_request.mutable_len(),
          /*count=*/kIssuanceCount,
          /*msg=*/reinterpret_cast<const unsigned char*>(message.data()),
          /*msg_len=*/message.length())) {
    return absl::nullopt;
  }

  return base::Base64Encode(raw_issuance_request.as_span());
}

absl::optional<std::string>
BoringsslAttestationCryptographer::ConfirmIssuanceAndBeginRedemption(
    base::StringPiece response_header) {
  if (!state_) {
    return absl::nullopt;
  }

  std::string decoded_response;
  if (!base::Base64Decode(response_header, &decoded_response)) {
    return absl::nullopt;
  }

  // The `out_key_index` returns the index of the key used among the
  // keys made available by the issuer; added in `AddKey`. This allows
  // to retrieve the key used on issuance to then confirm that it is
  // still available before beginning the redemption process.
  //
  // For attesation, we begin the redemption immediately after, removing
  // the need to know which key was used.
  size_t key_index;

  bssl::UniquePtr<STACK_OF(TRUST_TOKEN)> tokens(
      TRUST_TOKEN_CLIENT_finish_issuance(
          /*ctx=*/state_->Get(),
          /*out_key_index=*/&key_index,
          /*response=*/base::as_bytes(base::make_span(decoded_response)).data(),
          /*response_len=*/decoded_response.size()));

  if (!tokens) {
    return absl::nullopt;
  }

  ScopedBoringsslBytes raw_redemption_request;
  if (!TRUST_TOKEN_CLIENT_begin_redemption(
          /*ctx=*/state_->Get(), /*out=*/raw_redemption_request.mutable_ptr(),
          /*out_len=*/raw_redemption_request.mutable_len(),
          /*token=*/sk_TRUST_TOKEN_value(tokens.get(), 0),
          /*data=*/nullptr, /*data_len=*/0,
          /*time=*/(base::Time::Now() - base::Time::UnixEpoch()).InSeconds())) {
    return absl::nullopt;
  }

  return base::Base64Encode(raw_redemption_request.as_span());
}

}  // namespace network
