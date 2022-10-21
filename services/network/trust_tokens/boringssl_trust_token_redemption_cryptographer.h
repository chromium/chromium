// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_REDEMPTION_CRYPTOGRAPHER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_REDEMPTION_CRYPTOGRAPHER_H_

#include "services/network/trust_tokens/trust_token_request_redemption_helper.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace network {

// Executes one instance of a Trust Tokens redemption operation by calling the
// appropriate BoringSSL methods.
class BoringsslTrustTokenRedemptionCryptographer
    : public TrustTokenRequestRedemptionHelper::Cryptographer {
 public:
  BoringsslTrustTokenRedemptionCryptographer();
  ~BoringsslTrustTokenRedemptionCryptographer() override;

  // TrustTokenRequestRedemptionHelper::Cryptographer implementation:
  bool Initialize(mojom::TrustTokenProtocolVersion issuer_configured_version,
                  int issuer_configured_batch_size) override;
  absl::optional<std::string> BeginRedemption(
      TrustToken token,
      const url::Origin& top_level_origin) override;
  absl::optional<std::string> ConfirmRedemption(
      base::StringPiece response_header) override;

 private:
  // Maintains Trust Tokens protocol state.
  bssl::UniquePtr<TRUST_TOKEN_CLIENT> ctx_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_REDEMPTION_CRYPTOGRAPHER_H_
