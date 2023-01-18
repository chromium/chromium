// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_BORINGSSL_ATTESTATION_CRYPTOGRAPHER_H_
#define SERVICES_NETWORK_ATTRIBUTION_BORINGSSL_ATTESTATION_CRYPTOGRAPHER_H_

#include "services/network/attribution/attribution_attestation_mediator.h"

namespace network {

class BoringsslAttestationCryptographer
    : public AttributionAttestationMediator::Cryptographer {
 public:
  BoringsslAttestationCryptographer();
  ~BoringsslAttestationCryptographer() override;

  // AttributionAttestationMediator::Cryptographer implementation:
  bool Initialize(
      mojom::TrustTokenProtocolVersion issuer_configured_version) override;
  bool AddKey(base::StringPiece key) override;
  absl::optional<std::string> BeginIssuance(base::StringPiece message) override;
  absl::optional<std::string> ConfirmIssuanceAndBeginRedemption(
      base::StringPiece response_header) override;
};

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_BORINGSSL_ATTESTATION_CRYPTOGRAPHER_H_
