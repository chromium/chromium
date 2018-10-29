// Copyright 2017 The Chromium Authors. All rights reserved.
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
    IsAttestationCertificateInappropriatelyIdentifying() {
  return false;
}

bool NoneAttestationStatement::IsSelfAttestation() {
  return false;
}

base::Optional<base::span<const uint8_t>>
NoneAttestationStatement::GetLeafCertificate() const {
  return base::nullopt;
}

cbor::Value::MapValue NoneAttestationStatement::GetAsCBORMap() const {
  return cbor::Value::MapValue();
}

}  // namespace device
