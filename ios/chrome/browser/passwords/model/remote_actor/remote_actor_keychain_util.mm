// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_keychain_util.h"

#import <Security/Security.h>

#import "base/apple/foundation_util.h"
#import "base/containers/to_vector.h"
#import "base/threading/scoped_blocking_call.h"

namespace {

std::optional<std::vector<uint8_t>> GetKeyWithQuery(NSString* access_group,
                                                    NSString* account,
                                                    NSString* service) {
  NSMutableDictionary* query = [@{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : account,
    (__bridge id)kSecAttrService : service,
    (__bridge id)kSecReturnData : @YES,
    (__bridge id)kSecMatchLimit : (__bridge id)kSecMatchLimitOne,
  } mutableCopy];

  if (access_group.length > 0) {
    query[(__bridge id)kSecAttrAccessGroup] = access_group;
  }

  CFTypeRef result = nullptr;
  OSStatus status;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
  }
  if (status != errSecSuccess || !result) {
    return std::nullopt;
  }

  NSData* data = (__bridge_transfer NSData*)result;
  return base::ToVector(base::apple::NSDataToSpan(data));
}

}  // namespace

namespace remote_actor {

std::optional<std::vector<uint8_t>> GetSharedSymmetricKey() {
  return GetKeyWithQuery(kKeychainAccessGroup, kKeychainAccount,
                         kKeychainService);
}

}  // namespace remote_actor
