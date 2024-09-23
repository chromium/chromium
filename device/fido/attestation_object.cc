// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/attestation_object.h"

#include <utility>

#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/opaque_attestation_statement.h"
#include "device/fido/public_key.h"

namespace device {

AttestationObject::ResponseFields::ResponseFields() = default;
AttestationObject::ResponseFields::~ResponseFields() = default;
AttestationObject::ResponseFields::ResponseFields(ResponseFields&&) = default;

// static
std::optional<AttestationObject> AttestationObject::Parse(
    const cbor::Value& value) {
  if (!value.is_map()) {
    return std::nullopt;
  }
  const cbor::Value::MapValue& map = value.GetMap();

  const auto& format_it = map.find(cbor::Value(kFormatKey));
  if (format_it == map.end() || !format_it->second.is_string()) {
    return std::nullopt;
  }
  const std::string& fmt = format_it->second.GetString();

  const auto& att_stmt_it = map.find(cbor::Value(kAttestationStatementKey));
  if (att_stmt_it == map.end()) {
    return std::nullopt;
  }
  std::unique_ptr<AttestationStatement> attestation_statement =
      std::make_unique<OpaqueAttestationStatement>(fmt,
                                                   att_stmt_it->second.Clone());

  const auto& auth_data_it = map.find(cbor::Value(kAuthDataKey));
  if (auth_data_it == map.end() || !auth_data_it->second.is_bytestring()) {
    return std::nullopt;
  }
  std::optional<AuthenticatorData> authenticator_data =
      AuthenticatorData::DecodeAuthenticatorData(
          auth_data_it->second.GetBytestring());
  if (!authenticator_data) {
    return std::nullopt;
  }
  return AttestationObject(std::move(*authenticator_data),
                           std::move(attestation_statement));
}

// static
std::optional<AttestationObject::ResponseFields>
AttestationObject::ParseForResponseFields(
    std::vector<uint8_t> attestation_object_bytes,
    bool attestation_acceptable) {
  std::optional<cbor::Value> attestation_object_map =
      cbor::Reader::Read(attestation_object_bytes);
  if (!attestation_object_map || !attestation_object_map->is_map()) {
    return std::nullopt;
  }

  std::optional<device::AttestationObject> attestation_object =
      device::AttestationObject::Parse(*attestation_object_map);
  if (!attestation_object) {
    return std::nullopt;
  }

  const std::optional<device::AttestedCredentialData>& att_cred_data(
      attestation_object->authenticator_data().attested_data());
  if (!att_cred_data) {
    return std::nullopt;
  }

  const device::PublicKey* pub_key = att_cred_data->public_key();
  ResponseFields ret;
  if (pub_key->der_bytes) {
    ret.public_key_der = pub_key->der_bytes;
  }
  ret.public_key_algo = pub_key->algorithm;

  if (attestation_acceptable) {
    ret.attestation_object_bytes = std::move(attestation_object_bytes);
  } else {
    const bool did_modify = attestation_object->EraseAttestationStatement(
        device::AttestationObject::AAGUID::kInclude);
    if (did_modify) {
      ret.attestation_object_bytes =
          *cbor::Writer::Write(AsCBOR(*attestation_object));
    } else {
      ret.attestation_object_bytes = std::move(attestation_object_bytes);
    }
  }

  ret.authenticator_data =
      attestation_object->authenticator_data().SerializeToByteArray();

  return ret;
}

AttestationObject::AttestationObject(
    AuthenticatorData data,
    std::unique_ptr<AttestationStatement> statement)
    : authenticator_data_(std::move(data)),
      attestation_statement_(std::move(statement)) {}

AttestationObject::AttestationObject(AttestationObject&& other) = default;
AttestationObject& AttestationObject::operator=(AttestationObject&& other) =
    default;

AttestationObject::~AttestationObject() = default;

std::vector<uint8_t> AttestationObject::GetCredentialId() const {
  return authenticator_data_.GetCredentialId();
}

bool AttestationObject::EraseAttestationStatement(
    AttestationObject::AAGUID erase_aaguid) {
  bool did_make_change = false;
  if (!attestation_statement_->IsNoneAttestation()) {
    attestation_statement_ = std::make_unique<NoneAttestationStatement>();
    did_make_change = true;
  }

  if (erase_aaguid == AAGUID::kErase) {
    did_make_change |= authenticator_data_.DeleteDeviceAaguid();
  }

// Attested credential data is optional section within authenticator data. But
// if present, the first 16 bytes of it represents a device AAGUID which must
// be set to zeros for none attestation statement format, unless explicitly
// requested otherwise (we make an exception for platform authenticators).
#if DCHECK_IS_ON()
  if (authenticator_data_.attested_data()) {
    DCHECK(erase_aaguid == AAGUID::kInclude ||
           authenticator_data_.attested_data()->IsAaguidZero());
  }
#endif

  return did_make_change;
}

bool AttestationObject::EraseExtension(std::string_view name) {
  return authenticator_data_.EraseExtension(name);
}

bool AttestationObject::IsSelfAttestation() {
  if (!attestation_statement_->IsSelfAttestation()) {
    return false;
  }
  // Self-attestation also requires that the AAGUID be zero. See
  // https://www.w3.org/TR/webauthn/#createCredential.
  return !authenticator_data_.attested_data() ||
         authenticator_data_.attested_data()->IsAaguidZero();
}

bool AttestationObject::IsAttestationCertificateInappropriatelyIdentifying() {
  return attestation_statement_
      ->IsAttestationCertificateInappropriatelyIdentifying();
}

cbor::Value AsCBOR(const AttestationObject& object) {
  cbor::Value::MapValue map;
  map[cbor::Value(kFormatKey)] =
      cbor::Value(object.attestation_statement().format_name());
  map[cbor::Value(kAuthDataKey)] =
      cbor::Value(object.authenticator_data().SerializeToByteArray());
  map[cbor::Value(kAttestationStatementKey)] =
      AsCBOR(object.attestation_statement());
  return cbor::Value(std::move(map));
}

}  // namespace device
