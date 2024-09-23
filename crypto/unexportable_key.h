// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_UNEXPORTABLE_KEY_H_
#define CRYPTO_UNEXPORTABLE_KEY_H_

#include <memory>
#include <optional>

#include "build/build_config.h"
#include "crypto/crypto_export.h"
#include "crypto/signature_verifier.h"

#if BUILDFLAG(IS_MAC)
#import <Security/Security.h>
#endif  // BUILDFLAG(IS_MAC)

namespace crypto {

// UnexportableSigningKey provides a hardware-backed signing oracle on platforms
// that support it. Current support is:
//   Windows: RSA_PKCS1_SHA256 via TPM 1.2+ and ECDSA_SHA256 via TPM 2.0.
//   macOS: ECDSA_SHA256 via the Secure Enclave.
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

  // GetWrappedKey returns a handle to the private key of this object. Usually,
  // it is the private key encrypted to a key that is kept in hardware and the
  // unencrypted private key never exists in the CPU's memory, hence the name.
  // On Mac, this is instead a hash of the public key and the wrapped key
  // material is stored in the Keychain.
  //
  // A key handle may be used with a future instance of this code to recreate
  // the key so long as it's running on the same computer.
  //
  // Note: on Windows it is possible to export this wrapped key off machine, but
  // it must be sealed with an AEAD first. The wrapped key may contain machine
  // identifiers and other values that you wouldn't want to export. Additionally
  // |UnexportableKeyProvider::FromWrappedSigningKey| should not be presented
  // attacked-controlled input and the AEAD would serve to authenticate the
  // wrapped key.
  virtual std::vector<uint8_t> GetWrappedKey() const = 0;

  // SignSlowly returns a signature of |data|, or |nullopt| if an error occurs
  // during signing.
  //
  // Note: this may take a second or more to run.
  virtual std::optional<std::vector<uint8_t>> SignSlowly(
      base::span<const uint8_t> data) = 0;

  // Returns true if the underlying key is stored in "hardware". Something like
  // ARM TrustZone would count as hardware for these purposes. Ideally all
  // implementations of this class would return true here, because software
  // implementations aren't really "unexportable", but a software implementation
  // does exist.
  virtual bool IsHardwareBacked() const;

#if BUILDFLAG(IS_MAC)
  // Returns the underlying reference to a Keychain key owned by the current
  // instance.
  virtual SecKeyRef GetSecKeyRef() const = 0;
#endif  // BUILDFLAG(IS_MAC)
};

// UnexportableKeyProvider creates |UnexportableSigningKey|s.
class CRYPTO_EXPORT UnexportableKeyProvider {
 public:
  virtual ~UnexportableKeyProvider();

  // Platform-specific configuration parameters for the provider.
  struct Config {
#if BUILDFLAG(IS_MAC)
    // Determines the level of user verification needed to sign with the key.
    // https://developer.apple.com/documentation/security/secaccesscontrolcreateflags?language=objc
    enum class AccessControl {
      // No access control. User presence is not required to access this secret.
      kNone,

      // Either biometry or the local account password are required to access
      // this secret. This is equivalent to kSecAccessControlUserPresence.
      // Note that if you set this and choose not to pass an authenticated
      // LAContext when signing, macOS will prompt the user for biometrics and
      // the thread will block until that resolves.
      kUserPresence,
    };

    // The keychain access group the key is shared with. The binary must be
    // codesigned with the corresponding entitlement.
    // https://developer.apple.com/documentation/bundleresources/entitlements/keychain-access-groups?language=objc
    // This must be set to a non empty value when using unexportable keys on
    // macOS.
    std::string keychain_access_group;

    // An optional application tag that will be set for all keys created by this
    // provider. If non empty, this should uniquely identify a group of related
    // keys, and can be used to query or delete all credentials with the same
    // tag.
    // https://developer.apple.com/documentation/security/ksecattrapplicationtag?language=objc
    std::string application_tag;

    // The access control set for keys created by the provider.
    AccessControl access_control = AccessControl::kNone;
#endif  // BUILDFLAG(IS_MAC)
  };

  // SelectAlgorithm returns which signature algorithm from
  // |acceptable_algorithms| would be used if |acceptable_algorithms| was passed
  // to |GenerateSigningKeySlowly|.
  virtual std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
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

  // Unexportable key implementations may be stateful. This is the case for
  // macOS. |DeleteSigningKey| deletes all state associated with a given signing
  // key on such implementations. For stateless implementations, this is a
  // no-op.
  // Returns true on successful deletion, false otherwise.
  // This can sometimes block, and therefore must not be called from the UI
  // thread.
  virtual bool DeleteSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) = 0;
};

// This is an experimental API as it uses an unofficial Windows API.
// The current implementation is here to gather metrics only. It should not be
// used outside of metrics gathering without knowledge of crypto OWNERS.
//
// UnexportableSigningKey provides a software-backed signing oracle based in a
// specialized virtual machine on platforms that support it. Current support is:
//   Windows: RSA_PKCS1_SHA256 and ECDSA_SHA256.
//
// These keys differs from UnexportableSigningKey in several ways:
// - They are backed not by hardware, but by a specialized limited virtual
// machine resistant to attacks.
// - The latency of operations are expected to be about 100 times less, making
// them much more practical in cases that would otherwise disrupt the user
// experience.
// - The keys are stored in the virtual machine by name, this namespace is
// shared by all applications and there is a limited number of available keys
// (~65k from testing).
//
// For more info see:
// https://learn.microsoft.com/en-us/windows/security/identity-protection/credential-guard/credential-guard
class CRYPTO_EXPORT VirtualUnexportableSigningKey {
 public:
  virtual ~VirtualUnexportableSigningKey();

  // Algorithm returns the algorithm of the key in this object.
  virtual SignatureVerifier::SignatureAlgorithm Algorithm() const = 0;

  // GetSubjectPublicKeyInfo returns an SPKI that contains the public key of
  // this object.
  virtual std::vector<uint8_t> GetSubjectPublicKeyInfo() const = 0;

  // GetKeyName may be used with a future instance of this code to recreate
  // the key so long as it's running on the same computer.
  //
  // Note: All local applications can enumerate all keys on device and
  // recreate them. Private keys can also be exported with the first HANDLE
  // after creation.
  virtual std::string GetKeyName() const = 0;

  // Sign returns a signature of |data|, or |nullopt| if an error occurs
  // during signing.
  //
  // Note: this is expected to be under 10ms.
  virtual std::optional<std::vector<uint8_t>> Sign(
      base::span<const uint8_t> data) = 0;

  // Deletes the key from storage in the virtual machine. As the virtual machine
  // has limited storage shared by all applications it is important to delete
  // keys no longer in use.
  virtual void DeleteKey() = 0;
};

// VirtualUnexportableKeyProvider creates |VirtualUnexportableSigningKey|s.
class CRYPTO_EXPORT VirtualUnexportableKeyProvider {
 public:
  virtual ~VirtualUnexportableKeyProvider();

  // SelectAlgorithm returns which signature algorithm from
  // |acceptable_algorithms| would be used if |acceptable_algorithms| was passed
  // to |GenerateSigningKeySlowly|.
  virtual std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) = 0;

  // GenerateSigningKey creates a new opaque signing key in a virtual machine.
  // The first supported value of |acceptable_algorithms| determines the type of
  // the key. Returns nullptr if it is not supported in the operating system,
  // if no value in |acceptable_algorithms| is supported, or if there was an
  // error creating the key.
  // As the namespace is shared between all applications care should be taken to
  // use a name that will not already be used by other applications. If a new
  // key is created with the same name as a current key the creation will fail.
  // Do not create a key with NULL or empty string as the name.
  //
  // Note: This may take milliseconds to run.
  virtual std::unique_ptr<VirtualUnexportableSigningKey> GenerateSigningKey(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      std::string name) = 0;

  // FromKeyName creates an |UnexportableSigningKey| from |name|, which is the
  // name used to create the key. Returns nullptr if |name| cannot be imported.
  //
  // Note: This may take milliseconds to run.
  virtual std::unique_ptr<VirtualUnexportableSigningKey> FromKeyName(
      std::string name) = 0;
};

// GetUnexportableKeyProvider returns an |UnexportableKeyProvider|
// for the current platform, or nullptr if there isn't one. This can be called
// from any thread but, in tests, but be sequenced with
// |SetUnexportableSigningKeyProvider|.
CRYPTO_EXPORT std::unique_ptr<UnexportableKeyProvider>
GetUnexportableKeyProvider(UnexportableKeyProvider::Config config);

// GetVirtualUnexportableKeyProvider_DO_NOT_USE_METRICS_ONLY returns a
// |VirtualUnexportableKeyProvider| for the current platform, or nullptr if
// there isn't one. This should currently only be used for metrics gathering.
CRYPTO_EXPORT std::unique_ptr<VirtualUnexportableKeyProvider>
GetVirtualUnexportableKeyProvider_DO_NOT_USE_METRICS_ONLY();

// `GetSoftwareUnsecureUnexportableKeyProvider()` returns a mock software
// implementation of `UnexportableKeyProvider` that can be used on platforms
// that do not have a native secure implementation.
// This should be used for development purposes only since these keys are not
// backed by hardware and are not stored securely.
CRYPTO_EXPORT std::unique_ptr<UnexportableKeyProvider>
GetSoftwareUnsecureUnexportableKeyProvider();

namespace internal {

CRYPTO_EXPORT bool HasScopedUnexportableKeyProvider();

CRYPTO_EXPORT void SetUnexportableKeyProviderForTesting(
    std::unique_ptr<UnexportableKeyProvider> (*func)());

}  // namespace internal

}  // namespace crypto

#endif  // CRYPTO_UNEXPORTABLE_KEY_H_
