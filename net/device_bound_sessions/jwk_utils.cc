// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/jwk_utils.h"

#include "base/base64url.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
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

bssl::UniquePtr<EVP_PKEY> ParsePublicKey(base::span<const uint8_t> pkey_spki) {
  CBS cbs;
  CBS_init(&cbs, pkey_spki.data(), pkey_spki.size());
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
  if (CBS_len(&cbs) != 0) {
    return nullptr;
  }
  return pkey;
}

base::Value::Dict ConvertES256PkeySpkiToJwk(
    base::span<const uint8_t> pkey_spki) {
  bssl::UniquePtr<EVP_PKEY> pkey = ParsePublicKey(pkey_spki);
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

  std::vector<uint8_t> x_bytes(BN_num_bytes(x.get()));
  std::vector<uint8_t> y_bytes(BN_num_bytes(y.get()));
  BN_bn2bin(x.get(), x_bytes.data());
  BN_bn2bin(y.get(), y_bytes.data());

  return base::Value::Dict()
      .Set(kKeyTypeParam, kEcKeyType)
      .Set(kEcCurve, kEcCurveP256)
      .Set(kEcCoordinateX, Base64UrlEncode(x_bytes))
      .Set(kEcCoordinateY, Base64UrlEncode(y_bytes));
}

base::Value::Dict ConvertRS256PkeySpkiToJwk(
    base::span<const uint8_t> pkey_spki) {
  bssl::UniquePtr<EVP_PKEY> pkey = ParsePublicKey(pkey_spki);
  if (!pkey || EVP_PKEY_id(pkey.get()) != EVP_PKEY_RSA) {
    return base::Value::Dict();
  }

  RSA* rsa_key = EVP_PKEY_get0_RSA(pkey.get());
  if (!rsa_key) {
    return base::Value::Dict();
  }

  const BIGNUM* n = RSA_get0_n(rsa_key);
  const BIGNUM* e = RSA_get0_e(rsa_key);
  if (!n || !e) {
    return base::Value::Dict();
  }

  std::vector<uint8_t> n_bytes(BN_num_bytes(n));
  std::vector<uint8_t> e_bytes(BN_num_bytes(e));
  BN_bn2bin(n, n_bytes.data());
  BN_bn2bin(e, e_bytes.data());

  return base::Value::Dict()
      .Set(kKeyTypeParam, kRsaKeyType)
      .Set(kRsaModulus, Base64UrlEncode(n_bytes))
      .Set(kRsaExponent, Base64UrlEncode(e_bytes));
}
}  // namespace

base::Value::Dict ConvertPkeySpkiToJwk(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> pkey_spki) {
  // TODO(crbug.com/360756896): Support more algorithms.
  switch (algorithm) {
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
      return ConvertRS256PkeySpkiToJwk(pkey_spki);
    case crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      return ConvertES256PkeySpkiToJwk(pkey_spki);
    default:
      return base::Value::Dict();
  }
}

}  // namespace net::device_bound_sessions
