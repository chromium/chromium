// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_SCOPED_KEYCHAIN_USER_INTERACTION_ALLOWED_H_
#define CRYPTO_APPLE_SCOPED_KEYCHAIN_USER_INTERACTION_ALLOWED_H_

#include <MacTypes.h>
#include <optional>

#include "crypto/crypto_export.h"

namespace crypto::apple {

// Sets whether Keychain Services is permitted to display UI if needed by
// calling SecKeychainSetUserInteractionAllowed. This operates in a scoped
// fashion: on destruction, the previous state will be restored. This is useful
// to interact with the Keychain on a best-effort basis, without displaying any
// Keychain Services UI (which is beyond the application's control) to the user.
class CRYPTO_EXPORT ScopedKeychainUserInteractionAllowed {
 public:
  ScopedKeychainUserInteractionAllowed(
      const ScopedKeychainUserInteractionAllowed&) = delete;
  ScopedKeychainUserInteractionAllowed& operator=(
      const ScopedKeychainUserInteractionAllowed&) = delete;

  explicit ScopedKeychainUserInteractionAllowed(Boolean allowed,
                                                OSStatus* status = nullptr);

  ~ScopedKeychainUserInteractionAllowed();

 private:
  // The previous value of whether user interaction was allowed, for
  // restoration. If this is nullopt, this scoper did not succeed in its
  // constructor, so it must not attempt to restore the value.
  std::optional<Boolean> was_allowed_;
};

}  // namespace crypto::apple

#endif  // CRYPTO_APPLE_SCOPED_KEYCHAIN_USER_INTERACTION_ALLOWED_H_
