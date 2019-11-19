// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_make_credential_response.h"

#include <utility>

#include "components/cbor/writer.h"
#include "device/fido/attestation_object.h"
#include "device/fido/attestation_statement_formats.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/ec_public_key.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

// static
base::Optional<AuthenticatorMakeCredentialResponse>
AuthenticatorMakeCredentialResponse::CreateFromU2fRegisterResponse(
    base::Optional<FidoTransportProtocol> transport_used,
    base::span<const uint8_t, kRpIdHashLength> relying_party_id_hash,
    base::span<const uint8_t> u2f_data) {
  auto public_key = ECPublicKey::ExtractFromU2fRegistrationResponse(
      fido_parsing_utils::kEs256, u2f_data);
  if (!public_key)
    return base::nullopt;

  auto attested_credential_data =
      AttestedCredentialData::CreateFromU2fRegisterResponse(
          u2f_data, std::move(public_key));

  if (!attested_credential_data)
    return base::nullopt;

  // Extract the credential_id for packing into the response data.
  std::vector<uint8_t> credential_id =
      attested_credential_data->credential_id();

  // The counter is zeroed out for Register requests.
  std::array<uint8_t, 4> counter = {};
  constexpr uint8_t flags =
      static_cast<uint8_t>(AuthenticatorData::Flag::kTestOfUserPresence) |
      static_cast<uint8_t>(AuthenticatorData::Flag::kAttestation);

  AuthenticatorData authenticator_data(relying_party_id_hash, flags, counter,
                                       std::move(attested_credential_data));

  auto fido_attestation_statement =
      FidoAttestationStatement::CreateFromU2fRegisterResponse(u2f_data);

  if (!fido_attestation_statement)
    return base::nullopt;

  return AuthenticatorMakeCredentialResponse(
      transport_used, AttestationObject(std::move(authenticator_data),
                                        std::move(fido_attestation_statement)));
}

AuthenticatorMakeCredentialResponse::AuthenticatorMakeCredentialResponse(
    base::Optional<FidoTransportProtocol> transport_used,
    AttestationObject attestation_object)
    : ResponseData(attestation_object.GetCredentialId()),
      attestation_object_(std::move(attestation_object)),
      transport_used_(transport_used) {}

AuthenticatorMakeCredentialResponse::AuthenticatorMakeCredentialResponse(
    AuthenticatorMakeCredentialResponse&& that) = default;

AuthenticatorMakeCredentialResponse& AuthenticatorMakeCredentialResponse::
operator=(AuthenticatorMakeCredentialResponse&& other) = default;

AuthenticatorMakeCredentialResponse::~AuthenticatorMakeCredentialResponse() =
    default;

std::vector<uint8_t>
AuthenticatorMakeCredentialResponse::GetCBOREncodedAttestationObject() const {
  return cbor::Writer::Write(AsCBOR(attestation_object_))
      .value_or(std::vector<uint8_t>());
}

void AuthenticatorMakeCredentialResponse::EraseAttestationStatement(
    AttestationObject::AAGUID erase_aaguid) {
  attestation_object_.EraseAttestationStatement(erase_aaguid);
}

bool AuthenticatorMakeCredentialResponse::IsSelfAttestation() {
  return attestation_object_.IsSelfAttestation();
}

bool AuthenticatorMakeCredentialResponse::
    IsAttestationCertificateInappropriatelyIdentifying() {
  return attestation_object_
      .IsAttestationCertificateInappropriatelyIdentifying();
}

const std::array<uint8_t, kRpIdHashLength>&
AuthenticatorMakeCredentialResponse::GetRpIdHash() const {
  return attestation_object_.rp_id_hash();
}

std::vector<uint8_t> AsCTAPStyleCBORBytes(
    const AuthenticatorMakeCredentialResponse& response) {
  const AttestationObject& object = response.attestation_object();
  cbor::Value::MapValue map;
  map.emplace(1, object.attestation_statement().format_name());
  map.emplace(2, object.authenticator_data().SerializeToByteArray());
  map.emplace(3, AsCBOR(object.attestation_statement()));
  auto encoded_bytes = cbor::Writer::Write(cbor::Value(std::move(map)));
  DCHECK(encoded_bytes);
  return std::move(*encoded_bytes);
}

}  // namespace device
