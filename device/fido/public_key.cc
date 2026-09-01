// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public_key.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/cose.h"
#include "crypto/keypair.h"
#include "device/fido/public/fido_constants.h"

namespace device {

namespace {

std::unique_ptr<PublicKey> ParseEcP256CoseKey(
    int32_t algorithm,
    base::span<const uint8_t> cbor_bytes,
    const cbor::Value::MapValue& map) {
  const auto crv_it =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kEllipticCurve)));
  if (crv_it == map.end() || !crv_it->second.is_integer() ||
      crv_it->second.GetInteger() != static_cast<int64_t>(CoseCurves::kP256)) {
    return nullptr;
  }

  const auto x_it =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kEllipticX)));
  if (x_it == map.end() || !x_it->second.is_bytestring() ||
      x_it->second.GetBytestring().size() != 32) {
    return nullptr;
  }

  const auto y_it =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kEllipticY)));
  if (y_it == map.end() || !y_it->second.is_bytestring() ||
      y_it->second.GetBytestring().size() != 32) {
    return nullptr;
  }

  const auto& x = x_it->second.GetBytestring();
  const auto& y = y_it->second.GetBytestring();

  std::vector<uint8_t> uncompressed_point;
  uncompressed_point.reserve(65);
  uncompressed_point.push_back(0x04);
  base::Extend(uncompressed_point, x);
  base::Extend(uncompressed_point, y);

  auto key = crypto::keypair::PublicKey::FromEcP256Point(uncompressed_point);
  if (!key) {
    FIDO_LOG(ERROR) << "P-256 public key is not on curve";
    return nullptr;
  }
  return std::make_unique<PublicKey>(algorithm, cbor_bytes,
                                     key->ToSubjectPublicKeyInfo());
}

std::unique_ptr<PublicKey> ParseEd25519CoseKey(
    int32_t algorithm,
    base::span<const uint8_t> cbor_bytes,
    const cbor::Value::MapValue& map) {
  const auto crv_it =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kEllipticCurve)));
  if (crv_it == map.end() || !crv_it->second.is_integer() ||
      crv_it->second.GetInteger() !=
          static_cast<int64_t>(CoseCurves::kEd25519)) {
    return nullptr;
  }

  const auto x_it =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kEllipticX)));
  if (x_it == map.end() || !x_it->second.is_bytestring() ||
      x_it->second.GetBytestring().size() != 32) {
    return nullptr;
  }

  const auto& x = base::span<const uint8_t, 32>(x_it->second.GetBytestring());
  auto key = crypto::keypair::PublicKey::FromEd25519PublicKey(x);
  return std::make_unique<PublicKey>(algorithm, cbor_bytes,
                                     key.ToSubjectPublicKeyInfo());
}

std::unique_ptr<PublicKey> ParseRsaCoseKey(int32_t algorithm,
                                           base::span<const uint8_t> cbor_bytes,
                                           const cbor::Value::MapValue& map) {
  const auto n_it =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kRSAModulus)));
  if (n_it == map.end() || !n_it->second.is_bytestring()) {
    return nullptr;
  }

  const auto e_it = map.find(
      cbor::Value(static_cast<int64_t>(CoseKeyKey::kRSAPublicExponent)));
  if (e_it == map.end() || !e_it->second.is_bytestring()) {
    return nullptr;
  }

  auto key = crypto::keypair::PublicKey::FromRsaPublicKeyComponents(
      n_it->second.GetBytestring(), e_it->second.GetBytestring());
  if (!key) {
    FIDO_LOG(ERROR) << "Invalid RSA public key";
    return nullptr;
  }
  return std::make_unique<PublicKey>(algorithm, cbor_bytes,
                                     key->ToSubjectPublicKeyInfo());
}

std::unique_ptr<PublicKey> ParseMldsaCoseKey(
    int32_t algorithm,
    base::span<const uint8_t> cbor_bytes,
    const cbor::Value::MapValue& map) {
  const auto pubkey_it =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kAkpPublicKey)));
  if (pubkey_it == map.end() || !pubkey_it->second.is_bytestring()) {
    return nullptr;
  }

  std::optional<crypto::keypair::PublicKey> key;
  switch (static_cast<CoseAlgorithmIdentifier>(algorithm)) {
    case CoseAlgorithmIdentifier::kMlDsa44:
      key = crypto::keypair::PublicKey::FromMldsa44PublicKey(
          pubkey_it->second.GetBytestring());
      break;
    case CoseAlgorithmIdentifier::kMlDsa65:
      key = crypto::keypair::PublicKey::FromMldsa65PublicKey(
          pubkey_it->second.GetBytestring());
      break;
    case CoseAlgorithmIdentifier::kMlDsa87:
      key = crypto::keypair::PublicKey::FromMldsa87PublicKey(
          pubkey_it->second.GetBytestring());
      break;
    default:
      NOTREACHED();
  }
  if (!key) {
    FIDO_LOG(ERROR) << "Invalid ML-DSA public key";
    return nullptr;
  }
  return std::make_unique<PublicKey>(algorithm, cbor_bytes,
                                     key->ToSubjectPublicKeyInfo());
}

}  // namespace

// static
std::unique_ptr<PublicKey> PublicKey::FromCOSEKey(
    int32_t algorithm,
    base::span<const uint8_t> cbor_bytes,
    const cbor::Value::MapValue& map) {
  const auto kty_it =
      map.find(cbor::Value(static_cast<int64_t>(CoseKeyKey::kKty)));
  if (kty_it == map.end() || !kty_it->second.is_integer()) {
    return nullptr;
  }

  const int64_t kty = kty_it->second.GetInteger();
  if (kty == static_cast<int64_t>(CoseKeyTypes::kEC2)) {
    return ParseEcP256CoseKey(algorithm, cbor_bytes, map);
  }
  if (kty == static_cast<int64_t>(CoseKeyTypes::kOKP)) {
    return ParseEd25519CoseKey(algorithm, cbor_bytes, map);
  }
  if (kty == static_cast<int64_t>(CoseKeyTypes::kRSA)) {
    return ParseRsaCoseKey(algorithm, cbor_bytes, map);
  }
  if (kty == static_cast<int64_t>(CoseKeyTypes::kAKP)) {
    switch (static_cast<CoseAlgorithmIdentifier>(algorithm)) {
      case CoseAlgorithmIdentifier::kMlDsa44:
      case CoseAlgorithmIdentifier::kMlDsa65:
      case CoseAlgorithmIdentifier::kMlDsa87:
        return ParseMldsaCoseKey(algorithm, cbor_bytes, map);
      default:
        return std::make_unique<PublicKey>(algorithm, cbor_bytes, std::nullopt);
    }
  }

  return std::make_unique<PublicKey>(algorithm, cbor_bytes, std::nullopt);
}

// static
std::unique_ptr<PublicKey> PublicKey::FromSpkiDer(
    int32_t algorithm,
    base::span<const uint8_t> spki_der) {
  auto key = crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(spki_der);
  if (!key || (!key->IsEcP256() && !key->IsRsa() && !key->IsEd25519() &&
               !key->IsMldsa44() && !key->IsMldsa65() && !key->IsMldsa87())) {
    return nullptr;
  }
  return std::make_unique<PublicKey>(
      algorithm, crypto::PublicKeyToCoseKey(*key), base::ToVector(spki_der));
}

// static
std::unique_ptr<PublicKey> PublicKey::FromU2fRegistrationResponse(
    int32_t algorithm,
    base::span<const uint8_t> u2f_data) {
  constexpr size_t kUncompressedPointLength = 65;
  // In a U2F registration response, there is first a reserved byte that must be
  // ignored. Following that is the rest of the response.
  if (u2f_data.size() < 1 + kUncompressedPointLength) {
    return nullptr;
  }
  return FromRawP256UncompressedPoint(
      algorithm, u2f_data.subspan(1u, kUncompressedPointLength));
}

// static
std::unique_ptr<PublicKey> PublicKey::FromRawP256UncompressedPoint(
    int32_t algorithm,
    base::span<const uint8_t> x962) {
  auto key = crypto::keypair::PublicKey::FromEcP256Point(x962);
  if (!key) {
    FIDO_LOG(ERROR) << "P-256 public key is not on curve";
    return nullptr;
  }
  return std::make_unique<PublicKey>(algorithm,
                                     crypto::PublicKeyToCoseKey(*key),
                                     key->ToSubjectPublicKeyInfo());
}

PublicKey::PublicKey(int32_t in_algorithm,
                     base::span<const uint8_t> in_cose_key_bytes,
                     std::optional<std::vector<uint8_t>> in_der_bytes)
    : algorithm(in_algorithm),
      cose_key_bytes(base::ToVector(in_cose_key_bytes)),
      der_bytes(std::move(in_der_bytes)) {}

PublicKey::~PublicKey() = default;

}  // namespace device
