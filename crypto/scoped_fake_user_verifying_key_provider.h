// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_FAKE_USER_VERIFYING_KEY_PROVIDER_H_
#define CRYPTO_SCOPED_FAKE_USER_VERIFYING_KEY_PROVIDER_H_

#include "crypto/user_verifying_key.h"

namespace crypto {

// `ScopedFakeUserVerifyingKeyProvider` causes `GetUserVerifyingKeyProvider` to
// return a mock implementation of `UserVerifyingKeyProvider`, that does not use
// system APIs, while it is in scope.
class ScopedFakeUserVerifyingKeyProvider {
 public:
  explicit ScopedFakeUserVerifyingKeyProvider(
      bool fake_hardware_backing = false);
  ~ScopedFakeUserVerifyingKeyProvider();
};

// `ScopedNullUserVerifyingKeyProvider` causes `GetUserVerifyingKeyProvider` to
// return a nullptr, emulating the key provider not being supported, while it
// is in scope.
class ScopedNullUserVerifyingKeyProvider {
 public:
  ScopedNullUserVerifyingKeyProvider();
  ~ScopedNullUserVerifyingKeyProvider();
};

// `ScopedFailingUserVerifyingKeyProvider` causes `GetUserVerifyingKeyProvider`
// to return a mock implementation of `UserVerifyingKeyProvider` that fails all
// signing requests.
class ScopedFailingUserVerifyingKeyProvider {
 public:
  ScopedFailingUserVerifyingKeyProvider();
  ~ScopedFailingUserVerifyingKeyProvider();
};

// `ScopedUserVerifyingKeysSupportedOverride` allows a client to control the
// timing and result of `AreUserVerifyingKeysSupported`. If this is in scope, it
// takes priority over any of the scoped providers above.
class ScopedUserVerifyingKeysSupportedOverride {
 public:
  explicit ScopedUserVerifyingKeysSupportedOverride(
      UserVerifyingKeysSupportedOverride override);
  ~ScopedUserVerifyingKeysSupportedOverride();
};

}  // namespace crypto

#endif  // CRYPTO_SCOPED_FAKE_USER_VERIFYING_KEY_PROVIDER_H_
