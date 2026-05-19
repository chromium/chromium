// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/scoped_mock_unexportable_key_provider.h"

#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "crypto/mock_unexportable_key.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace crypto {

namespace {

ScopedMockUnexportableKeyProvider* g_mock_provider = nullptr;

class ForwardingUnexportableKeyProvider : public UnexportableKeyProvider {
 public:
  explicit ForwardingUnexportableKeyProvider(UnexportableKeyProvider& provider)
      : provider_(provider) {}
  ~ForwardingUnexportableKeyProvider() override = default;

  // UnexportableKeyProvider:
  std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    return provider_->SelectAlgorithm(acceptable_algorithms);
  }

  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    return provider_->GenerateSigningKeySlowly(acceptable_algorithms);
  }

  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    return provider_->FromWrappedSigningKeySlowly(wrapped_key);
  }

  std::unique_ptr<UnexportableAttestationKey> GenerateAttestationKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override {
    return provider_->GenerateAttestationKeySlowly(acceptable_algorithms);
  }

  std::unique_ptr<UnexportableAttestationKey> FromWrappedAttestationKeySlowly(
      base::span<const uint8_t> wrapped_key) override {
    return provider_->FromWrappedAttestationKeySlowly(wrapped_key);
  }

  StatefulUnexportableKeyProvider* AsStatefulUnexportableKeyProvider()
      override {
    return provider_->AsStatefulUnexportableKeyProvider();
  }

 private:
  const raw_ref<crypto::UnexportableKeyProvider> provider_;
};

std::unique_ptr<UnexportableKeyProvider> GetMockKeyProvider() {
  return std::make_unique<ForwardingUnexportableKeyProvider>(
      g_mock_provider->mock());
}

// Tries to pop the next key from the queue. Returns `nullptr` if the queue
// is empty.
template <typename KeyT>
std::unique_ptr<KeyT> TryPopNextKey(base::queue<std::unique_ptr<KeyT>>& keys) {
  std::unique_ptr<KeyT> key;
  if (!keys.empty()) {
    key = std::move(keys.front());
    keys.pop();
  }
  return key;
}

}  // namespace

ScopedMockUnexportableKeyProvider::ScopedMockUnexportableKeyProvider() {
  CHECK(!g_mock_provider) << "Nested providers are not allowed";
  // Store `this` in a global pointer so that all mock key providers can access
  // the `next_generated_keys_` queue.
  g_mock_provider = this;
  crypto::internal::SetUnexportableKeyProviderForTesting(&GetMockKeyProvider);

  ON_CALL(mock_provider_, SelectAlgorithm)
      .WillByDefault([](base::span<const SignatureVerifier::SignatureAlgorithm>
                            algorithms) {
        return algorithms.empty() ? std::nullopt : std::optional(algorithms[0]);
      });
  ON_CALL(mock_provider_, GenerateSigningKeySlowly).WillByDefault([this](auto) {
    return TryPopNextKey(next_generated_signing_keys_);
  });
  ON_CALL(mock_provider_, FromWrappedSigningKeySlowly)
      .WillByDefault(
          [this](auto) { return TryPopNextKey(next_generated_signing_keys_); });
  ON_CALL(mock_provider_, GenerateAttestationKeySlowly)
      .WillByDefault([this](auto) {
        return TryPopNextKey(next_generated_attestation_keys_);
      });
  ON_CALL(mock_provider_, FromWrappedAttestationKeySlowly)
      .WillByDefault([this](auto) {
        return TryPopNextKey(next_generated_attestation_keys_);
      });
}

ScopedMockUnexportableKeyProvider::~ScopedMockUnexportableKeyProvider() {
  crypto::internal::SetUnexportableKeyProviderForTesting(nullptr);
  g_mock_provider = nullptr;
}

UnexportableSigningKey*
ScopedMockUnexportableKeyProvider::AddNextGeneratedSigningKey(
    std::unique_ptr<UnexportableSigningKey> key) {
  return next_generated_signing_keys_.emplace(std::move(key)).get();
}

UnexportableAttestationKey*
ScopedMockUnexportableKeyProvider::AddNextGeneratedAttestationKey(
    std::unique_ptr<UnexportableAttestationKey> key) {
  return next_generated_attestation_keys_.emplace(std::move(key)).get();
}

}  // namespace crypto
