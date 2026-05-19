// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_
#define CRYPTO_SCOPED_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_

#include <memory>

#include "base/containers/queue.h"
#include "crypto/mock_unexportable_key_provider.h"
#include "crypto/unexportable_key.h"

namespace crypto {

// Causes `GetUnexportableKeyProvider()` to return fully mockable
// `MockUnexportableKey`s while it is in scope.
// The mock provider will return mock keys previously added via
// `AddNextGeneratedKey()` in the queue order.
class ScopedMockUnexportableKeyProvider {
 public:
  ScopedMockUnexportableKeyProvider();
  ScopedMockUnexportableKeyProvider(const ScopedMockUnexportableKeyProvider&) =
      delete;
  ScopedMockUnexportableKeyProvider& operator=(
      const ScopedMockUnexportableKeyProvider&) = delete;
  ~ScopedMockUnexportableKeyProvider();

  MockUnexportableKeyProvider& mock() { return mock_provider_; }

  UnexportableSigningKey* AddNextGeneratedSigningKey(
      std::unique_ptr<UnexportableSigningKey> key);

  UnexportableAttestationKey* AddNextGeneratedAttestationKey(
      std::unique_ptr<UnexportableAttestationKey> key);

 private:
  MockUnexportableKeyProvider mock_provider_;
  base::queue<std::unique_ptr<UnexportableSigningKey>>
      next_generated_signing_keys_;
  base::queue<std::unique_ptr<UnexportableAttestationKey>>
      next_generated_attestation_keys_;
};

}  // namespace crypto

#endif  // CRYPTO_SCOPED_MOCK_UNEXPORTABLE_KEY_PROVIDER_H_
