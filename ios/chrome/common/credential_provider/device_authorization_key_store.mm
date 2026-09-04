// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/device_authorization_key_store.h"

#import <Security/Security.h>

#import "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#import "base/apple/scoped_cftyperef.h"
#import "base/strings/sys_string_conversions.h"

namespace {

using ::base::apple::CFToNSPtrCast;
using ::base::apple::NSToCFOwnershipCast;

// Service identifier for Device Authorization Keys in the iOS Keychain.
NSString* const kDeviceAuthorizationKeyKeychainService =
    @"com.google.chrome.DeviceAuthorizationKey";

// Constructs the base query dictionary identifying the Keychain item for
// `gaia_id`.
NSMutableDictionary* MakeBaseKeychainQuery(const std::string& gaia_id) {
  return [NSMutableDictionary dictionaryWithDictionary:@{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrService) : kDeviceAuthorizationKeyKeychainService,
    CFToNSPtrCast(kSecAttrAccount) : base::SysUTF8ToNSString(gaia_id),
    CFToNSPtrCast(kSecAttrSynchronizable) : @NO,
  }];
}

}  // namespace

bool StoreDeviceAuthorizationKeys(const std::string& gaia_id,
                                  const DeviceAuthorizationKeys& keys) {
  if (gaia_id.empty() || keys.keys().empty()) {
    return false;
  }

  for (const DeviceAuthorizationKey& key : keys.keys()) {
    if (key.key().empty() || key.version() < 0) {
      return false;
    }
  }

  std::string serialized_data;
  if (!keys.SerializeToString(&serialized_data)) {
    return false;
  }

  NSData* key_data = [NSData dataWithBytes:serialized_data.data()
                                    length:serialized_data.size()];

  NSMutableDictionary* attributes_to_add = MakeBaseKeychainQuery(gaia_id);
  attributes_to_add[CFToNSPtrCast(kSecValueData)] = key_data;
  attributes_to_add[CFToNSPtrCast(kSecAttrAccessible)] =
      CFToNSPtrCast(kSecAttrAccessibleWhenUnlockedThisDeviceOnly);

  OSStatus status = SecItemAdd(NSToCFOwnershipCast(attributes_to_add), nullptr);
  if (status == errSecSuccess) {
    return true;
  }

  if (status == errSecDuplicateItem) {
    NSMutableDictionary* query = MakeBaseKeychainQuery(gaia_id);
    NSDictionary* attributes_to_update = @{
      CFToNSPtrCast(kSecValueData) : key_data,
    };
    OSStatus update_status = SecItemUpdate(
        NSToCFOwnershipCast(query), NSToCFOwnershipCast(attributes_to_update));
    return update_status == errSecSuccess;
  }

  return false;
}

std::optional<DeviceAuthorizationKeys> GetDeviceAuthorizationKeys(
    const std::string& gaia_id) {
  if (gaia_id.empty()) {
    return std::nullopt;
  }

  NSMutableDictionary* query = MakeBaseKeychainQuery(gaia_id);
  query[CFToNSPtrCast(kSecReturnData)] = @YES;
  query[CFToNSPtrCast(kSecMatchLimit)] = CFToNSPtrCast(kSecMatchLimitOne);

  base::apple::ScopedCFTypeRef<CFTypeRef> result;
  OSStatus status =
      SecItemCopyMatching(NSToCFOwnershipCast(query), result.InitializeInto());

  if (status != errSecSuccess || !result) {
    return std::nullopt;
  }

  CFDataRef data_ref = base::apple::CFCast<CFDataRef>(result.get());
  if (!data_ref) {
    return std::nullopt;
  }

  NSData* data = CFToNSPtrCast(data_ref);
  DeviceAuthorizationKeys keys;
  if (!keys.ParseFromArray(data.bytes, static_cast<int>(data.length))) {
    return std::nullopt;
  }

  return keys;
}
