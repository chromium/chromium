// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/p256_public_key.h"

#include <utility>

#include "base/memory/raw_ptr_exclusion.h"
#include "components/cbor/writer.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/cbor_extract.h"
#include "device/fido/fido_constants.h"
#include "device/fido/public_key.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

using device::cbor_extract::IntKey;
using device::cbor_extract::Is;
using device::cbor_extract::StepOrByte;
using device::cbor_extract::Stop;

namespace device {

// kFieldElementLength is the size of a P-256 field element. The field is
// GF(2^256 - 2^224 + 2^192 + 2^96 - 1) and thus an element is 256 bits, or 32
// bytes.
constexpr size_t kFieldElementLength = 32;

// kUncompressedPointLength is the size of an X9.62 uncompressed point over
// P-256. It's one byte of type information followed by two field elements (x
// and y).
constexpr size_t kUncompressedPointLength = 1 + 2 * kFieldElementLength;

namespace {

// DERFromEC_POINT returns the ASN.1, DER, SubjectPublicKeyInfo for a given
// elliptic-curve point.
static std::vector<uint8_t> DERFromEC_POINT(const EC_POINT* point) {
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  CHECK(EC_KEY_set_public_key(ec_key.get(), point));
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  CHECK(EVP_PKEY_assign_EC_KEY(pkey.get(), ec_key.release()));

  bssl::ScopedCBB cbb;
  uint8_t* der_bytes = nullptr;
  size_t der_bytes_len = 0;
  CHECK(CBB_init(cbb.get(), /* initial size */ 128) &&
        EVP_marshal_public_key(cbb.get(), pkey.get()) &&
        CBB_finish(cbb.get(), &der_bytes, &der_bytes_len));

  std::vector<uint8_t> ret(der_bytes, der_bytes + der_bytes_len);
  OPENSSL_free(der_bytes);
  return ret;
}

}  // namespace

// static
std::unique_ptr<PublicKey> P256PublicKey::ExtractFromU2fRegistrationResponse(
    int32_t algorithm,
    base::span<const uint8_t> u2f_data) {
  // In a U2F registration response, there is first a reserved byte that must be
  // ignored. Following that is the rest of the response.
  if (u2f_data.size() < 1 + kUncompressedPointLength) {
    return nullptr;
  }
  return ParseX962Uncompressed(algorithm,
                               u2f_data.subspan(1, kUncompressedPointLength));
}

// static
std::unique_ptr<PublicKey> P256PublicKey::ExtractFromCOSEKey(
    int32_t algorithm,
    base::span<const uint8_t> cbor_bytes,
    const cbor::Value::MapValue& map) {
  struct COSEKey {
    // All the fields below are not a raw_ptr<,,,>, because ELEMENT() treats the
    // raw_ptr<T> as a void*, skipping AddRef() call and causing a ref-counting
    // mismatch.
    RAW_PTR_EXCLUSION const int64_t* kty;
    RAW_PTR_EXCLUSION const int64_t* crv;
    RAW_PTR_EXCLUSION const std::vector<uint8_t>* x;
    RAW_PTR_EXCLUSION const std::vector<uint8_t>* y;
  } cose_key;

  static constexpr cbor_extract::StepOrByte<COSEKey> kSteps[] = {
      // clang-format off
      ELEMENT(Is::kRequired, COSEKey, kty),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kKty)),

      ELEMENT(Is::kRequired, COSEKey, crv),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kEllipticCurve)),

      ELEMENT(Is::kRequired, COSEKey, x),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kEllipticX)),

      ELEMENT(Is::kRequired, COSEKey, y),
      IntKey<COSEKey>(static_cast<int>(CoseKeyKey::kEllipticY)),

      Stop<COSEKey>(),
      // clang-format on
  };

  if (!cbor_extract::Extract<COSEKey>(&cose_key, kSteps, map) ||
      *cose_key.kty != static_cast<int64_t>(CoseKeyTypes::kEC2) ||
      *cose_key.crv != static_cast<int64_t>(CoseCurves::kP256) ||
      cose_key.x->size() != kFieldElementLength ||
      cose_key.y->size() != kFieldElementLength) {
    return nullptr;
  }

  bssl::UniquePtr<BIGNUM> x_bn(BN_new());
  bssl::UniquePtr<BIGNUM> y_bn(BN_new());
  if (!BN_bin2bn(cose_key.x->data(), cose_key.x->size(), x_bn.get()) ||
      !BN_bin2bn(cose_key.y->data(), cose_key.y->size(), y_bn.get())) {
    return nullptr;
  }

  // Parse into an |EC_POINT| to perform the validity checks that BoringSSL
  // does.
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (!EC_POINT_set_affine_coordinates_GFp(p256.get(), point.get(), x_bn.get(),
                                           y_bn.get(), /*ctx=*/nullptr)) {
    FIDO_LOG(ERROR) << "P-256 public key is not on curve";
    return nullptr;
  }

  return std::make_unique<PublicKey>(algorithm, cbor_bytes,
                                     DERFromEC_POINT(point.get()));
}

// static
std::unique_ptr<PublicKey> P256PublicKey::ParseX962Uncompressed(
    int32_t algorithm,
    base::span<const uint8_t> x962) {
  // Parse into an |EC_POINT| to perform the validity checks that BoringSSL
  // does.
  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> point(EC_POINT_new(p256.get()));
  if (x962.size() != kUncompressedPointLength ||
      x962[0] != POINT_CONVERSION_UNCOMPRESSED ||
      !EC_POINT_oct2point(p256.get(), point.get(), x962.data(), x962.size(),
                          /*ctx=*/nullptr)) {
    FIDO_LOG(ERROR) << "P-256 public key is not on curve";
    return nullptr;
  }

  base::span<const uint8_t, kFieldElementLength> x(&x962[1],
                                                   kFieldElementLength);
  base::span<const uint8_t, kFieldElementLength> y(
      &x962[1 + kFieldElementLength], kFieldElementLength);

  cbor::Value::MapValue map;
  map.emplace(static_cast<int>(CoseKeyKey::kKty),
              static_cast<int64_t>(CoseKeyTypes::kEC2));
  map.emplace(static_cast<int>(CoseKeyKey::kAlg), algorithm);
  map.emplace(static_cast<int>(CoseKeyKey::kEllipticCurve),
              static_cast<int64_t>(CoseCurves::kP256));
  map.emplace(static_cast<int>(CoseKeyKey::kEllipticX), x);
  map.emplace(static_cast<int>(CoseKeyKey::kEllipticY), y);

  const std::vector<uint8_t> cbor_bytes(
      std::move(cbor::Writer::Write(cbor::Value(std::move(map))).value()));

  return std::make_unique<PublicKey>(algorithm, cbor_bytes,
                                     DERFromEC_POINT(point.get()));
}

// static
std::unique_ptr<PublicKey> P256PublicKey::ParseSpkiDer(
    int32_t algorithm,
    base::span<const uint8_t> spki_der) {
  CBS cbs;
  CBS_init(&cbs, spki_der.data(), spki_der.size());
  bssl::UniquePtr<EVP_PKEY> public_key(EVP_parse_public_key(&cbs));
  if (!public_key || CBS_len(&cbs) != 0 ||
      EVP_PKEY_id(public_key.get()) != EVP_PKEY_EC) {
    return nullptr;
  }
  bssl::UniquePtr<EC_KEY> ec_key(EVP_PKEY_get1_EC_KEY(public_key.get()));
  if (!ec_key || EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key.get())) !=
                     NID_X9_62_prime256v1) {
    return nullptr;
  }
  const EC_POINT* ec_point = EC_KEY_get0_public_key(ec_key.get());
  if (!ec_point) {
    return nullptr;
  }

  bssl::UniquePtr<BIGNUM> x_bn(BN_new());
  bssl::UniquePtr<BIGNUM> y_bn(BN_new());
  if (!EC_POINT_get_affine_coordinates_GFp(EC_KEY_get0_group(ec_key.get()),
                                           ec_point, x_bn.get(), y_bn.get(),
                                           nullptr)) {
    return nullptr;
  }

  std::vector<uint8_t> x(kFieldElementLength);
  std::vector<uint8_t> y(kFieldElementLength);
  if (!BN_bn2binpad(x_bn.get(), x.data(), x.size()) ||
      !BN_bn2binpad(y_bn.get(), y.data(), y.size())) {
    return nullptr;
  }

  cbor::Value::MapValue map;
  map.emplace(static_cast<int>(CoseKeyKey::kKty),
              static_cast<int64_t>(CoseKeyTypes::kEC2));
  map.emplace(static_cast<int>(CoseKeyKey::kAlg), algorithm);
  map.emplace(static_cast<int>(CoseKeyKey::kEllipticCurve),
              static_cast<int64_t>(CoseCurves::kP256));
  map.emplace(static_cast<int>(CoseKeyKey::kEllipticX), std::move(x));
  map.emplace(static_cast<int>(CoseKeyKey::kEllipticY), std::move(y));

  const std::vector<uint8_t> cbor_bytes(
      std::move(cbor::Writer::Write(cbor::Value(std::move(map))).value()));

  return std::make_unique<PublicKey>(
      algorithm, cbor_bytes,
      std::vector<uint8_t>(spki_der.begin(), spki_der.end()));
}

}  // namespace device
