// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple_keychain.h"

#include <memory>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "crypto/apple_keychain_secitem.h"

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

}  // namespace crypto
