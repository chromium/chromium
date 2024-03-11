// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/scoped_fake_user_verifying_key_provider.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "crypto/user_verifying_key.h"

namespace crypto {

namespace {

// Simulate the storing of UV keys by the platform.
// TODO(enclave): This possibly can be replaced when the
// UserVerifyingKeyProvider is modified to support vending of labels, rather
// than the caller having to supply it. When that is implemented, this fake
// should be able to store the wrapped software key directly in the label.
base::flat_map<UserVerifyingKeyLabel, std::vector<uint8_t>> stored_uv_keys_;

// Wraps a software `UnexportableSigningKey`.
class FakeUserVerifyingSigningKey : public UserVerifyingSigningKey {
 public:
  FakeUserVerifyingSigningKey(
      UserVerifyingKeyLabel label,
      std::unique_ptr<UnexportableSigningKey> software_key)
      : label_(std::move(label)), software_key_(std::move(software_key)) {}

  ~FakeUserVerifyingSigningKey() override = default;

  void Sign(base::span<const uint8_t> data,
            base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
                callback) override {
    std::move(callback).Run(software_key_->SignSlowly(data));
  }

  std::vector<uint8_t> GetPublicKey() const override {
    return software_key_->GetSubjectPublicKeyInfo();
  }

  const UserVerifyingKeyLabel& GetKeyLabel() const override { return label_; }

 private:
  const UserVerifyingKeyLabel label_;
  std::unique_ptr<UnexportableSigningKey> software_key_;
};

class FakeUserVerifyingKeyProvider : public UserVerifyingKeyProvider {
 public:
  ~FakeUserVerifyingKeyProvider() override = default;

  void GenerateUserVerifyingSigningKey(
      UserVerifyingKeyLabel key_label,
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    auto software_unexportable_key =
        GetSoftwareUnsecureUnexportableKeyProvider()->GenerateSigningKeySlowly(
            acceptable_algorithms);
    stored_uv_keys_.insert_or_assign(
        key_label, software_unexportable_key->GetWrappedKey());
    std::move(callback).Run(std::make_unique<FakeUserVerifyingSigningKey>(
        std::move(key_label), std::move(software_unexportable_key)));
  }

  void GetUserVerifyingSigningKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    std::vector<SignatureVerifier::SignatureAlgorithm> algorithms = {
        SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256};
    auto it = stored_uv_keys_.find(key_label);
    if (it == stored_uv_keys_.end()) {
      std::move(callback).Run(nullptr);
      return;
    }
    auto software_unexportable_key =
        GetSoftwareUnsecureUnexportableKeyProvider()
            ->FromWrappedSigningKeySlowly(it->second);
    CHECK(software_unexportable_key);
    std::move(callback).Run(std::make_unique<FakeUserVerifyingSigningKey>(
        std::move(key_label), std::move(software_unexportable_key)));
  }
};

std::unique_ptr<UserVerifyingKeyProvider> GetMockUserVerifyingKeyProvider() {
  return std::make_unique<FakeUserVerifyingKeyProvider>();
}

std::unique_ptr<UserVerifyingKeyProvider> GetNullUserVerifyingKeyProvider() {
  return nullptr;
}

}  // namespace

ScopedFakeUserVerifyingKeyProvider::ScopedFakeUserVerifyingKeyProvider() {
  internal::SetUserVerifyingKeyProviderForTesting(
      GetMockUserVerifyingKeyProvider);
}

ScopedFakeUserVerifyingKeyProvider::~ScopedFakeUserVerifyingKeyProvider() {
  internal::SetUserVerifyingKeyProviderForTesting(nullptr);
}

ScopedNullUserVerifyingKeyProvider::ScopedNullUserVerifyingKeyProvider() {
  internal::SetUserVerifyingKeyProviderForTesting(
      GetNullUserVerifyingKeyProvider);
}

ScopedNullUserVerifyingKeyProvider::~ScopedNullUserVerifyingKeyProvider() {
  internal::SetUserVerifyingKeyProviderForTesting(nullptr);
}

}  // namespace crypto
