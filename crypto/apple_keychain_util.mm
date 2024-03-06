// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple_keychain_util.h"

#include <string>

#import <Security/Security.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"
#include "crypto/apple_keychain_v2.h"

namespace crypto {

#if !BUILDFLAG(IS_IOS)
bool ExecutableHasKeychainAccessGroupEntitlement(
    const std::string& keychain_access_group) {
  base::apple::ScopedCFTypeRef<SecTaskRef> task(SecTaskCreateFromSelf(nullptr));
  if (!task) {
    return false;
  }

  base::apple::ScopedCFTypeRef<CFTypeRef> entitlement_value_cftype(
      AppleKeychainV2::GetInstance().TaskCopyValueForEntitlement(
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

}  // namespace crypto
