// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/attested_credential_data.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_math.h"
#include "components/cbor/reader.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/opaque_public_key.h"
#include "device/fido/public_key.h"

namespace device {

// static
base::Optional<std::pair<AttestedCredentialData, base::span<const uint8_t>>>
AttestedCredentialData::ConsumeFromCtapResponse(
    base::span<const uint8_t> buffer) {
  if (buffer.size() < kAaguidLength)
    return base::nullopt;

  auto aaguid = buffer.first<kAaguidLength>();
  buffer = buffer.subspan(kAaguidLength);

  if (buffer.size() < kCredentialIdLengthLength)
    return base::nullopt;

  auto credential_id_length_span = buffer.first<kCredentialIdLengthLength>();
  const size_t credential_id_length =
      (base::strict_cast<size_t>(credential_id_length_span[0]) << 8) |
      base::strict_cast<size_t>(credential_id_length_span[1]);
  buffer = buffer.subspan(kCredentialIdLengthLength);

  if (buffer.size() < credential_id_length)
    return base::nullopt;

  auto credential_id = buffer.first(credential_id_length);
  buffer = buffer.subspan(credential_id_length);

  // The public key is a CBOR map and is thus variable length. Therefore the
  // CBOR parser needs to be invoked to find its length, even though the result
  // is discarded.
  size_t bytes_read;
  if (!cbor::Reader::Read(buffer, &bytes_read)) {
    return base::nullopt;
  }
  auto credential_public_key_data =
      std::make_unique<OpaquePublicKey>(buffer.first(bytes_read));
  buffer = buffer.subspan(bytes_read);

  return std::make_pair(
      AttestedCredentialData(aaguid, credential_id_length_span,
                             fido_parsing_utils::Materialize(credential_id),
                             std::move(credential_public_key_data)),
      buffer);
}

// static
base::Optional<AttestedCredentialData>
AttestedCredentialData::CreateFromU2fRegisterResponse(
    base::span<const uint8_t> u2f_data,
    std::unique_ptr<PublicKey> public_key) {
  // TODO(crbug/799075): Introduce a CredentialID class to do this extraction.
  // Extract the length of the credential (i.e. of the U2FResponse key
  // handle). Length is big endian.
  std::vector<uint8_t> extracted_length =
      fido_parsing_utils::Extract(u2f_data, kU2fKeyHandleLengthOffset, 1);

  if (extracted_length.empty()) {
    return base::nullopt;
  }

  // For U2F register request, device AAGUID is set to zeros.
  std::array<uint8_t, kAaguidLength> aaguid = {};

  // Note that U2F responses only use one byte for length.
  std::array<uint8_t, kCredentialIdLengthLength> credential_id_length = {
      0, extracted_length[0]};

  // Extract the credential id (i.e. key handle).
  std::vector<uint8_t> credential_id = fido_parsing_utils::Extract(
      u2f_data, kU2fKeyHandleOffset,
      base::strict_cast<size_t>(credential_id_length[1]));

  if (credential_id.empty()) {
    return base::nullopt;
  }

  return AttestedCredentialData(aaguid, credential_id_length,
                                std::move(credential_id),
                                std::move(public_key));
}

AttestedCredentialData::AttestedCredentialData(AttestedCredentialData&& other) =
    default;

AttestedCredentialData& AttestedCredentialData::operator=(
    AttestedCredentialData&& other) = default;

AttestedCredentialData::~AttestedCredentialData() = default;

bool AttestedCredentialData::IsAaguidZero() const {
  return std::all_of(aaguid_.begin(), aaguid_.end(),
                     [](uint8_t v) { return v == 0; });
}

void AttestedCredentialData::DeleteAaguid() {
  std::fill(aaguid_.begin(), aaguid_.end(), 0);
}

std::vector<uint8_t> AttestedCredentialData::SerializeAsBytes() const {
  std::vector<uint8_t> attestation_data;
  fido_parsing_utils::Append(&attestation_data, aaguid_);
  fido_parsing_utils::Append(&attestation_data, credential_id_length_);
  fido_parsing_utils::Append(&attestation_data, credential_id_);
  fido_parsing_utils::Append(&attestation_data, public_key_->EncodeAsCOSEKey());
  return attestation_data;
}

AttestedCredentialData::AttestedCredentialData(
    base::span<const uint8_t, kAaguidLength> aaguid,
    base::span<const uint8_t, kCredentialIdLengthLength> credential_id_length,
    std::vector<uint8_t> credential_id,
    std::unique_ptr<PublicKey> public_key)
    : aaguid_(fido_parsing_utils::Materialize(aaguid)),
      credential_id_length_(
          fido_parsing_utils::Materialize(credential_id_length)),
      credential_id_(std::move(credential_id)),
      public_key_(std::move(public_key)) {}

}  // namespace device
