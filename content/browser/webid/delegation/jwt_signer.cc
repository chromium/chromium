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
#include "crypto/signature_verifier.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
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

std::optional<std::string> Base64UrlDecode(const std::string_view& base64) {
  std::string str;
  if (!base::Base64UrlDecode(
          base64, base::Base64UrlDecodePolicy::IGNORE_PADDING, &str)) {
    return std::nullopt;
  }
  return str;
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

  // TODO(crbug.com/380367784): should we move this call into the caller so that
  // we can generalize signing between ES256, RS256 and ED?
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

std::optional<std::vector<uint8_t>> SignJwtEdDsa(
    crypto::keypair::PrivateKey private_key,
    const std::string_view& message) {
  if (!private_key.IsEd25519()) {
    return std::nullopt;
  }
  std::vector<uint8_t> sig =
      crypto::sign::Sign(crypto::sign::SignatureKind::ED25519, private_key,
                         base::as_byte_span(message));
  return sig;
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

std::optional<Jwk> ExportPublicKeyEdDsa(
    const crypto::keypair::PrivateKey& private_key) {
  Jwk jwk;

  jwk.kty = "OKP";
  jwk.crv = "Ed25519";
  jwk.alg = "EdDSA";

  std::array<uint8_t, 32> public_key_raw = private_key.ToEd25519PublicKey();

  std::string x_base64;
  base::Base64UrlEncode(public_key_raw,
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &x_base64);

  jwk.x = x_base64;

  return jwk;
}

}  // namespace

std::optional<Jwk> ExportPublicKey(
    const crypto::keypair::PrivateKey& private_key) {
  if (private_key.IsEcP256()) {
    return ExportPublicKeyEs256(private_key);
  } else if (private_key.IsRsa()) {
    return ExportPublicKeyRsa256(private_key);
  } else if (private_key.IsEd25519()) {
    return ExportPublicKeyEdDsa(private_key);
  }
  return std::nullopt;
}

Signer CreateJwtSigner(crypto::keypair::PrivateKey private_key) {
  switch (EVP_PKEY_base_id(private_key.key())) {
    case EVP_PKEY_EC:
      return base::BindOnce(&SignJwtEs256, std::move(private_key));
    case EVP_PKEY_RSA:
      return base::BindOnce(&SignJwtRs256, std::move(private_key));
    case EVP_PKEY_ED25519:
      return base::BindOnce(&SignJwtEdDsa, std::move(private_key));
    default:
      return base::BindOnce(
          [](const std::string_view&) -> std::optional<std::vector<uint8_t>> {
            return std::nullopt;
          });
  }
}

std::optional<crypto::keypair::PublicKey> ImportPublicKey(const Jwk& jwk) {
  if (jwk.kty == "OKP" && jwk.crv == "Ed25519") {
    auto x_bytes = Base64UrlDecode(jwk.x);
    if (!x_bytes || x_bytes->size() != 32) {
      return std::nullopt;
    }
    return crypto::keypair::PublicKey::FromEd25519PublicKey(
        base::as_byte_span(*x_bytes).first<32>());
  } else if (jwk.kty == "EC" && jwk.crv == "P-256") {
    auto x_bytes = Base64UrlDecode(jwk.x);
    auto y_bytes = Base64UrlDecode(jwk.y);
    if (!x_bytes || !y_bytes || x_bytes->size() != 32 ||
        y_bytes->size() != 32) {
      return std::nullopt;
    }
    std::vector<uint8_t> point;
    point.reserve(1 + x_bytes->size() + y_bytes->size());
    point.push_back(0x04);
    point.insert(point.end(), x_bytes->begin(), x_bytes->end());
    point.insert(point.end(), y_bytes->begin(), y_bytes->end());

    return crypto::keypair::PublicKey::FromEcP256Point(point);
  } else if (jwk.kty == "RSA") {
    auto n_bytes = Base64UrlDecode(jwk.n);
    auto e_bytes = Base64UrlDecode(jwk.e);
    if (!n_bytes || !e_bytes) {
      return std::nullopt;
    }
    return crypto::keypair::PublicKey::FromRsaPublicKeyComponents(
        base::as_byte_span(*n_bytes), base::as_byte_span(*e_bytes));
  }

  return std::nullopt;
}

bool VerifyJwt(const Jwk& jwk,
               const Header& header,
               const std::string_view& data,
               base::span<const uint8_t> signature) {
  auto public_key = ImportPublicKey(jwk);
  if (!public_key) {
    return false;
  }

  crypto::sign::SignatureKind kind;
  if (jwk.kty == "OKP" && jwk.crv == "Ed25519" && header.alg == "EdDSA") {
    kind = crypto::sign::SignatureKind::ED25519;
  } else if (jwk.kty == "EC" && jwk.crv == "P-256" && header.alg == "ES256") {
    kind = crypto::sign::SignatureKind::ECDSA_SHA256;
  } else if (jwk.kty == "RSA" && header.alg == "RS256") {
    kind = crypto::sign::SignatureKind::RSA_PKCS1_SHA256;
  } else {
    return false;
  }

  if (kind == crypto::sign::SignatureKind::ED25519 ||
      kind == crypto::sign::SignatureKind::RSA_PKCS1_SHA256) {
    return crypto::sign::Verify(kind, *public_key, base::as_byte_span(data),
                                signature);
  }

  // ES256 requires an exception because JWS signature format is raw
  // concatenated r and s (64 bytes), but BoringSSL expects DER encoded ASN.1
  // structure.
  if (signature.size() != 64u) {
    return false;
  }
  bssl::UniquePtr<BIGNUM> r(BN_bin2bn(signature.data(), 32, nullptr));
  bssl::UniquePtr<BIGNUM> s(
      BN_bin2bn(signature.subspan(32u).data(), 32, nullptr));
  if (!r || !s) {
    return false;
  }

  bssl::UniquePtr<ECDSA_SIG> ecdsa_sig(ECDSA_SIG_new());
  if (!ecdsa_sig ||
      !ECDSA_SIG_set0(ecdsa_sig.get(), r.release(), s.release())) {
    return false;
  }
  std::vector<uint8_t> sig_vec;
  const int len = i2d_ECDSA_SIG(ecdsa_sig.get(), nullptr);
  if (len <= 0) {
    return false;
  }
  sig_vec.resize(len);
  uint8_t* ptr = sig_vec.data();
  if (i2d_ECDSA_SIG(ecdsa_sig.get(), &ptr) <= 0) {
    return false;
  }
  return crypto::sign::Verify(kind, *public_key, base::as_byte_span(data),
                              sig_vec);
}

Verifier CreateJwtVerifier(const Jwk& jwk, const Header& header) {
  return base::BindOnce(&VerifyJwt, jwk, header);
}

}  // namespace content::sdjwt
