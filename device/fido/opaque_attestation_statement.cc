// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/opaque_attestation_statement.h"

#include <utility>

#include "components/cbor/values.h"

using cbor::Value;

namespace device {

OpaqueAttestationStatement::OpaqueAttestationStatement(
    std::string attestation_format,
    Value attestation_statement)
    : AttestationStatement(std::move(attestation_format)),
      attestation_statement_(std::move(attestation_statement)) {}

OpaqueAttestationStatement::~OpaqueAttestationStatement() = default;

// Returns the deep copied CBOR value of |attestation_statement_|.
Value OpaqueAttestationStatement::AsCBOR() const {
  return attestation_statement_.Clone();
}

bool OpaqueAttestationStatement::IsNoneAttestation() const {
  return format_ == "none" && attestation_statement_.is_map() &&
         attestation_statement_.GetMap().empty();
}

bool OpaqueAttestationStatement::IsSelfAttestation() const {
  if (!attestation_statement_.is_map()) {
    return false;
  }

  const Value::MapValue& m(attestation_statement_.GetMap());
  const Value alg("alg");
  const Value sig("sig");

  return format_ == "packed" && m.size() == 2 && m.count(std::move(alg)) == 1 &&
         m.count(std::move(sig)) == 1;
}

bool OpaqueAttestationStatement::
    IsAttestationCertificateInappropriatelyIdentifying() const {
  return false;
}

std::optional<base::span<const uint8_t>>
OpaqueAttestationStatement::GetLeafCertificate() const {
  if (!attestation_statement_.is_map()) {
    return std::nullopt;
  }

  const Value::MapValue& m(attestation_statement_.GetMap());
  const Value x5c("x5c");
  const auto it = m.find(x5c);
  if (it == m.end() || !it->second.is_array()) {
    return std::nullopt;
  }

  const Value::ArrayValue& certs = it->second.GetArray();
  if (certs.empty() || !certs[0].is_bytestring()) {
    return std::nullopt;
  }

  return certs[0].GetBytestring();
}

}  // namespace device
