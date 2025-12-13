// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/jwk_utils.h"

#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "crypto/evp.h"
#include "crypto/keypair.h"
#include "crypto/sha2.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace net::device_bound_sessions {

namespace {
// The format of JSON Web Key (JWK) is specified in the section 4 of RFC 7517:
// https://www.ietf.org/rfc/rfc7517.html#section-4
//
// The parameters of a particular key type are specified by the JWA spec:
// https://www.ietf.org/rfc/rfc7518.html#section-6
constexpr char kKeyTypeParam[] = "kty";
constexpr char kEcKeyType[] = "EC";
constexpr char kEcCurve[] = "crv";
constexpr char kEcCurveP256[] = "P-256";
constexpr char kEcCoordinateX[] = "x";
constexpr char kEcCoordinateY[] = "y";
constexpr char kRsaKeyType[] = "RSA";
constexpr char kRsaModulus[] = "n";
constexpr char kRsaExponent[] = "e";

std::string Base64UrlEncode(base::span<const uint8_t> input) {
  std::string output;
  base::Base64UrlEncode(input, base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &output);
  return output;
}

base::Value::Dict ConvertES256PkeySpkiToJwk(
    base::span<const uint8_t> pkey_spki) {
  bssl::UniquePtr<EVP_PKEY> pkey = crypto::evp::PublicKeyFromBytes(pkey_spki);
  if (!pkey || EVP_PKEY_id(pkey.get()) != EVP_PKEY_EC) {
    return base::Value::Dict();
  }

  EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(pkey.get());
  if (!ec_key) {
    return base::Value::Dict();
  }

  const EC_GROUP* group = EC_KEY_get0_group(ec_key);
  const EC_POINT* point = EC_KEY_get0_public_key(ec_key);
  if (!group || !point) {
    return base::Value::Dict();
  }

  bssl::UniquePtr<BIGNUM> x(BN_new());
  bssl::UniquePtr<BIGNUM> y(BN_new());
  if (!x || !y) {
    return base::Value::Dict();
  }

  if (!EC_POINT_get_affine_coordinates_GFp(group, point, x.get(), y.get(),
                                           nullptr)) {
    return base::Value::Dict();
  }

  std::vector<uint8_t> x_bytes(32);
  std::vector<uint8_t> y_bytes(32);
  if (!BN_bn2bin_padded(x_bytes.data(), x_bytes.size(), x.get()) ||
      !BN_bn2bin_padded(y_bytes.data(), y_bytes.size(), y.get())) {
    return base::Value::Dict();
  }

  return base::Value::Dict()
      .Set(kKeyTypeParam, kEcKeyType)
      .Set(kEcCurve, kEcCurveP256)
      .Set(kEcCoordinateX, Base64UrlEncode(x_bytes))
      .Set(kEcCoordinateY, Base64UrlEncode(y_bytes));
}

base::Value::Dict ConvertRS256PkeySpkiToJwk(
    base::span<const uint8_t> pkey_spki) {
  std::optional<crypto::keypair::PublicKey> key =
      crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(pkey_spki);
  if (!key || !key->IsRsa()) {
    return base::Value::Dict();
  }

  return base::Value::Dict()
      .Set(kKeyTypeParam, kRsaKeyType)
      .Set(kRsaModulus, Base64UrlEncode(key->GetRsaModulus()))
      .Set(kRsaExponent, Base64UrlEncode(key->GetRsaExponent()));
}
}  // namespace

base::Value::Dict ConvertPkeySpkiToJwk(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pkey_spki) {
  switch (algorithm) {
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
      return ConvertRS256PkeySpkiToJwk(pkey_spki);
    case crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      return ConvertES256PkeySpkiToJwk(pkey_spki);
    default:
      return base::Value::Dict();
  }
}

std::string CreateJwkThumbprint(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pkey_spki) {
  base::Value::Dict jwk = ConvertPkeySpkiToJwk(algorithm, pkey_spki);
  if (jwk.empty()) {
    return "";
  }

  // Move only the required fields from `jwk` to `canonical_jwk`.
  base::Value::Dict canonical_jwk;
  switch (algorithm) {
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
      canonical_jwk.Set(kKeyTypeParam, std::move(*jwk.Extract(kKeyTypeParam)));
      canonical_jwk.Set(kRsaExponent, std::move(*jwk.Extract(kRsaExponent)));
      canonical_jwk.Set(kRsaModulus, std::move(*jwk.Extract(kRsaModulus)));
      break;
    case crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      canonical_jwk.Set(kKeyTypeParam, std::move(*jwk.Extract(kKeyTypeParam)));
      canonical_jwk.Set(kEcCurve, std::move(*jwk.Extract(kEcCurve)));
      canonical_jwk.Set(kEcCoordinateX,
                        std::move(*jwk.Extract(kEcCoordinateX)));
      canonical_jwk.Set(kEcCoordinateY,
                        std::move(*jwk.Extract(kEcCoordinateY)));
      break;
    default:
      NOTREACHED();
  }

  // The canonical representation of the JWK requires the keys to be sorted
  // alphabetically. `base::Value::Dict` is already sorted.
  std::string canonical_jwk_string;
  CHECK(base::JSONWriter::Write(canonical_jwk, &canonical_jwk_string));

  std::string thumbprint_hash = crypto::SHA256HashString(canonical_jwk_string);
  return Base64UrlEncode(base::as_bytes(base::span(thumbprint_hash)));
}

}  // namespace net::device_bound_sessions
