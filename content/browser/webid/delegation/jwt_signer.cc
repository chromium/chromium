// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/delegation/jwt_signer.h"

#include <map>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/keypair.h"
#include "crypto/openssl_util.h"
#include "crypto/random.h"
#include "crypto/sign.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content::sdjwt {

namespace {

// Rounds a bit count (up) to the nearest byte count.
//
// This is mathematically equivalent to (x + 7) / 8, however has no
// possibility of integer overflow.
template <typename T>
T NumBitsToBytes(T x) {
  return (x / 8) + (7 + (x % 8)) / 8;
}

int GetGroupDegreeInBytes(EC_KEY* ec) {
  const EC_GROUP* group = EC_KEY_get0_group(ec);
  return NumBitsToBytes(EC_GROUP_get_degree(group));
}

std::optional<std::string> BIGNUMToPadded(const BIGNUM* value,
                                          size_t padded_length) {
  std::vector<uint8_t> padded_bytes(padded_length);
  if (!BN_bn2bin_padded(padded_bytes.data(), padded_bytes.size(), value)) {
    return std::nullopt;
  }

  std::string base64;
  base::Base64UrlEncode(base::as_byte_span(padded_bytes),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &base64);

  return base64;
}

std::optional<std::string> BIGNUMToBase64(const BIGNUM* value) {
  return BIGNUMToPadded(value, BN_num_bytes(value));
}

std::optional<std::vector<uint8_t>> SignJwtEs256(
    crypto::keypair::PrivateKey private_key,
    const std::string_view& message) {
  // The signature unpacking step won't work if the key uses a curve other than
  // P-256.
  if (!private_key.IsEcP256()) {
    return std::nullopt;
  }

  const auto sig = crypto::sign::Sign(crypto::sign::SignatureKind::ECDSA_SHA256,
                                      private_key, base::as_byte_span(message));
  return crypto::ConvertEcdsaDerSignatureToRaw(
      crypto::keypair::PublicKey::FromPrivateKey(private_key), sig);
}

std::optional<std::vector<uint8_t>> SignJwtRs256(
    crypto::keypair::PrivateKey private_key,
    const std::string_view& message) {
  if (!private_key.IsRsa()) {
    return std::nullopt;
  }

  return crypto::sign::Sign(crypto::sign::SignatureKind::RSA_PKCS1_SHA256,
                            private_key, base::as_byte_span(message));
}

std::optional<Jwk> ExportPublicKeyEs256(
    const crypto::keypair::PrivateKey& private_key) {
  Jwk jwk;

  jwk.kty = "EC";
  jwk.crv = "P-256";
  jwk.alg = "ES256";

  // Get public key
  bssl::UniquePtr<BIGNUM> x(BN_new());
  bssl::UniquePtr<BIGNUM> y(BN_new());

  EC_KEY* ec = EVP_PKEY_get0_EC_KEY(private_key.key());

  const EC_GROUP* group = EC_KEY_get0_group(ec);
  const EC_POINT* point = EC_KEY_get0_public_key(ec);

  if (!EC_POINT_get_affine_coordinates_GFp(group, point, x.get(), y.get(),
                                           nullptr)) {
    return std::nullopt;
  }

  int degree_bytes = GetGroupDegreeInBytes(ec);

  auto x_base64 = BIGNUMToPadded(x.get(), degree_bytes);
  if (!x_base64) {
    return std::nullopt;
  }

  jwk.x = *x_base64;

  auto y_base64 = BIGNUMToPadded(y.get(), degree_bytes);
  if (!y_base64) {
    return std::nullopt;
  }

  jwk.y = *y_base64;

  return jwk;
}

std::optional<Jwk> ExportPublicKeyRsa256(
    const crypto::keypair::PrivateKey& private_key) {
  Jwk jwk;

  RSA* rsa = EVP_PKEY_get0_RSA(private_key.key());

  jwk.kty = "RSA";
  jwk.alg = "RS256";

  const BIGNUM* n;
  const BIGNUM* e;
  RSA_get0_key(rsa, &n, &e, nullptr);

  auto n_base64 = BIGNUMToBase64(n);
  if (!n_base64) {
    return std::nullopt;
  }
  jwk.n = *n_base64;

  auto e_base64 = BIGNUMToBase64(e);
  if (!e_base64) {
    return std::nullopt;
  }
  jwk.e = *e_base64;

  return jwk;
}

}  // namespace

std::optional<Jwk> ExportPublicKey(
    const crypto::keypair::PrivateKey& private_key) {
  if (private_key.IsEcP256()) {
    return ExportPublicKeyEs256(private_key);
  }

  if (private_key.IsRsa()) {
    return ExportPublicKeyRsa256(private_key);
  }

  return std::nullopt;
}

Signer CreateJwtSigner(crypto::keypair::PrivateKey private_key) {
  switch (EVP_PKEY_base_id(private_key.key())) {
    case EVP_PKEY_EC:
      return base::BindOnce(&SignJwtEs256, std::move(private_key));
    case EVP_PKEY_RSA:
      return base::BindOnce(&SignJwtRs256, std::move(private_key));
    default:
      return base::BindOnce(
          [](const std::string_view&) -> std::optional<std::vector<uint8_t>> {
            return std::nullopt;
          });
  }
}

}  // namespace content::sdjwt
