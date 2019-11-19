// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ATTESTATION_OBJECT_H_
#define DEVICE_FIDO_ATTESTATION_OBJECT_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"

namespace device {

class AttestationStatement;

// Object containing the authenticator-provided attestation every time
// a credential is created, per
// https://www.w3.org/TR/2017/WD-webauthn-20170505/#cred-attestation.
class COMPONENT_EXPORT(DEVICE_FIDO) AttestationObject {
 public:
  AttestationObject(AuthenticatorData data,
                    std::unique_ptr<AttestationStatement> statement);

  // Moveable.
  AttestationObject(AttestationObject&& other);
  AttestationObject& operator=(AttestationObject&& other);

  ~AttestationObject();

  std::vector<uint8_t> GetCredentialId() const;

  enum class AAGUID {
    kErase,
    kInclude,
  };

  // Replaces the attestation statement with a “none” attestation, and replaces
  // device AAGUID with zero bytes (unless |erase_aaguid| is kInclude) as
  // specified for step 20.3 in
  // https://w3c.github.io/webauthn/#createCredential.
  void EraseAttestationStatement(AAGUID erase_aaguid);

  // Returns true if the attestation is a "self" attestation, i.e. is just the
  // private key signing itself to show that it is fresh. See
  // https://www.w3.org/TR/webauthn/#self-attestation. Note that self-
  // attestation also requires at the AAGUID in the authenticator data be all
  // zeros.
  bool IsSelfAttestation();

  // Returns true if the attestation certificate is known to be inappropriately
  // identifying. Some tokens return unique attestation certificates even when
  // the bit to request that is not set. (Normal attestation certificates are
  // not indended to be trackable.)
  bool IsAttestationCertificateInappropriatelyIdentifying();

  const std::array<uint8_t, kRpIdHashLength>& rp_id_hash() const {
    return authenticator_data_.application_parameter();
  }

  const AuthenticatorData& authenticator_data() const {
    return authenticator_data_;
  }

  const AttestationStatement& attestation_statement() const {
    return *attestation_statement_.get();
  }

 private:
  AuthenticatorData authenticator_data_;
  std::unique_ptr<AttestationStatement> attestation_statement_;

  DISALLOW_COPY_AND_ASSIGN(AttestationObject);
};

// Produces a WebAuthN style CBOR-encoded byte-array
// in the following format, when written:
// {"authData": authenticator data bytes,
//  "fmt": attestation format name,
//  "attStmt": attestation statement bytes }
COMPONENT_EXPORT(DEVICE_FIDO)
cbor::Value AsCBOR(const AttestationObject&);

}  // namespace device

#endif  // DEVICE_FIDO_ATTESTATION_OBJECT_H_
