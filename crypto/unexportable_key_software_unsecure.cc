// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"

namespace crypto {

namespace {

class SoftwareKey : public UnexportableSigningKey {
 public:
  explicit SoftwareKey(crypto::keypair::PrivateKey key)
      : key_(std::move(key)) {}
  ~SoftwareKey() override = default;

  SignatureVerifier::SignatureAlgorithm Algorithm() const override {
    if (key_.IsRsa()) {
      return SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256;
    }
    if (key_.IsEcP256()) {
      return SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256;
    }
    NOTREACHED();
  }

  std::vector<uint8_t> GetSubjectPublicKeyInfo() const override {
    return key_.ToSubjectPublicKeyInfo();
  }

  std::vector<uint8_t> GetWrappedKey() const override {
    if (key_.IsRsa()) {
      return key_.ToRSAPrivateKey();
    }
    if (key_.IsEcP256()) {
      return key_.ToEcP256PrivateKey();
    }
    NOTREACHED();
  }

  std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) override {
    return sign::Sign(GetSignatureKind(), key_, data);
  }

#if BUILDFLAG(IS_APPLE)
  SecKeyRef GetSecKeyRef() const override { NOTREACHED(); }
#elif BUILDFLAG(IS_WIN)
  bool SupportsTls13() override { return true; }
#endif  // BUILDFLAG(IS_APPLE)

 private:
  sign::SignatureKind GetSignatureKind() const {
    if (key_.IsRsa()) {
      return sign::RSA_PKCS1_SHA256;
    }
    if (key_.IsEcP256()) {
      return sign::ECDSA_SHA256;
    }
    NOTREACHED();
  }

  crypto::keypair::PrivateKey key_;
};

class SoftwareProvider : public UnexportableKeyProvider {
 public:
  ~SoftwareProvider() override = default;

  std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    for (auto algo : acceptable_algorithms) {
      switch (algo) {
        case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
          return algo;
        case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
        case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
          continue;  // Not supported
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

    for (auto algo : acceptable_algorithms) {
      switch (algo) {
        case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256: {
          return std::make_unique<SoftwareKey>(
              crypto::keypair::PrivateKey::GenerateEcP256());
        }

        case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256: {
          return std::make_unique<SoftwareKey>(
              crypto::keypair::PrivateKey::GenerateRsa2048());
        }
        case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
        case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
          continue;  // Not supported
      }
    }

    return nullptr;
  }

  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    if (auto key =
            crypto::keypair::PrivateKey::FromEcP256PrivateKey(wrapped_key)) {
      return std::make_unique<SoftwareKey>(std::move(*key));
    }

    if (auto key =
            crypto::keypair::PrivateKey::FromRSAPrivateKey(wrapped_key)) {
      return std::make_unique<SoftwareKey>(std::move(*key));
    }

    return nullptr;
  }

  StatefulUnexportableKeyProvider* AsStatefulUnexportableKeyProvider()
      override {
    // Unexportable software keys are stateless.
    return nullptr;
  }
};

}  // namespace

std::unique_ptr<UnexportableKeyProvider>
GetSoftwareUnsecureUnexportableKeyProvider() {
  return std::make_unique<SoftwareProvider>();
}

}  // namespace crypto
