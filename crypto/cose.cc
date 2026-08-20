// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/cose.h"

#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace crypto {

namespace {

constexpr size_t kEcP256FieldElementLength = 32;

// Enumerates the keys in a COSE Key structure. See
// https://tools.ietf.org/html/rfc8152#section-7.1
enum class CoseKeyKey : int {
  kAlg = 3,
  kKty = 1,
  kRSAModulus = -1,
  kRSAPublicExponent = -2,
  kEllipticCurve = -1,
  kEllipticX = -2,
  kEllipticY = -3,
  kAkpPublicKey = -1,
};

// Enumerates COSE key types. See
// https://tools.ietf.org/html/rfc8152#section-13 and
// https://tools.ietf.org/html/rfc9964#name-new-cose-key-types.
enum class CoseKeyTypes : int {
  kOKP = 1,
  kEC2 = 2,
  kRSA = 3,
  kAKP = 7,
};

// Enumerates COSE elliptic curves. See
// https://tools.ietf.org/html/rfc8152#section-13.1
enum class CoseCurves : int {
  kP256 = 1,
  kEd25519 = 6,
};

// Enumerates COSE algorithm identifiers. See
// https://tools.ietf.org/html/rfc8152#section-13 and
// https://tools.ietf.org/html/rfc9964#section-8.1.
enum class CoseAlgorithmIdentifier : int {
  kEs256 = -7,
  kEdDSA = -8,
  kMlDsa44 = -48,
  kMlDsa65 = -49,
  kMlDsa87 = -50,
  kRs256 = -257,
};

std::vector<uint8_t> RsaToCoseKey(const EVP_PKEY* key) {
  // Extract the RSA key and its components (n and e).
  const RSA* rsa_key = EVP_PKEY_get0_RSA(key);
  CHECK(rsa_key);

  const BIGNUM* n = RSA_get0_n(rsa_key);  // modulus
  const BIGNUM* e = RSA_get0_e(rsa_key);  // public exponent
  CHECK(n && e);

  // Convert BIGNUM components to byte vectors.
  std::vector<uint8_t> n_bytes(BN_num_bytes(n));
  BN_bn2bin(n, n_bytes.data());
  std::vector<uint8_t> e_bytes(BN_num_bytes(e));
  BN_bn2bin(e, e_bytes.data());

  // Construct the COSE_Key CBOR Map.
  cbor::Value::MapValue map;
  map.emplace(static_cast<int64_t>(CoseKeyKey::kAlg),
              static_cast<int64_t>(CoseAlgorithmIdentifier::kRs256));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kKty),
              static_cast<int64_t>(CoseKeyTypes::kRSA));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kRSAModulus),
              std::move(n_bytes));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kRSAPublicExponent),
              std::move(e_bytes));

  std::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  CHECK(cbor_bytes);
  return cbor_bytes.value();
}

std::vector<uint8_t> EcP256ToCoseKey(const keypair::PublicKey& key) {
  // COSE's non-standard public key encoding is the x and y halves of the
  // standard X9.62 uncompressed encoding: 04 || x || y.
  std::vector<uint8_t> uncompressed = key.ToUncompressedX962Point();
  CHECK_EQ(uncompressed.size(), 1 + 2 * kEcP256FieldElementLength);
  auto [x, y] =
      base::span(uncompressed).subspan(1u).split_at(kEcP256FieldElementLength);

  cbor::Value::MapValue map;
  map.emplace(static_cast<int64_t>(CoseKeyKey::kKty),
              static_cast<int64_t>(CoseKeyTypes::kEC2));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kAlg),
              static_cast<int64_t>(CoseAlgorithmIdentifier::kEs256));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kEllipticCurve),
              static_cast<int64_t>(CoseCurves::kP256));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kEllipticX), std::move(x));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kEllipticY), std::move(y));

  std::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  CHECK(cbor_bytes);
  return cbor_bytes.value();
}

std::vector<uint8_t> Ed25519ToCoseKey(const keypair::PublicKey& key) {
  std::array<uint8_t, 32> pub_bytes = key.ToEd25519PublicKey();

  cbor::Value::MapValue map;
  map.emplace(static_cast<int64_t>(CoseKeyKey::kKty),
              static_cast<int64_t>(CoseKeyTypes::kOKP));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kAlg),
              static_cast<int64_t>(CoseAlgorithmIdentifier::kEdDSA));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kEllipticCurve),
              static_cast<int64_t>(CoseCurves::kEd25519));
  // The COSE RFC says that "This contains the x-coordinate for the EC point".
  // The RFC authors do not appear to understand what's going on because it
  // actually just contains the Ed25519 public key, which you would expect, and
  // which also encodes the y-coordinate as a sign bit.
  map.emplace(static_cast<int64_t>(CoseKeyKey::kEllipticX),
              std::vector<uint8_t>(std::from_range, pub_bytes));

  std::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  CHECK(cbor_bytes);
  return cbor_bytes.value();
}

std::vector<uint8_t> MldsaToCoseKey(std::vector<uint8_t> pub_bytes,
                                    CoseAlgorithmIdentifier alg) {
  cbor::Value::MapValue map;
  map.emplace(static_cast<int64_t>(CoseKeyKey::kKty),
              static_cast<int64_t>(CoseKeyTypes::kAKP));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kAlg),
              static_cast<int64_t>(alg));
  map.emplace(static_cast<int64_t>(CoseKeyKey::kAkpPublicKey),
              std::move(pub_bytes));

  std::optional<std::vector<uint8_t>> cbor_bytes =
      cbor::Writer::Write(cbor::Value(std::move(map)));
  CHECK(cbor_bytes);
  return cbor_bytes.value();
}

}  // namespace

std::vector<uint8_t> PublicKeyToCoseKey(const keypair::PublicKey& key) {
  if (key.IsRsa()) {
    return RsaToCoseKey(key.key());
  } else if (key.IsEcP256()) {
    return EcP256ToCoseKey(key);
  } else if (key.IsEd25519()) {
    return Ed25519ToCoseKey(key);
  } else if (key.IsMldsa44()) {
    return MldsaToCoseKey(key.ToMldsa44PublicKey(),
                          CoseAlgorithmIdentifier::kMlDsa44);
  } else if (key.IsMldsa65()) {
    return MldsaToCoseKey(key.ToMldsa65PublicKey(),
                          CoseAlgorithmIdentifier::kMlDsa65);
  } else if (key.IsMldsa87()) {
    return MldsaToCoseKey(key.ToMldsa87PublicKey(),
                          CoseAlgorithmIdentifier::kMlDsa87);
  }

  NOTREACHED();
}

}  // namespace crypto
