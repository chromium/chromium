// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_OPAQUE_ATTESTATION_STATEMENT_H_
#define DEVICE_FIDO_OPAQUE_ATTESTATION_STATEMENT_H_

#include <string>

#include "base/component_export.h"
#include "components/cbor/values.h"
#include "device/fido/attestation_statement.h"

namespace device {

// https://www.w3.org/TR/2017/WD-webauthn-20170505/#fido-u2f-attestation
class COMPONENT_EXPORT(DEVICE_FIDO) OpaqueAttestationStatement
    : public AttestationStatement {
 public:
  OpaqueAttestationStatement(std::string attestation_format,
                             cbor::Value attestation_statement);

  OpaqueAttestationStatement(const OpaqueAttestationStatement&) = delete;
  OpaqueAttestationStatement& operator=(const OpaqueAttestationStatement&) =
      delete;

  ~OpaqueAttestationStatement() override;

  // AttestationStatement:
  cbor::Value AsCBOR() const override;
  bool IsNoneAttestation() const override;
  bool IsSelfAttestation() const override;
  bool IsAttestationCertificateInappropriatelyIdentifying() const override;
  std::optional<base::span<const uint8_t>> GetLeafCertificate() const override;

 private:
  const cbor::Value attestation_statement_;
};

}  // namespace device

#endif  // DEVICE_FIDO_OPAQUE_ATTESTATION_STATEMENT_H_
