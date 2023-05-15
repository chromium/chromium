// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_BORINGSSL_VERIFICATION_CRYPTOGRAPHER_H_
#define SERVICES_NETWORK_ATTRIBUTION_BORINGSSL_VERIFICATION_CRYPTOGRAPHER_H_

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "services/network/attribution/attribution_verification_mediator.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {

class BoringsslTrustTokenState;

class BoringsslVerificationCryptographer
    : public AttributionVerificationMediator::Cryptographer {
 public:
  BoringsslVerificationCryptographer();
  ~BoringsslVerificationCryptographer() override;

  bool Initialize(
      mojom::TrustTokenProtocolVersion issuer_configured_version) override;
  bool AddKey(base::StringPiece key) override;
  absl::optional<std::string> BeginIssuance(base::StringPiece message) override;
  absl::optional<std::string> ConfirmIssuanceAndBeginRedemption(
      base::StringPiece response_header) override;

 private:
  // In the context of report verification, we always issue a single token as it
  // is issued and redeemed immediately for a specific use case. This is
  // compared to private state tokens which are issued in large quantity and
  // redeemed individually at a different point in time for general use cases.
  static constexpr auto kIssuanceCount = 1;

  std::unique_ptr<BoringsslTrustTokenState> state_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_BORINGSSL_VERIFICATION_CRYPTOGRAPHER_H_
