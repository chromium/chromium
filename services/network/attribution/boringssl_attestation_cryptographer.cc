// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/boringssl_attestation_cryptographer.h"

namespace network {

// TODO(crbug.com/1395818): An empty implementation has been added when
// `AttributionRequestHelper` was added as it instantiates an
// `BoringsslAttestationCryptographer`. The implementation should be added here.

BoringsslAttestationCryptographer::BoringsslAttestationCryptographer() =
    default;

BoringsslAttestationCryptographer::~BoringsslAttestationCryptographer() =
    default;

bool BoringsslAttestationCryptographer::Initialize(
    mojom::TrustTokenProtocolVersion issuer_configured_version) {
  return false;
}

bool BoringsslAttestationCryptographer::AddKey(base::StringPiece key) {
  return false;
}

absl::optional<std::string> BoringsslAttestationCryptographer::BeginIssuance(
    base::StringPiece message) {
  return absl::nullopt;
}

absl::optional<std::string>
BoringsslAttestationCryptographer::ConfirmIssuanceAndBeginRedemption(
    base::StringPiece response_header) {
  return absl::nullopt;
}

}  // namespace network
