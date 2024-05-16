// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/obj.h"

#if BUILDFLAG(IS_MAC)
#include "base/notreached.h"
#endif  // BUILDFLAG(IS_MAC)

namespace crypto {

namespace {

std::vector<uint8_t> CBBToVector(const CBB* cbb) {
  return std::vector<uint8_t>(CBB_data(cbb), CBB_data(cbb) + CBB_len(cbb));
}

class SoftwareECDSA : public UnexportableSigningKey {
 public:
  explicit SoftwareECDSA(bssl::UniquePtr<EC_KEY> key) : key_(std::move(key)) {}
  ~SoftwareECDSA() override = default;

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    return SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
    CHECK(EVP_PKEY_set1_EC_KEY(pkey.get(), key_.get()));

    bssl::ScopedCBB cbb;
    CHECK(CBB_init(cbb.get(), /*initial_capacity=*/128) &&
          EVP_marshal_public_key(cbb.get(), pkey.get()));
    return CBBToVector(cbb.get());
  }

  std::vector<uint8_t> GetWrappedKey() const override {
    bssl::ScopedCBB cbb;
    CHECK(
        CBB_init(cbb.get(), /*initial_capacity=*/128) &&
        EC_KEY_marshal_private_key(cbb.get(), key_.get(),
                                   EC_PKEY_NO_PARAMETERS | EC_PKEY_NO_PUBKEY));
    return CBBToVector(cbb.get());
  }

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    std::vector<uint8_t> ret(ECDSA_size(key_.get()));
    std::array<uint8_t, kSHA256Length> digest = SHA256Hash(data);
    unsigned int ret_size;
    CHECK(ECDSA_sign(0, digest.data(), digest.size(), ret.data(), &ret_size,
                     key_.get()));
    ret.resize(ret_size);
    return ret;
  }

#if BUILDFLAG(IS_MAC)
  SecKeyRef GetSecKeyRef() const override {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
#endif  // BUILDFLAG(IS_MAC)

 private:
  bssl::UniquePtr<EC_KEY> key_;
};

class SoftwareProvider : public UnexportableKeyProvider {
 public:
  ~SoftwareProvider() override = default;

  std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    for (auto algo : acceptable_algorithms) {
      if (algo == SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256) {
        return algo;
      }
    }

    return std::nullopt;
  }

  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    if (!SelectAlgorithm(acceptable_algorithms)) {
      return nullptr;
    }

    bssl::UniquePtr<EC_KEY> key(EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
    CHECK(EC_KEY_generate_key(key.get()));

    return std::make_unique<SoftwareECDSA>(std::move(key));
  }

  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    CBS cbs;
    CBS_init(&cbs, wrapped_key.data(), wrapped_key.size());
    bssl::UniquePtr<EC_KEY> key(EC_KEY_parse_private_key(&cbs, p256.get()));
    if (!key || CBS_len(&cbs) != 0) {
      return nullptr;
    }
    return std::make_unique<SoftwareECDSA>(std::move(key));
  }

  bool DeleteSigningKeySlowly(base::span<const uint8_t> wrapped_key) override {
    // Unexportable software keys are stateless.
    return true;
  }
};

}  // namespace

std::unique_ptr<UnexportableKeyProvider>
GetSoftwareUnsecureUnexportableKeyProvider() {
  return std::make_unique<SoftwareProvider>();
}

}  // namespace crypto
