// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/attestation_statement.h"

#include <string>
#include <utility>

#include "device/fido/fido_constants.h"

namespace device {

AttestationStatement::~AttestationStatement() = default;

AttestationStatement::AttestationStatement(std::string format)
    : format_(std::move(format)) {}

NoneAttestationStatement::NoneAttestationStatement()
    : AttestationStatement(kNoneAttestationValue) {}

NoneAttestationStatement::~NoneAttestationStatement() = default;

bool NoneAttestationStatement::
    IsAttestationCertificateInappropriatelyIdentifying() const {
  return false;
}

bool NoneAttestationStatement::IsNoneAttestation() const {
  return true;
}

bool NoneAttestationStatement::IsSelfAttestation() const {
  return false;
}

std::optional<base::span<const uint8_t>>
NoneAttestationStatement::GetLeafCertificate() const {
  return std::nullopt;
}

cbor::Value NoneAttestationStatement::AsCBOR() const {
  return cbor::Value(cbor::Value::MapValue());
}

cbor::Value AsCBOR(const AttestationStatement& as) {
  return as.AsCBOR();
}

}  // namespace device
