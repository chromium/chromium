// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_MAKE_CREDENTIAL_RESPONSE_H_
#define DEVICE_FIDO_AUTHENTICATOR_MAKE_CREDENTIAL_RESPONSE_H_

#include <stdint.h>

#include <array>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "device/fido/attestation_object.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

// Attestation object which includes attestation format, authentication
// data, and attestation statement returned by the authenticator as a response
// to MakeCredential request.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticatorMakeCredential
class COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorMakeCredentialResponse {
 public:
  static std::optional<AuthenticatorMakeCredentialResponse>
  CreateFromU2fRegisterResponse(
      std::optional<FidoTransportProtocol> transport_used,
      base::span<const uint8_t, kRpIdHashLength> relying_party_id_hash,
      base::span<const uint8_t> u2f_data);

  AuthenticatorMakeCredentialResponse(
      std::optional<FidoTransportProtocol> transport_used,
      AttestationObject attestation_object);
  AuthenticatorMakeCredentialResponse(
      AuthenticatorMakeCredentialResponse&& that);
  AuthenticatorMakeCredentialResponse& operator=(
      AuthenticatorMakeCredentialResponse&& other);

  AuthenticatorMakeCredentialResponse(
      const AuthenticatorMakeCredentialResponse&) = delete;
  AuthenticatorMakeCredentialResponse& operator=(
      const AuthenticatorMakeCredentialResponse&) = delete;

  ~AuthenticatorMakeCredentialResponse();

  std::vector<uint8_t> GetCBOREncodedAttestationObject() const;

  const std::array<uint8_t, kRpIdHashLength>& GetRpIdHash() const;

  AttestationObject attestation_object;

  // enterprise_attestation_returned is true if the authenticator indicated that
  // it returned an enterprise attestation. Note: U2F authenticators can
  // support enterprise/individual attestation but cannot indicate when they
  // have done so, so this will always be false in the U2F case.
  bool enterprise_attestation_returned = false;

  // is_resident_key indicates whether the created credential is client-side
  // discoverable. It is nullopt if no discoverable credential was requested,
  // but the authenticator may have created one anyway.
  std::optional<bool> is_resident_key;

  // attestation_should_be_filtered is true iff a filter indicated that the
  // attestation should not be returned. This is acted upon by
  // |AuthenticatorCommon| based on enterprise policy.
  bool attestation_should_be_filtered = false;

  // transports contains the full set of transports supported by the
  // authenticator, if known.
  std::optional<base::flat_set<FidoTransportProtocol>> transports;

  // Contains the transport used to register the credential in this case. It is
  // nullopt for cases where we cannot determine the transport (Windows).
  std::optional<FidoTransportProtocol> transport_used;

  // Whether the credential that was created has an associated large blob key or
  // supports the largeBlob extension. This can only be true if the credential
  // is created with the largeBlob or largeBlobKey extension on a capable
  // authenticator.
  std::optional<LargeBlobSupportType> large_blob_type;

  // Whether a PRF is configured for this credential. This only reflects the
  // output of the `prf` extension. Any output from the `hmac-secret` extension
  // is in the authenticator data. However, note that the WebAuthn-level prf
  // extension may be using the `hmac-secret` extension at the CTAP layer.
  bool prf_enabled = false;

  // Contains the output of the `prf` extension.
  std::optional<std::vector<uint8_t>> prf_results;
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
