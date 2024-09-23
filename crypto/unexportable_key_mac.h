// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_UNEXPORTABLE_KEY_MAC_H_
#define CRYPTO_UNEXPORTABLE_KEY_MAC_H_

#include <memory>

#if defined(__OBJC__)
#import <LocalAuthentication/LocalAuthentication.h>
#endif  // defined(__OBJC__)

#include "crypto/unexportable_key.h"

namespace crypto {

// UserVerifyingKeyProviderMac is an implementation of the
// UserVerifyingKeyProvider interface on top of Apple's Secure Enclave. Callers
// must provide a keychain access group when instantiating this class. This
// means that the build must be codesigned for any of this to work.
// https://developer.apple.com/documentation/bundleresources/entitlements/keychain-access-groups?language=objc
//
// Only NIST P-256 elliptic curves are supported.
//
// Unlike Windows keys, macOS will store key metadata locally. Callers are
// responsible for deleting keys when they are no longer needed.
class UnexportableKeyProviderMac : public UnexportableKeyProvider {
 public:
  explicit UnexportableKeyProviderMac(Config config);
  ~UnexportableKeyProviderMac() override;

#if defined(__OBJC__)
  // Like UnexportableKeyProvider::FromWrappedSigningKeySlowly, but lets you
  // pass an authenticated LAContext to avoid having macOS prompt the user for
  // user verification.
  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key,
      LAContext* lacontext);

  // Like UnexportableKeyProvider::GenerateSigningKeySlowly, but lets you pass
  // an authenticated LAContext to avoid having macOS prompt the user for user
  // verification.
  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      LAContext* lacontext);
#endif  // defined(__OBJC__)

  // UnexportableKeyProvider:
  std::optional<SignatureVerifier::SignatureAlgorithm> SelectAlgorithm(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override;
  std::unique_ptr<UnexportableSigningKey> GenerateSigningKeySlowly(
      base::span<const SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms) override;
  std::unique_ptr<UnexportableSigningKey> FromWrappedSigningKeySlowly(
      base::span<const uint8_t> wrapped_key) override;
  bool DeleteSigningKeySlowly(base::span<const uint8_t> wrapped_key) override;

 private:
  struct ObjCStorage;
  const Config::AccessControl access_control_;
  std::unique_ptr<ObjCStorage> objc_storage_;
};

std::unique_ptr<UnexportableKeyProviderMac> GetUnexportableKeyProviderMac(
    UnexportableKeyProvider::Config config);

}  // namespace crypto

#endif  // CRYPTO_UNEXPORTABLE_KEY_MAC_H_
