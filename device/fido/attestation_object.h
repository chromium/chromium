// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ATTESTATION_OBJECT_H_
#define DEVICE_FIDO_ATTESTATION_OBJECT_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"

namespace device {

class AttestationStatement;

// Object containing the authenticator-provided attestation every time
// a credential is created, per
// https://www.w3.org/TR/2017/WD-webauthn-20170505/#cred-attestation.
class COMPONENT_EXPORT(DEVICE_FIDO) AttestationObject {
 public:
  // ResponseFields contains the parts of an attestation object that are needed
  // when constructing a browser's response to a create() call.
  struct COMPONENT_EXPORT(DEVICE_FIDO) ResponseFields {
    ResponseFields();
    ~ResponseFields();
    ResponseFields(ResponseFields&&);
    ResponseFields(const ResponseFields&) = delete;
    ResponseFields& operator=(const ResponseFields&) = delete;

    std::vector<uint8_t> attestation_object_bytes;
    std::vector<uint8_t> authenticator_data;
    std::optional<std::vector<uint8_t>> public_key_der;
    int32_t public_key_algo;
  };

  static std::optional<AttestationObject> Parse(const cbor::Value& value);

  // ParseForResponseFields parses a serialized attestation object and extracts
  // the fields needed to build a browser's response to a create() call. If
  // `attestation_acceptable` is false any attestation will have been removed
  // in the return value.
  static std::optional<ResponseFields> ParseForResponseFields(
      std::vector<uint8_t> attestation_object_bytes,
      bool attestation_acceptable);

  // TODO: this could just take a `cbor::Value` and all the
  // `AttestationStatement` code could be turned into a parse-on-demand
  // pattern.
  AttestationObject(AuthenticatorData data,
                    std::unique_ptr<AttestationStatement> statement);
  AttestationObject(AttestationObject&& other);
  AttestationObject& operator=(AttestationObject&& other);

  AttestationObject(const AttestationObject&) = delete;
  AttestationObject& operator=(const AttestationObject&) = delete;

  ~AttestationObject();

  std::vector<uint8_t> GetCredentialId() const;

  enum class AAGUID {
    kErase,
    kInclude,
  };

  // Replaces the attestation statement with a “none” attestation, and replaces
  // device AAGUID with zero bytes (unless |erase_aaguid| is kInclude) as
  // specified for step 20.3 in
  // https://w3c.github.io/webauthn/#createCredential. Returns true if any
  // modifications needed to be made and false otherwise.
  bool EraseAttestationStatement(AAGUID erase_aaguid);

  // EraseExtension deletes the named extension. It returns true iff the
  // extension was present.
  bool EraseExtension(std::string_view name);

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
