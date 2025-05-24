// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple_keychain_secitem.h"

#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/containers/to_vector.h"
#include "base/strings/sys_string_conversions.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFOwnershipCast;

namespace {

enum KeychainAction { kKeychainActionCreate, kKeychainActionUpdate };

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
    keychain_data = @{
      // Set the password.
      CFToNSPtrCast(kSecValueData) : password,

      // Set the type of the data.
      CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),

      // Only allow access when the device has been unlocked.
      CFToNSPtrCast(kSecAttrAccessible) :
          CFToNSPtrCast(kSecAttrAccessibleWhenUnlocked),

      // Set the service name.
      CFToNSPtrCast(kSecAttrService) : base::SysUTF8ToNSString(serviceName),

      // Set the account name.
      CFToNSPtrCast(kSecAttrAccount) : base::SysUTF8ToNSString(accountName),
    };
  }

  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      NSToCFOwnershipCast(keychain_data));
}

}  // namespace

namespace crypto {

AppleKeychainSecItem::AppleKeychainSecItem() = default;

AppleKeychainSecItem::~AppleKeychainSecItem() = default;

OSStatus AppleKeychainSecItem::AddGenericPassword(
    std::string_view service_name,
    std::string_view account_name,
    base::span<const uint8_t> password) const {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> query =
      MakeGenericPasswordQuery(service_name, account_name);
  // Check that there is not already a password.
  OSStatus status = SecItemCopyMatching(query.get(), /*result=*/nullptr);
  if (status == errSecItemNotFound) {
    // A new entry must be created.
    base::apple::ScopedCFTypeRef<CFDictionaryRef> keychain_data =
        MakeKeychainData(service_name, account_name, password,
                         kKeychainActionCreate);
    status = SecItemAdd(keychain_data.get(), /*result=*/nullptr);
  } else if (status == noErr) {
    // The entry must be updated.
    base::apple::ScopedCFTypeRef<CFDictionaryRef> keychain_data =
        MakeKeychainData(service_name, account_name, password,
                         kKeychainActionUpdate);
    status = SecItemUpdate(query.get(), keychain_data.get());
  }

  return status;
}

base::expected<std::vector<uint8_t>, OSStatus>
AppleKeychainSecItem::FindGenericPassword(std::string_view service_name,
                                          std::string_view account_name) const {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> query =
      MakeGenericPasswordQuery(service_name, account_name);

  // Get the keychain item containing the password.
  base::apple::ScopedCFTypeRef<CFTypeRef> result;
  OSStatus status = SecItemCopyMatching(query.get(), result.InitializeInto());
  if (status != noErr) {
    return base::unexpected(status);
  }

  CFDataRef data = base::apple::CFCast<CFDataRef>(result.get());
  return base::ToVector(base::apple::CFDataToSpan(data));
}

}  // namespace crypto
