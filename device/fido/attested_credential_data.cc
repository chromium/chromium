// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/attested_credential_data.h"

#include <utility>

#include "base/memory/raw_ptr_exclusion.h"
#include "base/numerics/safe_math.h"
#include "base/ranges/algorithm.h"
#include "components/cbor/reader.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cbor_extract.h"
#include "device/fido/ed25519_public_key.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/p256_public_key.h"
#include "device/fido/public_key.h"
#include "device/fido/rsa_public_key.h"

using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;

namespace device {

// static
std::optional<std::pair<AttestedCredentialData, base::span<const uint8_t>>>
AttestedCredentialData::ConsumeFromCtapResponse(
    base::span<const uint8_t> buffer) {
  if (buffer.size() < kAaguidLength) {
    return std::nullopt;
  }

  auto aaguid = buffer.first<kAaguidLength>();
  buffer = buffer.subspan(kAaguidLength);

  if (buffer.size() < kCredentialIdLengthLength) {
    return std::nullopt;
  }

  auto credential_id_length_span = buffer.first<kCredentialIdLengthLength>();
  const size_t credential_id_length =
      (base::strict_cast<size_t>(credential_id_length_span[0]) << 8) |
      base::strict_cast<size_t>(credential_id_length_span[1]);
  buffer = buffer.subspan(kCredentialIdLengthLength);

  if (buffer.size() < credential_id_length) {
    return std::nullopt;
  }

  auto credential_id = buffer.first(credential_id_length);
  buffer = buffer.subspan(credential_id_length);

  size_t public_key_byte_len;
  std::optional<cbor::Value> public_key_cbor =
      cbor::Reader::Read(buffer, &public_key_byte_len);
  if (!public_key_cbor || !public_key_cbor->is_map()) {
    FIDO_LOG(ERROR) << "CBOR error in COSE public key";
    return std::nullopt;
  }

  const base::span<const uint8_t> public_key_cbor_bytes(
      buffer.first(public_key_byte_len));
  buffer = buffer.subspan(public_key_byte_len);

  const cbor::Value::MapValue& public_key_map = public_key_cbor->GetMap();

  struct COSEKey {
    // Both fields below are not a raw_ptr<int64_t>, because ELEMENT() treats
    // the raw_ptr<T> as a void*, skipping AddRef() call and causing a
    // ref-counting mismatch.
    RAW_PTR_EXCLUSION const int64_t* alg;
    RAW_PTR_EXCLUSION const int64_t* kty;
  } cose_key;

  static constexpr cbor_extract::StepOrByte<COSEKey> kSteps[] = {
      // clang-format off

      // kAlg is required to be present. See
      // https://www.w3.org/TR/webauthn/#credentialpublickey
      // COSE allows it to be a string or an integer. However, WebAuthn defines
      // COSEAlgorithmIdentifier to be a long[1], thus only integer-based
      // algorithms can be negotiated.
      //
      // [1] https://www.w3.org/TR/webauthn/#alg-identifier
      ELEMENT(Is::kRequired, COSEKey, alg),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kAlg)),

      // kKty is required in COSE keys:
      // https://tools.ietf.org/html/rfc8152#section-7
      ELEMENT(Is::kRequired, COSEKey, kty),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kKty)),

      Stop<COSEKey>(),
      // clang-format on
  };

  if (!cbor_extract::Extract<COSEKey>(&cose_key, kSteps, public_key_map)) {
    FIDO_LOG(ERROR) << "Failed to parse COSE key";
    return std::nullopt;
  }

  // In WebIDL, a |long| is an |int32_t|[1].
  //
  // [1] https://heycam.github.io/webidl/#idl-long
  const int64_t algorithm64 = *cose_key.alg;
  if (algorithm64 > std::numeric_limits<int32_t>::max() ||
      algorithm64 < std::numeric_limits<int32_t>::min()) {
    FIDO_LOG(ERROR) << "COSE algorithm in public key is out of range";
    return std::nullopt;
  }
  const int32_t algorithm = static_cast<int32_t>(algorithm64);
  const int64_t key_type = *cose_key.kty;

  std::unique_ptr<PublicKey> public_key;

  if (key_type == static_cast<int64_t>(CoseKeyTypes::kEC2) ||
      key_type == static_cast<int64_t>(CoseKeyTypes::kOKP)) {
    auto curve = public_key_map.find(
        cbor::Value(static_cast<int64_t>(CoseKeyKey::kEllipticCurve)));
    if (curve == public_key_map.end() || !curve->second.is_integer()) {
      return std::nullopt;
    }
    const int64_t curve_id = curve->second.GetInteger();

    if (key_type == static_cast<int64_t>(CoseKeyTypes::kEC2) &&
        curve_id == static_cast<int64_t>(CoseCurves::kP256)) {
      auto p256_key = P256PublicKey::ExtractFromCOSEKey(
          algorithm, public_key_cbor_bytes, public_key_map);
      if (!p256_key) {
        FIDO_LOG(ERROR) << "Invalid P-256 public key";
        return std::nullopt;
      }
      public_key = std::move(p256_key);
    } else if (key_type == static_cast<int64_t>(CoseKeyTypes::kOKP) &&
               curve_id == static_cast<int64_t>(CoseCurves::kEd25519)) {
      auto ed25519_key = Ed25519PublicKey::ExtractFromCOSEKey(
          algorithm, public_key_cbor_bytes, public_key_map);
      if (!ed25519_key) {
        FIDO_LOG(ERROR) << "Invalid Ed25519 public key";
        return std::nullopt;
      }
      public_key = std::move(ed25519_key);
    }
  } else if (key_type == static_cast<int64_t>(CoseKeyTypes::kRSA)) {
    auto rsa_key = RSAPublicKey::ExtractFromCOSEKey(
        algorithm, public_key_cbor_bytes, public_key_map);
    if (!rsa_key) {
      FIDO_LOG(ERROR) << "Invalid RSA public key";
      return std::nullopt;
    }
    public_key = std::move(rsa_key);
  }

  if (!public_key) {
    public_key = std::make_unique<PublicKey>(algorithm, public_key_cbor_bytes,
                                             std::nullopt);
  }

  return std::make_pair(
      AttestedCredentialData(aaguid, credential_id_length_span,
                             fido_parsing_utils::Materialize(credential_id),
                             std::move(public_key)),
      buffer);
}

// static
std::optional<AttestedCredentialData>
AttestedCredentialData::CreateFromU2fRegisterResponse(
    base::span<const uint8_t> u2f_data,
    std::unique_ptr<PublicKey> public_key) {
  // TODO(crbug.com/41363164): Introduce a CredentialID class to do this
  // extraction. Extract the length of the credential (i.e. of the U2FResponse
  // key handle). Length is big endian.
  std::vector<uint8_t> extracted_length =
      fido_parsing_utils::Extract(u2f_data, kU2fKeyHandleLengthOffset, 1);

  if (extracted_length.empty()) {
    return std::nullopt;
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
    return std::nullopt;
  }

  return AttestedCredentialData(aaguid, credential_id_length,
                                std::move(credential_id),
                                std::move(public_key));
}

AttestedCredentialData::AttestedCredentialData(AttestedCredentialData&& other) =
    default;

AttestedCredentialData::AttestedCredentialData(
    base::span<const uint8_t, kAaguidLength> aaguid,
    base::span<const uint8_t, kCredentialIdLengthLength>
        credential_id_length_bytes,
    std::vector<uint8_t> credential_id,
    std::unique_ptr<PublicKey> public_key)
    : aaguid_(fido_parsing_utils::Materialize(aaguid)),
      credential_id_length_(
          fido_parsing_utils::Materialize(credential_id_length_bytes)),
      credential_id_(std::move(credential_id)),
      public_key_(std::move(public_key)) {
  const size_t credential_id_length =
      (base::strict_cast<size_t>(credential_id_length_[0]) << 8) |
      base::strict_cast<size_t>(credential_id_length_[1]);
  CHECK_EQ(credential_id_length, credential_id_.size());
}

AttestedCredentialData::AttestedCredentialData(
    base::span<const uint8_t, kAaguidLength> aaguid,
    base::span<const uint8_t> credential_id,
    std::unique_ptr<PublicKey> public_key)
    : AttestedCredentialData(
          aaguid,
          std::array<uint8_t, kCredentialIdLengthLength>{
              base::checked_cast<uint8_t>((0xff00 & credential_id.size()) >> 8),
              base::checked_cast<uint8_t>(0xff & credential_id.size())},
          fido_parsing_utils::Materialize(credential_id),
          std::move(public_key)) {
  CHECK_LE(credential_id.size(), 0xffffu);
}

AttestedCredentialData& AttestedCredentialData::operator=(
    AttestedCredentialData&& other) = default;

AttestedCredentialData::~AttestedCredentialData() = default;

bool AttestedCredentialData::IsAaguidZero() const {
  return base::ranges::all_of(aaguid_, [](uint8_t v) { return v == 0; });
}

bool AttestedCredentialData::DeleteAaguid() {
  if (IsAaguidZero()) {
    return false;
  }
  std::fill(aaguid_.begin(), aaguid_.end(), 0);
  return true;
}

std::vector<uint8_t> AttestedCredentialData::SerializeAsBytes() const {
  std::vector<uint8_t> attestation_data;
  fido_parsing_utils::Append(&attestation_data, aaguid_);
  fido_parsing_utils::Append(&attestation_data, credential_id_length_);
  fido_parsing_utils::Append(&attestation_data, credential_id_);
  fido_parsing_utils::Append(&attestation_data, public_key_->cose_key_bytes);
  return attestation_data;
}

const PublicKey* AttestedCredentialData::public_key() const {
  return public_key_.get();
}

}  // namespace device
