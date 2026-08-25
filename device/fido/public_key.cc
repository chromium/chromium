// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public_key.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "components/device_event_log/device_event_log.h"
#include "crypto/cose.h"
#include "crypto/keypair.h"
#include "device/fido/cbor_extract.h"
#include "device/fido/public/fido_constants.h"

using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;

namespace device {

namespace {

std::unique_ptr<PublicKey> ParseEcP256CoseKey(
    int32_t algorithm,
    base::span<const uint8_t> cbor_bytes,
    const cbor::Value::MapValue& map) {
  struct EC2Key {
    RAW_PTR_EXCLUSION const int64_t* crv;
    RAW_PTR_EXCLUSION const std::vector<uint8_t>* x;
    RAW_PTR_EXCLUSION const std::vector<uint8_t>* y;
  } ec2_key;
  static constexpr cbor_extract::StepOrByte<EC2Key> kSteps[] = {
      // clang-format off
      ELEMENT(Is::kRequired, EC2Key, crv),
      IntKey<EC2Key>(static_cast<int>(CoseKeyKey::kEllipticCurve)),

      ELEMENT(Is::kRequired, EC2Key, x),
      IntKey<EC2Key>(static_cast<int>(CoseKeyKey::kEllipticX)),

      ELEMENT(Is::kRequired, EC2Key, y),
      IntKey<EC2Key>(static_cast<int>(CoseKeyKey::kEllipticY)),

      Stop<EC2Key>(),
      // clang-format on
  };

  if (!cbor_extract::Extract<EC2Key>(&ec2_key, kSteps, map) ||
      *ec2_key.crv != static_cast<int64_t>(CoseCurves::kP256) ||
      ec2_key.x->size() != 32 || ec2_key.y->size() != 32) {
    return nullptr;
  }

  std::vector<uint8_t> uncompressed_point;
  uncompressed_point.reserve(65);
  uncompressed_point.push_back(0x04);
  uncompressed_point.insert(uncompressed_point.end(), ec2_key.x->begin(),
                            ec2_key.x->end());
  uncompressed_point.insert(uncompressed_point.end(), ec2_key.y->begin(),
                            ec2_key.y->end());

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
  struct OKPKey {
    RAW_PTR_EXCLUSION const int64_t* crv;
    RAW_PTR_EXCLUSION const std::vector<uint8_t>* x;
  } okp_key;
  static constexpr cbor_extract::StepOrByte<OKPKey> kSteps[] = {
      // clang-format off
      ELEMENT(Is::kRequired, OKPKey, crv),
      IntKey<OKPKey>(static_cast<int>(CoseKeyKey::kEllipticCurve)),

      ELEMENT(Is::kRequired, OKPKey, x),
      IntKey<OKPKey>(static_cast<int>(CoseKeyKey::kEllipticX)),

      Stop<OKPKey>(),
      // clang-format on
  };

  if (!cbor_extract::Extract<OKPKey>(&okp_key, kSteps, map) ||
      *okp_key.crv != static_cast<int64_t>(CoseCurves::kEd25519) ||
      okp_key.x->size() != 32) {
    return nullptr;
  }

  auto key = crypto::keypair::PublicKey::FromEd25519PublicKey(
      base::span<const uint8_t, 32>(*okp_key.x));
  return std::make_unique<PublicKey>(algorithm, cbor_bytes,
                                     key.ToSubjectPublicKeyInfo());
}

std::unique_ptr<PublicKey> ParseRsaCoseKey(int32_t algorithm,
                                           base::span<const uint8_t> cbor_bytes,
                                           const cbor::Value::MapValue& map) {
  struct RSAKey {
    RAW_PTR_EXCLUSION const std::vector<uint8_t>* n;
    RAW_PTR_EXCLUSION const std::vector<uint8_t>* e;
  } rsa_key;
  static constexpr cbor_extract::StepOrByte<RSAKey> kSteps[] = {
      // clang-format off
      ELEMENT(Is::kRequired, RSAKey, n),
      IntKey<RSAKey>(static_cast<int>(CoseKeyKey::kRSAModulus)),

      ELEMENT(Is::kRequired, RSAKey, e),
      IntKey<RSAKey>(static_cast<int>(CoseKeyKey::kRSAPublicExponent)),

      Stop<RSAKey>(),
      // clang-format on
  };

  if (!cbor_extract::Extract<RSAKey>(&rsa_key, kSteps, map)) {
    return nullptr;
  }

  auto key = crypto::keypair::PublicKey::FromRsaPublicKeyComponents(*rsa_key.n,
                                                                    *rsa_key.e);
  if (!key) {
    FIDO_LOG(ERROR) << "Invalid RSA public key";
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
  struct BaseKey {
    RAW_PTR_EXCLUSION const int64_t* kty;
  } base_key;
  static constexpr cbor_extract::StepOrByte<BaseKey> kBaseSteps[] = {
      // clang-format off
      ELEMENT(Is::kRequired, BaseKey, kty),
      IntKey<BaseKey>(static_cast<int>(CoseKeyKey::kKty)),
      Stop<BaseKey>(),
      // clang-format on
  };

  if (!cbor_extract::Extract<BaseKey>(&base_key, kBaseSteps, map)) {
    return nullptr;
  }

  if (*base_key.kty == static_cast<int64_t>(CoseKeyTypes::kEC2)) {
    return ParseEcP256CoseKey(algorithm, cbor_bytes, map);
  }
  if (*base_key.kty == static_cast<int64_t>(CoseKeyTypes::kOKP)) {
    return ParseEd25519CoseKey(algorithm, cbor_bytes, map);
  }
  if (*base_key.kty == static_cast<int64_t>(CoseKeyTypes::kRSA)) {
    return ParseRsaCoseKey(algorithm, cbor_bytes, map);
  }

  return std::make_unique<PublicKey>(algorithm, cbor_bytes, std::nullopt);
}

// static
std::unique_ptr<PublicKey> PublicKey::FromSpkiDer(
    int32_t algorithm,
    base::span<const uint8_t> spki_der) {
  auto key = crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(spki_der);
  if (!key || (!key->IsEcP256() && !key->IsRsa() && !key->IsEd25519())) {
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
