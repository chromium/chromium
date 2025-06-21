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

bool IsEcdsaP256(EVP_PKEY* evp_key) {
  if (EVP_PKEY_base_id(evp_key) != EVP_PKEY_EC) {
    return false;
  }

  EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(evp_key);
  CHECK(ec_key);

  return EC_KEY_get0_group(ec_key) == EC_group_p256();
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

// Given a DER-encoded ECDSA-Sig-Value, unpack it into a raw ECDSA signature:
// (r, s) represented as two big-endian, zero-padded 256-bit integers. This
// function requires that the input be a valid ECDSA signature and that both r
// and s are <= 256 bits.
std::vector<uint8_t> UnpackDERSignature(base::span<const uint8_t> der_sig) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  // Create ECDSA_SIG object from DER-encoded data.
  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(
      ECDSA_SIG_from_bytes(der_sig.data(), der_sig.size()));
  CHECK(ecdsa_sig.get());

  // The result is made of two 32-byte vectors.
  const size_t kMaxBytesPerBN = 32;
  std::vector<uint8_t> result(2 * kMaxBytesPerBN);

  CHECK(BN_bn2bin_padded(&result[0], kMaxBytesPerBN, ecdsa_sig->r));
  CHECK(
      BN_bn2bin_padded(&result[kMaxBytesPerBN], kMaxBytesPerBN, ecdsa_sig->s));

  return result;
}

std::optional<std::vector<uint8_t>> SignJwt(
    crypto::keypair::PrivateKey private_key,
    const std::string_view& message) {
  // The signature unpacking step won't work if the key uses a curve other than
  // P-256.
  if (!IsEcdsaP256(private_key.key())) {
    return std::nullopt;
  }

  const auto sig = crypto::sign::Sign(crypto::sign::SignatureKind::ECDSA_SHA256,
                                      private_key, base::as_byte_span(message));
  return UnpackDERSignature(sig);
}

}  // namespace

std::optional<Jwk> ExportPublicKey(
    const crypto::keypair::PrivateKey& private_pkey) {
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

Signer CreateJwtSigner(crypto::keypair::PrivateKey private_key) {
  return base::BindOnce(SignJwt, std::move(private_key));
}

}  // namespace content::sdjwt
