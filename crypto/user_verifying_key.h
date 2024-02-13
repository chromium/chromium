// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_USER_VERIFYING_KEY_H_
#define CRYPTO_USER_VERIFYING_KEY_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"
#include "crypto/signature_verifier.h"

namespace crypto {

// The type of the identifiers for user-verifying keys depends on the
// underlying platform API.
#if BUILDFLAG(IS_WIN)
typedef std::string UserVerifyingKeyLabel;
#else
typedef int UserVerifyingKeyLabel;  // Unused.
#endif

// UserVerifyingSigningKey is a hardware-backed key that triggers a user
// verification by the platform before a signature will be provided.
//
// Notes:
// - This is currently only supported on Windows.
// - This does not export a wrapped key because it uses the WinRT
//   KeyCredentialManager which addresses stored keys by name.
// - The interface for this class will likely need to be generalized as support
//   for other platforms is added.
class CRYPTO_EXPORT UserVerifyingSigningKey {
 public:
  virtual ~UserVerifyingSigningKey();

  // Sign invokes |callback| to provide a signature of |data|, or |nullopt| if
  // an error occurs during signing.
  virtual void Sign(
      base::span<const uint8_t> data,
      base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
          callback) = 0;

  // Provides the SPKI public key.
  virtual std::vector<uint8_t> GetPublicKey() const = 0;

  // Get a reference to the label used to create or retrieve this key.
  virtual const UserVerifyingKeyLabel& GetKeyLabel() const = 0;
};

// UserVerifyingKeyProvider creates |UserVerifyingSigningKey|s.
// Only one call to |GenerateUserVerifyingSigningKey| or
// |GetUserVerifyingSigningKey| can be outstanding at one time for a single
// provider, but multiple providers can be used. Destroying a provider will
// cancel an outstanding key generation or retrieval and delete the callback
// without running it.
class CRYPTO_EXPORT UserVerifyingKeyProvider {
 public:
  virtual ~UserVerifyingKeyProvider();

  // Similar to |GenerateSigningKeySlowly| but the resulting signing key can
  // only be used with a local user authentication by the platform. This can be
  // called from any thread as the work is done asynchronously on a
  // low-priority thread.
  // Invokes |callback| with the resulting key, or nullptr on error.
  //
  // This is currently only supported on Windows.
  virtual void GenerateUserVerifyingSigningKey(
      UserVerifyingKeyLabel key_label,
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) = 0;

  // Similar to |FromWrappedSigningKey| but uses a wrapped key that was
  // generated from |GenerateUserVerifyingSigningKey|. This can be called from
  // any thread as the work is done asynchronously on a low-priority thread.
  // Invokes |callback| with the resulting key, or nullptr on error.
  //
  // This is currently only supported on Windows.
  virtual void GetUserVerifyingSigningKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(std::unique_ptr<UserVerifyingSigningKey>)>
          callback) = 0;
};

// GetUserVerifyingKeyProvider returns |UserVerifyingKeyProvider| for the
// current platform, or nullptr if this is not implemented on the current
// platform.
CRYPTO_EXPORT std::unique_ptr<UserVerifyingKeyProvider>
GetUserVerifyingKeyProvider();

}  // namespace crypto

#endif  // CRYPTO_USER_VERIFYING_KEY_H_
