// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_UNEXPORTABLE_KEY_H_
#define CRYPTO_UNEXPORTABLE_KEY_H_

#include <memory>

#include "crypto/crypto_export.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crypto {

// UnexportableSigningKey provides a hardware-backed signing oracle on platforms
// that support it. Current support is:
//   Windows: RSA_PKCS1_SHA256 via TPM 1.2+ and ECDSA_SHA256 via TPM 2.0.
//   Tests: ECDSA_SHA256 via ScopedMockUnexportableSigningKeyForTesting.
//
// See also //components/unexportable_keys for a higher-level key management
// API.
class CRYPTO_EXPORT UnexportableSigningKey {
 public:
  virtual ~UnexportableSigningKey();

  // Algorithm returns the algorithm of the key in this object.
  virtual SignatureVerifier::SignatureAlgorithm Algorithm() const = 0;

  // GetSubjectPublicKeyInfo returns an SPKI that contains the public key of
  // this object.
  virtual std::vector<uint8_t> GetSubjectPublicKeyInfo() const = 0;

  // GetWrappedKey returns the encrypted private key of this object. It is
  // encrypted to a key that is kept in hardware and the unencrypted private
  // key never exists in the CPU's memory.
  //
  // A wrapped key may be used with a future instance of this code to recreate
  // the key so long as it's running on the same computer.
  //
  // Note: it is possible to export this wrapped key off machine, but it must be
  // sealed with an AEAD first. The wrapped key may contain machine identifiers
  // and other values that you wouldn't want to export. Additionally
  // |UnexportableKeyProvider::FromWrappedSigningKey| should not be presented
  // attacked-controlled input and the AEAD would serve to authenticate the
  // wrapped key.
  virtual std::vector<uint8_t> GetWrappedKey() const = 0;

  // SignSlowly returns a signature of |data|, or |nullopt| if an error occurs
  // during signing.
  //
  // Note: this may take a second or more to run.
  virtual absl::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) = 0;
};

// UnexportableKeyProvider creates |UnexportableSigningKey|s.
class CRYPTO_EXPORT UnexportableKeyProvider {
 public:
  virtual ~UnexportableKeyProvider();

  // SelectAlgorithm returns which signature algorithm from
  // |acceptable_algorithms| would be used if |acceptable_algorithms| was passed
  // to |GenerateSigningKeySlowly|.
  virtual absl::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) = 0;

  // GenerateSigningKeySlowly creates a new opaque signing key in hardware. The
  // first supported value of |acceptable_algorithms| determines the type of the
  // key. Returns nullptr if no supported hardware exists, if no value in
  // |acceptable_algorithms| is supported, or if there was an error creating the
  // key.
  //
  // Note: this may take one or two seconds to run.
  virtual std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) = 0;

  // FromWrappedSigningKey creates an |UnexportableSigningKey| from
  // |wrapped_key|, which must have resulted from calling |GetWrappedKey| on a
  // previous instance of |UnexportableSigningKey|. Returns nullptr if
  // |wrapped_key| cannot be imported.
  //
  // Note: this may take up to a second.
  //
  // Note: do not call this with attacker-controlled data. The underlying
  // interfaces to the secure hardware may not be robust. See |GetWrappedKey|.
  virtual std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) = 0;
};

// GetUnexportableKeyProvider returns an |UnexportableKeyProvider|
// for the current platform, or nullptr if there isn't one. This can be called
// from any thread but, in tests, but be sequenced with
// |SetUnexportableSigningKeyProvider|.
CRYPTO_EXPORT std::unique_ptr<UnexportableKeyProvider>
GetUnexportableKeyProvider();

namespace internal {

CRYPTO_EXPORT void SetUnexportableKeyProviderForTesting(
    std::unique_ptr<UnexportableKeyProvider> (*func)());

}  // namespace internal

}  // namespace crypto

#endif  // CRYPTO_UNEXPORTABLE_KEY_H_
