// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ATTESTATION_STATEMENT_H_
#define DEVICE_FIDO_ATTESTATION_STATEMENT_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/cbor/values.h"

namespace device {

// A signed data object containing statements about a credential itself and
// the authenticator that created it.
// Each attestation statement format is defined by the following attributes:
// - The attestation statement format identifier.
// - The set of attestation types supported by the format.
// - The syntax of an attestation statement produced in this format.
// https://www.w3.org/TR/2017/WD-webauthn-20170505/#cred-attestation.
class COMPONENT_EXPORT(DEVICE_FIDO) AttestationStatement {
 public:
  virtual ~AttestationStatement();

  // The CBOR map data to be added to the attestation object, structured
  // in a way that is specified by its particular attestation format:
  // https://www.w3.org/TR/2017/WD-webauthn-20170505/#defined-attestation-formats
  // This is not a CBOR-encoded byte array, but the map that will be
  // nested within another CBOR object and encoded then.
  virtual cbor::Value AsCBOR() const = 0;

  // Returns true if the attestation is a "self" attestation, i.e. is just the
  // private key signing itself to show that it is fresh.
  virtual bool IsSelfAttestation() = 0;

  // Returns true if the attestation is known to be inappropriately identifying.
  // Some tokens return unique attestation certificates even when the bit to
  // request that is not set. (Normal attestation certificates are not
  // indended to be trackable.)
  virtual bool IsAttestationCertificateInappropriatelyIdentifying() = 0;

  // Return the DER bytes of the leaf X.509 certificate, if any.
  virtual base::Optional<base::span<const uint8_t>> GetLeafCertificate()
      const = 0;

  const std::string& format_name() const { return format_; }

 protected:
  explicit AttestationStatement(std::string format);
  const std::string format_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AttestationStatement);
};

// NoneAttestationStatement represents a “none” attestation, which is used when
// attestation information will not be returned. See
// https://w3c.github.io/webauthn/#none-attestation
class COMPONENT_EXPORT(DEVICE_FIDO) NoneAttestationStatement
    : public AttestationStatement {
 public:
  NoneAttestationStatement();
  ~NoneAttestationStatement() override;

  cbor::Value AsCBOR() const override;
  bool IsSelfAttestation() override;
  bool IsAttestationCertificateInappropriatelyIdentifying() override;
  base::Optional<base::span<const uint8_t>> GetLeafCertificate() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NoneAttestationStatement);
};

COMPONENT_EXPORT(DEVICE_FIDO)
cbor::Value AsCBOR(const AttestationStatement&);

}  // namespace device

#endif  // DEVICE_FIDO_ATTESTATION_STATEMENT_H_
