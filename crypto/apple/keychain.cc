// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/keychain.h"

#include <memory>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "crypto/apple/keychain_secitem.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

namespace crypto::apple {

// static
std::unique_ptr<Keychain> Keychain::DefaultKeychain() {
  return std::make_unique<KeychainSecItem>();
}

Keychain::Keychain() = default;
Keychain::~Keychain() = default;

#if BUILDFLAG(IS_MAC)

// ---------- ScopedKeychainUserInteractionAllowed ----------

// On the Mac, the SecItem keychain API has ways to suppress user interaction,
// but none of those ways work when using it to access file-based keychains.
// This was filed as FB16959400, but until that is addressed, this usage of
// deprecated API cannot be removed.

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

ScopedKeychainUserInteractionAllowed::ScopedKeychainUserInteractionAllowed(
    Boolean allowed,
    OSStatus* status) {
  Boolean was_allowed;
  OSStatus local_status = noErr;
  absl::Cleanup cleanup = [&status, &local_status] {
    if (status) {
      *status = local_status;
    }
  };

  local_status = SecKeychainGetUserInteractionAllowed(&was_allowed);
  if (local_status != noErr) {
    return;
  }

  local_status = SecKeychainSetUserInteractionAllowed(allowed);
  if (local_status != noErr) {
    return;
  }

  was_allowed_ = was_allowed;
}

ScopedKeychainUserInteractionAllowed::~ScopedKeychainUserInteractionAllowed() {
  if (was_allowed_.has_value()) {
    SecKeychainSetUserInteractionAllowed(was_allowed_.value());
  }
}

#pragma clang diagnostic pop

#endif  // BUILDFLAG(IS_MAC)

}  // namespace crypto::apple
