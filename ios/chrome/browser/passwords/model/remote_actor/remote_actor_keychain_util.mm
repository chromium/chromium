// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_keychain_util.h"

#import <Security/Security.h>

#import "base/apple/foundation_util.h"
#import "base/containers/to_vector.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

namespace {

NSString* g_keychain_access_group_override = nil;

NSString* GetEffectiveAccessGroup() {
  return g_keychain_access_group_override
             ?: app_group::CommonApplicationGroup();
}

NSMutableDictionary* CreateBaseKeychainQuery(NSString* access_group,
                                             NSString* account,
                                             NSString* service) {
  NSMutableDictionary* query = [@{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : account,
    (__bridge id)kSecAttrService : service,
  } mutableCopy];

  if (access_group.length > 0) {
    query[(__bridge id)kSecAttrAccessGroup] = access_group;
  }

  return query;
}

std::optional<std::vector<uint8_t>> GetKeyWithQuery(NSString* access_group,
                                                    NSString* account,
                                                    NSString* service) {
  NSMutableDictionary* query =
      CreateBaseKeychainQuery(access_group, account, service);
  query[(__bridge id)kSecReturnData] = @YES;
  query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;

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

bool DeleteKeyWithQuery(NSString* access_group,
                        NSString* account,
                        NSString* service) {
  NSMutableDictionary* query =
      CreateBaseKeychainQuery(access_group, account, service);

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  const OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
  return status == errSecSuccess || status == errSecItemNotFound;
}

}  // namespace

std::optional<std::vector<uint8_t>> GetRemoteActorSharedSymmetricKey() {
  return GetKeyWithQuery(GetEffectiveAccessGroup(), kRemoteActorKeychainAccount,
                         kRemoteActorKeychainService);
}

bool DeleteRemoteActorSharedSymmetricKey() {
  return DeleteKeyWithQuery(GetEffectiveAccessGroup(),
                            kRemoteActorKeychainAccount,
                            kRemoteActorKeychainService);
}

void SetRemoteActorKeychainAccessGroupOverrideForTesting(  // IN-TEST
    NSString* access_group_override) {
  g_keychain_access_group_override =
      access_group_override ? [access_group_override copy] : @"";
}

void ResetRemoteActorKeychainAccessGroupOverrideForTesting() {  // IN-TEST
  g_keychain_access_group_override = nil;
}
