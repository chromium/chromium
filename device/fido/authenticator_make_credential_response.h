// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_MAKE_CREDENTIAL_RESPONSE_H_
#define DEVICE_FIDO_AUTHENTICATOR_MAKE_CREDENTIAL_RESPONSE_H_

#include <stdint.h>

#include <array>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/attestation_object.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/response_data.h"

namespace device {

// Attestation object which includes attestation format, authentication
// data, and attestation statement returned by the authenticator as a response
// to MakeCredential request.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticatorMakeCredential
class COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorMakeCredentialResponse
    : public ResponseData {
 public:
  static base::Optional<AuthenticatorMakeCredentialResponse>
  CreateFromU2fRegisterResponse(
      base::Optional<FidoTransportProtocol> transport_used,
      base::span<const uint8_t, kRpIdHashLength> relying_party_id_hash,
      base::span<const uint8_t> u2f_data);

  AuthenticatorMakeCredentialResponse(
      base::Optional<FidoTransportProtocol> transport_used,
      AttestationObject attestation_object);
  AuthenticatorMakeCredentialResponse(
      AuthenticatorMakeCredentialResponse&& that);
  AuthenticatorMakeCredentialResponse& operator=(
      AuthenticatorMakeCredentialResponse&& other);
  ~AuthenticatorMakeCredentialResponse() override;

  std::vector<uint8_t> GetCBOREncodedAttestationObject() const;

  // Replaces the attestation statement with a “none” attestation, and removes
  // AAGUID from authenticator data section unless |preserve_aaguid| is true.
  // https://w3c.github.io/webauthn/#createCredential
  void EraseAttestationStatement(AttestationObject::AAGUID erase_aaguid);

  // Returns true if the attestation is a "self" attestation, i.e. is just the
  // private key signing itself to show that it is fresh and the AAGUID is zero.
  bool IsSelfAttestation();

  // Returns true if the attestation certificate is known to be inappropriately
  // identifying. Some tokens return unique attestation certificates even when
  // the bit to request that is not set. (Normal attestation certificates are
  // not intended to be trackable.)
  bool IsAttestationCertificateInappropriatelyIdentifying();

  // ResponseData:
  const std::array<uint8_t, kRpIdHashLength>& GetRpIdHash() const override;

  const AttestationObject& attestation_object() const {
    return attestation_object_;
  }

  base::Optional<FidoTransportProtocol> transport_used() const {
    return transport_used_;
  }

 private:
  AttestationObject attestation_object_;

  // Contains the transport used to register the credential in this case. It is
  // nullopt for cases where we cannot determine the transport (Windows).
  base::Optional<FidoTransportProtocol> transport_used_;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorMakeCredentialResponse);
};

// Through cbor::Writer, produces a CTAP style CBOR-encoded byte array
// that conforms to the format CTAP2 devices sends to the client as a response.
// {01: attestation format name,
//  02: authenticator data bytes,
//  03: attestation statement bytes }
COMPONENT_EXPORT(DEVICE_FIDO)
std::vector<uint8_t> AsCTAPStyleCBORBytes(
    const AuthenticatorMakeCredentialResponse& response);

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_MAKE_CREDENTIAL_RESPONSE_H_
