// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/scoped_fake_user_verifying_key_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "crypto/user_verifying_key.h"

namespace crypto {

namespace {

// This tracks deleted keys, so calling `DeleteUserVerifyingKey` with one
// can return false, allowing deletion to be tested.
std::vector<UserVerifyingKeyLabel> g_deleted_keys_;

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
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    auto software_unexportable_key =
        GetSoftwareUnsecureUnexportableKeyProvider()->GenerateSigningKeySlowly(
            acceptable_algorithms);
    UserVerifyingKeyLabel key_label =
        base::Base64Encode(software_unexportable_key->GetWrappedKey());
    std::move(callback).Run(std::make_unique<FakeUserVerifyingSigningKey>(
        std::move(key_label), std::move(software_unexportable_key)));
  }

  void GetUserVerifyingSigningKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    for (auto deleted_key : g_deleted_keys_) {
      if (deleted_key == key_label) {
        std::move(callback).Run(nullptr);
        return;
      }
    }
    std::vector<SignatureVerifier::SignatureAlgorithm> algorithms = {
        SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256};
    std::optional<std::vector<uint8_t>> wrapped_key =
        base::Base64Decode(key_label);
    CHECK(wrapped_key);
    auto software_unexportable_key =
        GetSoftwareUnsecureUnexportableKeyProvider()
            ->FromWrappedSigningKeySlowly(*wrapped_key);
    CHECK(software_unexportable_key);
    std::move(callback).Run(std::make_unique<FakeUserVerifyingSigningKey>(
        std::move(key_label), std::move(software_unexportable_key)));
  }

  void DeleteUserVerifyingKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(bool)> callback) override {
    g_deleted_keys_.push_back(key_label);
    std::move(callback).Run(true);
  }
};

class FailingUserVerifyingSigningKey : public UserVerifyingSigningKey {
 public:
  FailingUserVerifyingSigningKey() : label_("test") {}
  ~FailingUserVerifyingSigningKey() override = default;

  void Sign(base::span<const uint8_t> data,
            base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
                callback) override {
    std::move(callback).Run(std::nullopt);
  }

  std::vector<uint8_t> GetPublicKey() const override { return {1, 2, 3, 4}; }

  const UserVerifyingKeyLabel& GetKeyLabel() const override { return label_; }

 private:
  const UserVerifyingKeyLabel label_;
};

class FailingUserVerifyingKeyProvider : public UserVerifyingKeyProvider {
 public:
  ~FailingUserVerifyingKeyProvider() override = default;

  void GenerateUserVerifyingSigningKey(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    std::move(callback).Run(std::make_unique<FailingUserVerifyingSigningKey>());
  }

  void GetUserVerifyingSigningKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) override {
    std::move(callback).Run(std::make_unique<FailingUserVerifyingSigningKey>());
  }

  void DeleteUserVerifyingKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(bool)> callback) override {}
};

std::unique_ptr<UserVerifyingKeyProvider> GetMockUserVerifyingKeyProvider() {
  return std::make_unique<FakeUserVerifyingKeyProvider>();
}

std::unique_ptr<UserVerifyingKeyProvider> GetNullUserVerifyingKeyProvider() {
  return nullptr;
}

std::unique_ptr<UserVerifyingKeyProvider> GetFailingUserVerifyingKeyProvider() {
  return std::make_unique<FailingUserVerifyingKeyProvider>();
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

ScopedFailingUserVerifyingKeyProvider::ScopedFailingUserVerifyingKeyProvider() {
  internal::SetUserVerifyingKeyProviderForTesting(
      GetFailingUserVerifyingKeyProvider);
}

ScopedFailingUserVerifyingKeyProvider::
    ~ScopedFailingUserVerifyingKeyProvider() {
  internal::SetUserVerifyingKeyProviderForTesting(nullptr);
}
}  // namespace crypto
