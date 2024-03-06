// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_KEYCHAIN_UTIL_H_
#define CRYPTO_APPLE_KEYCHAIN_UTIL_H_

#include <string>

#include "build/build_config.h"
#include "crypto/crypto_export.h"

namespace crypto {

#if !BUILDFLAG(IS_IOS)
// Returns whether the main executable is signed with a keychain-access-groups
// entitlement that contains |keychain_access_group|.
// The API used to query this information is not available on iOS.
CRYPTO_EXPORT bool ExecutableHasKeychainAccessGroupEntitlement(
    const std::string& keychain_access_group);
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace crypto

#endif  // CRYPTO_APPLE_KEYCHAIN_UTIL_H_
