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

}  // namespace crypto::apple
