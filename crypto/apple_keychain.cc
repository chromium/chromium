// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple_keychain.h"

#include <memory>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "crypto/apple_keychain_secitem.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/apple_keychain_seckeychain.h"
#endif

namespace crypto {

#if BUILDFLAG(IS_MAC)
BASE_FEATURE(kAppleKeychainUseSecItem,
             "AppleKeychainUseSecItem",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// static
std::unique_ptr<AppleKeychain> AppleKeychain::DefaultKeychain() {
#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(kAppleKeychainUseSecItem)) {
    return std::make_unique<AppleKeychainSecItem>();
  }

  return std::make_unique<AppleKeychainSecKeychain>();
#else
  return std::make_unique<AppleKeychainSecItem>();
#endif
}

AppleKeychain::AppleKeychain() = default;
AppleKeychain::~AppleKeychain() = default;

#if BUILDFLAG(IS_MAC)

// ---------- ScopedKeychainUserInteractionAllowed ----------

// Much of the Keychain API was marked deprecated as of the macOS 13 SDK.
// Removal of its use is tracked in https://crbug.com/40233280 but deprecation
// warnings are disabled in the meanwhile.
//
// This specific usage is unfortunate. While the new SecItem keychain API has
// ways to suppress user interaction, none of those ways work when using the new
// API to access file-based keychains. This was filed as FB16959400, but until
// that is addressed, this usage of deprecated API cannot be removed.

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

}  // namespace crypto
