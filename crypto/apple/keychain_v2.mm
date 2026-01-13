// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/keychain_v2.h"

#import <CryptoTokenKit/CryptoTokenKit.h>
#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/to_vector.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "crypto/apple/keychain_util.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFOwnershipCast;

enum KeychainAction { kKeychainActionCreate, kKeychainActionUpdate };

namespace crypto::apple {

static KeychainV2* g_keychain_instance_override = nullptr;

// static
KeychainV2& KeychainV2::GetInstance() {
  if (g_keychain_instance_override) {
    return *g_keychain_instance_override;
  }
  static base::NoDestructor<KeychainV2> k;
  return *k;
}

// static
void KeychainV2::SetInstanceOverride(KeychainV2* KeychainV2) {
  CHECK(!g_keychain_instance_override);
  g_keychain_instance_override = KeychainV2;
}

// static
void KeychainV2::ClearInstanceOverride() {
  CHECK(g_keychain_instance_override);
  g_keychain_instance_override = nullptr;
}

KeychainV2::KeychainV2() = default;
KeychainV2::~KeychainV2() = default;

NSArray* KeychainV2::GetTokenIDs() {
  return [[TKTokenWatcher alloc] init].tokenIDs;
}

base::apple::ScopedCFTypeRef<SecKeyRef> KeychainV2::KeyCreateRandomKey(
    CFDictionaryRef params,
    CFErrorRef* error) {
  return base::apple::ScopedCFTypeRef<SecKeyRef>(
      SecKeyCreateRandomKey(params, error));
}

base::apple::ScopedCFTypeRef<CFDataRef> KeychainV2::KeyCreateSignature(
    SecKeyRef key,
    SecKeyAlgorithm algorithm,
    CFDataRef data,
    CFErrorRef* error) {
  return base::apple::ScopedCFTypeRef<CFDataRef>(
      SecKeyCreateSignature(key, algorithm, data, error));
}

base::apple::ScopedCFTypeRef<SecKeyRef> KeychainV2::KeyCopyPublicKey(
    SecKeyRef key) {
  return base::apple::ScopedCFTypeRef<SecKeyRef>(SecKeyCopyPublicKey(key));
}

base::apple::ScopedCFTypeRef<CFDataRef>
KeychainV2::KeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef* error) {
  return base::apple::ScopedCFTypeRef<CFDataRef>(
      SecKeyCopyExternalRepresentation(key, error));
}

base::apple::ScopedCFTypeRef<CFDictionaryRef> KeychainV2::KeyCopyAttributes(
    SecKeyRef key) {
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      SecKeyCopyAttributes(key));
}

OSStatus KeychainV2::ItemAdd(CFDictionaryRef attributes, CFTypeRef* result) {
  return SecItemAdd(attributes, result);
}

OSStatus KeychainV2::ItemCopyMatching(CFDictionaryRef query,
                                      CFTypeRef* result) {
  return SecItemCopyMatching(query, result);
}

OSStatus KeychainV2::ItemDelete(CFDictionaryRef query) {
  return SecItemDelete(query);
}

OSStatus KeychainV2::ItemUpdate(CFDictionaryRef query,
                                CFDictionaryRef keychain_data) {
  return SecItemUpdate(query, keychain_data);
}

// Creates a dictionary that can be used to query the keystore.
base::apple::ScopedCFTypeRef<CFDictionaryRef> MakeGenericPasswordQuery(
    std::string_view serviceName,
    std::string_view accountName) {
  NSDictionary* query = @{
    // Type of element is generic password.
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),

    // Set the service name.
    CFToNSPtrCast(kSecAttrService) : base::SysUTF8ToNSString(serviceName),

    // Set the account name.
    CFToNSPtrCast(kSecAttrAccount) : base::SysUTF8ToNSString(accountName),

    // Use the proper search constants, return only the data of the first match.
    CFToNSPtrCast(kSecMatchLimit) : CFToNSPtrCast(kSecMatchLimitOne),
    CFToNSPtrCast(kSecReturnData) : @YES,
    CFToNSPtrCast(kSecReturnAttributes) : @YES,
  };
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      NSToCFOwnershipCast(query));
}

// Creates a dictionary containing the data to save into the keychain.
base::apple::ScopedCFTypeRef<CFDictionaryRef> MakeKeychainData(
    std::string_view serviceName,
    std::string_view accountName,
    base::span<const uint8_t> passwordData,
    KeychainAction action) {
  NSData* password = [NSData dataWithBytes:passwordData.data()
                                    length:passwordData.size()];

  NSDictionary* keychain_data;

  if (action != kKeychainActionCreate) {
    // If this is not a creation, no structural information is needed, only the
    // password.
    keychain_data = @{
      // Set the password.
      CFToNSPtrCast(kSecValueData) : password,
    };
  } else {
    CFStringRef attr_accessible =
        crypto::apple::GetKeychainAccessibilityAttribute();
    keychain_data = @{
      // Set the password.
      CFToNSPtrCast(kSecValueData) : password,

      // Set the type of the data.
      CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),

      // Set the accessibility attribute as determined above.
      CFToNSPtrCast(kSecAttrAccessible) : CFToNSPtrCast(attr_accessible),

      // Set the service name.
      CFToNSPtrCast(kSecAttrService) : base::SysUTF8ToNSString(serviceName),

      // Set the account name.
      CFToNSPtrCast(kSecAttrAccount) : base::SysUTF8ToNSString(accountName),
    };
  }

  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      NSToCFOwnershipCast(keychain_data));
}

#if BUILDFLAG(IS_IOS)

// Creates a dictionary that can be used to update a generic password. Only used
// on iOS.
base::apple::ScopedCFTypeRef<CFDictionaryRef> MakeGenericPasswordUpdateQuery(
    std::string_view service_name,
    std::string_view account_name) {
  NSDictionary* query = @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrService) : base::SysUTF8ToNSString(service_name),
    CFToNSPtrCast(kSecAttrAccount) : base::SysUTF8ToNSString(account_name),
  };
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      NSToCFOwnershipCast(query));
}
#endif  // BUILDFLAG(IS_IOS)

OSStatus KeychainV2::AddGenericPassword(std::string_view service_name,
                                        std::string_view account_name,
                                        base::span<const uint8_t> password) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> query =
      MakeGenericPasswordQuery(service_name, account_name);
  // Check that there is not already a password.
  OSStatus status = ItemCopyMatching(query.get(), /*result=*/nullptr);
  if (status == errSecItemNotFound) {
    // A new entry must be created.
    base::apple::ScopedCFTypeRef<CFDictionaryRef> keychain_data =
        MakeKeychainData(service_name, account_name, password,
                         kKeychainActionCreate);
    status = ItemAdd(keychain_data.get(), /*result=*/nullptr);
  } else if (status == noErr) {
    // The entry must be updated.
    base::apple::ScopedCFTypeRef<CFDictionaryRef> keychain_data =
        MakeKeychainData(service_name, account_name, password,
                         kKeychainActionUpdate);
    status = ItemUpdate(query.get(), keychain_data.get());
  }

  return status;
}

base::expected<std::vector<uint8_t>, OSStatus> KeychainV2::FindGenericPassword(
    std::string_view service_name,
    std::string_view account_name) {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> query =
      MakeGenericPasswordQuery(service_name, account_name);

  // Get the keychain item containing the password and attributes.
  // When kSecReturnData and kSecReturnAttributes are both true, the result is
  // a CFDictionaryRef, but the API returns it as a CFTypeRef.
  base::apple::ScopedCFTypeRef<CFTypeRef> result;
  OSStatus status = SecItemCopyMatching(query.get(), result.InitializeInto());
  if (status != noErr) {
    return base::unexpected(status);
  }

  CFDictionaryRef result_dict =
      base::apple::CFCast<CFDictionaryRef>(result.get());
  CFDataRef password_data = base::apple::GetValueFromDictionary<CFDataRef>(
      result_dict, kSecValueData);

#if BUILDFLAG(IS_IOS)
  base::apple::ScopedCFTypeRef<CFDictionaryRef> update_query =
      MakeGenericPasswordUpdateQuery(service_name, account_name);
  MigrateKeychainItemAccessibilityIfNeeded(result_dict, update_query.get());
#endif  // BUILDFLAG(IS_IOS)

  return base::ToVector(base::apple::CFDataToSpan(password_data));
}

#if !BUILDFLAG(IS_IOS)
base::apple::ScopedCFTypeRef<CFTypeRef> KeychainV2::TaskCopyValueForEntitlement(
    SecTaskRef task,
    CFStringRef entitlement,
    CFErrorRef* error) {
  return base::apple::ScopedCFTypeRef<CFTypeRef>(
      SecTaskCopyValueForEntitlement(task, entitlement, error));
}
#endif  // !BUILDFLAG(IS_IOS)

#if !BUILDFLAG(IS_IOS_TVOS)
BOOL KeychainV2::LAContextCanEvaluatePolicy(LAPolicy policy, NSError** error) {
  LAContext* context = [[LAContext alloc] init];
  return [context canEvaluatePolicy:policy error:error];
}
#endif  // !BUILDFLAG(IS_IOS_TVOS)

}  // namespace crypto::apple
