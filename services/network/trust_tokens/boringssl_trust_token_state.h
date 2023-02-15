// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_STATE_H_
#define SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_STATE_H_

#include <memory>
#include <string>

#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace network {

class BoringsslTrustTokenState {
 public:
  ~BoringsslTrustTokenState();

  static std::unique_ptr<BoringsslTrustTokenState> Create(
      mojom::TrustTokenProtocolVersion issuer_configured_version,
      int issuer_configured_batch_size);

  TRUST_TOKEN_CLIENT* Get() const;

 private:
  explicit BoringsslTrustTokenState(bssl::UniquePtr<TRUST_TOKEN_CLIENT>);

  // Maintains Trust Tokens protocol state.
  bssl::UniquePtr<TRUST_TOKEN_CLIENT> ctx_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_BORINGSSL_TRUST_TOKEN_STATE_H_
