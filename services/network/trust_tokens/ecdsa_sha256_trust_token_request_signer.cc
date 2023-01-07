// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/ecdsa_sha256_trust_token_request_signer.h"

#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/signature_verifier.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace network {

EcdsaSha256TrustTokenRequestSigner::EcdsaSha256TrustTokenRequestSigner() =
    default;
EcdsaSha256TrustTokenRequestSigner::~EcdsaSha256TrustTokenRequestSigner() =
    default;

absl::optional<std::vector<uint8_t>> EcdsaSha256TrustTokenRequestSigner::Sign(
    base::span<const uint8_t> key,
    base::span<const uint8_t> data) {
  std::unique_ptr<crypto::ECPrivateKey> private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(key);
  if (!private_key)
    return absl::nullopt;

  EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(private_key->key());
  if (!ec_key)
    return absl::nullopt;

  if (EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key)) !=
      NID_X9_62_prime256v1)
    return absl::nullopt;

  std::unique_ptr<crypto::ECSignatureCreator> sig_creator =
      crypto::ECSignatureCreator::Create(private_key.get());

  std::vector<uint8_t> signature;

  if (!sig_creator->Sign(data, &signature))
    return absl::nullopt;

  return signature;
}

bool EcdsaSha256TrustTokenRequestSigner::Verify(
    base::span<const uint8_t> data,
    base::span<const uint8_t> signature,
    base::span<const uint8_t> verification_key) {
  // Require the public key be in uncompressed form. EC_POINT_oct2point
  // also accepts compressed form.
  if (verification_key.empty() ||
      verification_key[0] != POINT_CONVERSION_UNCOMPRESSED) {
    return false;
  }

  bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_POINT> pub_key(EC_POINT_new(EC_KEY_get0_group(key.get())));
  if (!EC_POINT_oct2point(EC_KEY_get0_group(key.get()), pub_key.get(),
                          verification_key.data(), verification_key.size(),
                          nullptr) ||
      !EC_KEY_set_public_key(key.get(), pub_key.get())) {
    return false;
  }

  bssl::UniquePtr<EVP_PKEY> public_key(EVP_PKEY_new());
  if (!EVP_PKEY_set1_EC_KEY(public_key.get(), key.get())) {
    return false;
  }

  bssl::ScopedEVP_MD_CTX ctx;
  EVP_PKEY_CTX* pctx;
  if (!EVP_DigestVerifyInit(ctx.get(), &pctx, EVP_sha256(), nullptr,
                            public_key.get())) {
    return false;
  }

  return EVP_DigestVerify(ctx.get(), signature.data(), signature.size(),
                          data.data(), data.size());
}

std::string EcdsaSha256TrustTokenRequestSigner::GetAlgorithmIdentifier() const {
  return "ecdsa_secp256r1_sha256";
}

}  // namespace network
