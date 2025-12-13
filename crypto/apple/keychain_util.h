// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_APPLE_KEYCHAIN_UTIL_H_
#define CRYPTO_APPLE_KEYCHAIN_UTIL_H_

#include <CoreFoundation/CoreFoundation.h>

#include <string>
#include <string_view>

#include "base/apple/scoped_cftyperef.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "crypto/crypto_export.h"

namespace crypto::apple {

#if !BUILDFLAG(IS_IOS)
// Returns whether the main executable is signed with a keychain-access-groups
// entitlement that contains |keychain_access_group|.
// The API used to query this information is not available on iOS.
CRYPTO_EXPORT bool ExecutableHasKeychainAccessGroupEntitlement(
    const std::string& keychain_access_group);
#endif  // !BUILDFLAG(IS_IOS)

// Returns the accessibility attribute to use for new keychain items.
// On iOS, this depends on the kMigrateIOSKeychainAccessibility feature.
// On macOS, it returns kSecAttrAccessibleWhenUnlocked.
CRYPTO_EXPORT CFStringRef GetKeychainAccessibilityAttribute();

#if BUILDFLAG(IS_IOS)
// Migrates the accessibility attribute of a keychain item if needed.
// `attributes` is the dictionary of attributes for the item, returned by
// SecItemCopyMatching. `query` is a dictionary that uniquely identifies the
// item to update.
// Returns true if migration was attempted (success or failure), false if not
// needed.
CRYPTO_EXPORT bool MigrateKeychainItemAccessibilityIfNeeded(
    CFDictionaryRef attributes,
    CFDictionaryRef query);

// Creates a dictionary that can be used to update a generic password.
CRYPTO_EXPORT base::apple::ScopedCFTypeRef<CFDictionaryRef>
GenerateGenericPasswordUpdateQuery(std::string_view account_name);
#endif  // BUILDFLAG(IS_IOS)

}  // namespace crypto::apple

#endif  // CRYPTO_APPLE_KEYCHAIN_UTIL_H_
