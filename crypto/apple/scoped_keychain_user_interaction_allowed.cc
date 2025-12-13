// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/scoped_keychain_user_interaction_allowed.h"

#include <Security/Security.h>

#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(IS_MAC)

// ---------- ScopedKeychainUserInteractionAllowed ----------

// On the Mac, the SecItem keychain API has ways to suppress user interaction,
// but none of those ways work when using it to access file-based keychains.
// This was filed as FB16959400, but until that is addressed, this usage of
// deprecated API cannot be removed.

namespace crypto::apple {
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

}  // namespace crypto::apple

#endif  // BUILDFLAG(IS_MAC)
