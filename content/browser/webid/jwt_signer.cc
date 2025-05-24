// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/jwt_signer.h"

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
#include "content/browser/webid/sd_jwt.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/random.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
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

std::optional<std::vector<uint8_t>> SignJwt(
    std::unique_ptr<crypto::ECPrivateKey> private_key,
    const std::string_view& message) {
  base::span<const uint8_t> data(base::as_byte_span(message));
  auto signer = crypto::ECSignatureCreator::Create(private_key.get());

  std::vector<uint8_t> der;
  if (!signer->Sign(data, &der)) {
    return std::nullopt;
  }

  std::vector<uint8_t> out;
  if (!signer->DecodeSignature(der, &out)) {
    return std::nullopt;
  }

  return out;
}

}  // namespace

std::optional<Jwk> ExportPublicKey(const crypto::ECPrivateKey& private_pkey) {
  EC_KEY* ec = EVP_PKEY_get0_EC_KEY(private_pkey.key());
  if (!ec) {
    return std::nullopt;
  }

  Jwk jwk;
  jwk.kty = "EC";
  jwk.crv = "P-256";

  // Get public key
  bssl::UniquePtr<BIGNUM> x(BN_new());
  bssl::UniquePtr<BIGNUM> y(BN_new());

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

Signer CreateJwtSigner(std::unique_ptr<crypto::ECPrivateKey> private_key) {
  return base::BindOnce(SignJwt, std::move(private_key));
}

}  // namespace content::sdjwt
