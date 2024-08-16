// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_USER_VERIFYING_KEY_H_
#define CRYPTO_USER_VERIFYING_KEY_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"
#include "crypto/scoped_lacontext.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"

namespace crypto {

typedef std::string UserVerifyingKeyLabel;

// Error values supplied to the callbacks for creating and retrieving
// user-verifying keys, upon failure.
enum class UserVerifyingKeyCreationError {
  kPlatformApiError = 0,
  kDuplicateCredential = 1,
  kNotFound = 2,
  kUserCancellation = 3,
  kNoMatchingAlgorithm = 4,
  kUnknownError = 5,
};

// Error values supplied to the callback for signing with a user-verifying key,
// upon failure.
enum class UserVerifyingKeySigningError {
  kPlatformApiError = 0,
  kUserCancellation = 1,
  kUnknownError = 2,
};

// UserVerifyingSigningKey is a hardware-backed key that triggers a user
// verification by the platform before a signature will be provided.
//
// Notes:
// - This is currently only supported on Windows and Mac.
// - This does not export a wrapped key because the Windows implementation uses
//   the WinRT KeyCredentialManager which addresses stored keys by name.
// - The interface for this class will likely need to be generalized as support
//   for other platforms is added.
class CRYPTO_EXPORT UserVerifyingSigningKey {
 public:
  virtual ~UserVerifyingSigningKey();
  using UserVerifyingKeySignatureCallback = base::OnceCallback<void(
      base::expected<std::vector<uint8_t>, UserVerifyingKeySigningError>)>;

  // Sign invokes |callback| to provide a signature of |data|, or |nullopt| if
  // an error occurs during signing.
  virtual void Sign(base::span<const uint8_t> data,
                    UserVerifyingKeySignatureCallback callback) = 0;

  // Provides the SPKI public key.
  virtual std::vector<uint8_t> GetPublicKey() const = 0;

  // Get a reference to the label used to create or retrieve this key.
  virtual const UserVerifyingKeyLabel& GetKeyLabel() const = 0;

  // Returns true if the underlying key is stored in "hardware". Something like
  // ARM TrustZone would count as hardware for these purposes.
  virtual bool IsHardwareBacked() const;
};

// Reference-counted wrapper for UserVeriyingSigningKey.
class CRYPTO_EXPORT RefCountedUserVerifyingSigningKey
    : public base::RefCountedThreadSafe<RefCountedUserVerifyingSigningKey> {
 public:
  explicit RefCountedUserVerifyingSigningKey(
      std::unique_ptr<crypto::UserVerifyingSigningKey> key);

  RefCountedUserVerifyingSigningKey(const RefCountedUserVerifyingSigningKey&) =
      delete;
  RefCountedUserVerifyingSigningKey& operator=(
      const RefCountedUserVerifyingSigningKey&) = delete;

  crypto::UserVerifyingSigningKey& key() const { return *key_; }

 private:
  friend class base::RefCountedThreadSafe<RefCountedUserVerifyingSigningKey>;
  ~RefCountedUserVerifyingSigningKey();

  const std::unique_ptr<crypto::UserVerifyingSigningKey> key_;
};

// UserVerifyingKeyProvider creates |UserVerifyingSigningKey|s.
class CRYPTO_EXPORT UserVerifyingKeyProvider {
 public:
  struct CRYPTO_EXPORT Config {
    Config();
    Config(const Config& config) = delete;
    Config& operator=(const Config& config) = delete;
    Config(Config&& config);
    Config& operator=(Config&& config);
    ~Config();

#if BUILDFLAG(IS_MAC)
    // The keychain access group the key is shared with. The binary must be
    // codesigned with the corresponding entitlement.
    // https://developer.apple.com/documentation/bundleresources/entitlements/keychain-access-groups?language=objc
    // This must be set to a non empty value when using user verifying keys on
    // macOS.
    std::string keychain_access_group;

    // Optional LAContext to be used when retrieving and storing keys. Passing
    // an authenticated LAContext lets you call UserVerifyingSigningKey::Sign()
    // without triggering a macOS local authentication prompt.
    std::optional<ScopedLAContext> lacontext;
#endif  // BUILDFLAG(IS_MAC)
  };

  using UserVerifyingKeyCreationCallback = base::OnceCallback<void(
      base::expected<std::unique_ptr<UserVerifyingSigningKey>,
                     UserVerifyingKeyCreationError>)>;

  virtual ~UserVerifyingKeyProvider();

  // Similar to |GenerateSigningKeySlowly| but the resulting signing key can
  // only be used with a local user authentication by the platform. This can be
  // called from any thread as the work is done asynchronously on a
  // high-priority thread when the underlying platform is slow.
  // Invokes |callback| with the resulting key, or nullptr on error.
  virtual void GenerateUserVerifyingSigningKey(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      UserVerifyingKeyCreationCallback callback) = 0;

  // Similar to |FromWrappedSigningKey| but uses a wrapped key that was
  // generated from |GenerateUserVerifyingSigningKey|. This can be called from
  // any thread as the work is done asynchronously on a high-priority thread
  // when the underlying platform is slow.
  // Invokes |callback| with the resulting key, or nullptr on error.
  virtual void GetUserVerifyingSigningKey(
      UserVerifyingKeyLabel key_label,
      UserVerifyingKeyCreationCallback callback) = 0;

  // Deletes a user verifying signing key. Work is be done asynchronously on a
  // low-priority thread when the underlying platform is slow.
  // Invokes |callback| with `true` if the key was found and deleted, `false`
  // otherwise.
  virtual void DeleteUserVerifyingKey(
      UserVerifyingKeyLabel key_label,
      base::OnceCallback<void(bool)> callback) = 0;
};

// GetUserVerifyingKeyProvider returns |UserVerifyingKeyProvider| for the
// current platform, or nullptr if this is not implemented on the current
// platform.
// Note that this will return non null if keys are supported but not available,
// i.e. if |AreUserVerifyingKeysSupported| returns false. In that case,
// operations would fail.
CRYPTO_EXPORT std::unique_ptr<UserVerifyingKeyProvider>
GetUserVerifyingKeyProvider(UserVerifyingKeyProvider::Config config);

// Invokes the callback with true if UV keys can be used on the current
// platform, and false otherwise. `callback` can be invoked synchronously or
// asynchronously.
CRYPTO_EXPORT void AreUserVerifyingKeysSupported(
    UserVerifyingKeyProvider::Config config,
    base::OnceCallback<void(bool)> callback);

namespace internal {

CRYPTO_EXPORT void SetUserVerifyingKeyProviderForTesting(
    std::unique_ptr<UserVerifyingKeyProvider> (*func)());

}  // namespace internal

}  // namespace crypto

#endif  // CRYPTO_USER_VERIFYING_KEY_H_
