// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/keychain_util.h"

#import <Security/Security.h>

#include <string>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "crypto/apple/keychain_v2.h"
#include "crypto/features.h"

namespace crypto::apple {

#if !BUILDFLAG(IS_IOS)
bool ExecutableHasKeychainAccessGroupEntitlement(
    const std::string& keychain_access_group) {
  base::apple::ScopedCFTypeRef<SecTaskRef> task(SecTaskCreateFromSelf(nullptr));
  if (!task) {
    return false;
  }

  base::apple::ScopedCFTypeRef<CFTypeRef> entitlement_value_cftype(
      KeychainV2::GetInstance().TaskCopyValueForEntitlement(
          task.get(), CFSTR("keychain-access-groups"), nullptr));
  if (!entitlement_value_cftype) {
    return false;
  }

  NSArray* entitlement_value_nsarray = base::apple::CFToNSPtrCast(
      base::apple::CFCast<CFArrayRef>(entitlement_value_cftype.get()));
  if (!entitlement_value_nsarray) {
    return false;
  }

  return [entitlement_value_nsarray
      containsObject:base::SysUTF8ToNSString(keychain_access_group)];
}
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_IOS)
// Creates a dictionary containing the attributes for an accessibility
// migration. Only used on iOS.
base::apple::ScopedCFTypeRef<CFDictionaryRef> MakeAttributeMigrationQuery() {
  NSDictionary* query = @{
    base::apple::CFToNSPtrCast(kSecAttrAccessible) :
        base::apple::CFToNSPtrCast(kSecAttrAccessibleAfterFirstUnlock),
  };
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      base::apple::NSToCFOwnershipCast(query));
}
#endif  // BUILDFLAG(IS_IOS)

CFStringRef GetKeychainAccessibilityAttribute() {
#if BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(
          crypto::features::kMigrateIOSKeychainAccessibility)) {
    return kSecAttrAccessibleAfterFirstUnlock;
  }
#endif  // BUILDFLAG(IS_IOS)
  return kSecAttrAccessibleWhenUnlocked;
}

#if BUILDFLAG(IS_IOS)
bool MigrateKeychainItemAccessibilityIfNeeded(CFDictionaryRef attributes,
                                              CFDictionaryRef query) {
  if (!base::FeatureList::IsEnabled(
          crypto::features::kMigrateIOSKeychainAccessibility)) {
    return false;
  }

  CFStringRef accessibility = base::apple::GetValueFromDictionary<CFStringRef>(
      attributes, kSecAttrAccessible);
  if (CFStringCompare(accessibility, kSecAttrAccessibleWhenUnlocked, 0) ==
      kCFCompareEqualTo) {
    // The item has the old accessibility attribute, so update it.
    base::apple::ScopedCFTypeRef<CFDictionaryRef> attributes_to_update =
        MakeAttributeMigrationQuery();
    OSStatus status = SecItemUpdate(query, attributes_to_update.get());
    // The status of the update is intentionally ignored. The goal is to
    // migrate the item on a best-effort basis. If it fails, the item will
    // just keep its legacy accessibility attribute.
    std::ignore = status;
    return true;
  }
  return false;
}

base::apple::ScopedCFTypeRef<CFDictionaryRef>
GenerateGenericPasswordUpdateQuery(std::string_view account_name) {
  NSDictionary* query = @{
    base::apple::CFToNSPtrCast(kSecClass) :
        base::apple::CFToNSPtrCast(kSecClassGenericPassword),
    base::apple::CFToNSPtrCast(kSecAttrAccount) :
        base::SysUTF8ToNSString(account_name),
  };
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      base::apple::NSToCFOwnershipCast(query));
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace crypto::apple
