// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_
#define CRYPTO_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace crypto {

class MockUnexportableKeyProvider : public StatefulUnexportableKeyProvider {
 public:
  MockUnexportableKeyProvider();
  ~MockUnexportableKeyProvider() override;

  // UnexportableKeyProvider:
  MOCK_METHOD(std::optional<SignatureVerifier::SignatureAlgorithm>,
              SelectAlgorithm,
              (base::span<const SignatureVerifier::SignatureAlgorithm>
                   acceptable_algorithms),
              (override));
  MOCK_METHOD(std::unique_ptr<UnexportableSigningKey>,
              GenerateSigningKeySlowly,
              (base::span<const SignatureVerifier::SignatureAlgorithm>
                   acceptable_algorithms),
              (override));
  MOCK_METHOD(std::unique_ptr<UnexportableSigningKey>,
              FromWrappedSigningKeySlowly,
              (base::span<const uint8_t> wrapped_key),
              (override));
  MOCK_METHOD(std::unique_ptr<UnexportableAttestationKey>,
              GenerateAttestationKeySlowly,
              (base::span<const SignatureVerifier::SignatureAlgorithm>
                   acceptable_algorithms),
              (override));
  MOCK_METHOD(std::unique_ptr<UnexportableAttestationKey>,
              FromWrappedAttestationKeySlowly,
              (base::span<const uint8_t> wrapped_key),
              (override));
  MOCK_METHOD(StatefulUnexportableKeyProvider*,
              AsStatefulUnexportableKeyProvider,
              (),
              (override));

  // StatefulUnexportableKeyProvider:
  MOCK_METHOD(
      std::optional<std::vector<std::unique_ptr<UnexportableSigningKey>>>,
      GetAllKeysSlowly,
      (),
      (override));
  MOCK_METHOD(std::optional<size_t>,
              DeleteWrappedKeysSlowly,
              (base::span<const base::span<const uint8_t>> wrapped_keys),
              (override));
  MOCK_METHOD(std::optional<size_t>,
              DeleteKeysSlowly,
              (base::span<const crypto::UnexportableKey* const> keys),
              (override));
  MOCK_METHOD(std::optional<size_t>, DeleteAllKeysSlowly, (), (override));
};

}  // namespace crypto

#endif  // CRYPTO_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_
