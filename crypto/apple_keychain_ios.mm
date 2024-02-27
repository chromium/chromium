// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple_keychain.h"

#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"

using base::apple::CFToNSPtrCast;
using base::apple::NSToCFOwnershipCast;

namespace {

enum KeychainAction {
  kKeychainActionCreate,
  kKeychainActionUpdate
};

NSString* StringWithBytesAndLength(const char* bytes, UInt32 length) {
  return [[NSString alloc] initWithBytes:bytes
                                  length:length
                                encoding:NSUTF8StringEncoding];
}

// Creates a dictionary that can be used to query the keystore.
base::apple::ScopedCFTypeRef<CFDictionaryRef> MakeGenericPasswordQuery(
    UInt32 serviceNameLength,
    const char* serviceName,
    UInt32 accountNameLength,
    const char* accountName) {
  NSDictionary* query = @{
    // Type of element is generic password.
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),

    // Set the service name.
    CFToNSPtrCast(kSecAttrService) :
        StringWithBytesAndLength(serviceName, serviceNameLength),

    // Set the account name.
    CFToNSPtrCast(kSecAttrAccount) :
        StringWithBytesAndLength(accountName, accountNameLength),

    // Use the proper search constants, return only the data of the first match.
    CFToNSPtrCast(kSecMatchLimit) : CFToNSPtrCast(kSecMatchLimitOne),
    CFToNSPtrCast(kSecReturnData) : @YES,
  };
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      NSToCFOwnershipCast(query));
}

// Creates a dictionary containing the data to save into the keychain.
base::apple::ScopedCFTypeRef<CFDictionaryRef> MakeKeychainData(
    UInt32 serviceNameLength,
    const char* serviceName,
    UInt32 accountNameLength,
    const char* accountName,
    UInt32 passwordLength,
    const void* passwordData,
    KeychainAction action) {
  NSData* password = [NSData dataWithBytes:passwordData length:passwordLength];

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
      CFToNSPtrCast(kSecAttrService) :
          StringWithBytesAndLength(serviceName, serviceNameLength),

      // Set the account name.
      CFToNSPtrCast(kSecAttrAccount) :
          StringWithBytesAndLength(accountName, accountNameLength),
    };
  }

  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(
      NSToCFOwnershipCast(keychain_data));
}

}  // namespace

namespace crypto {

AppleKeychain::AppleKeychain() = default;

AppleKeychain::~AppleKeychain() = default;

OSStatus AppleKeychain::ItemFreeContent(void* data) const {
  free(data);
  return noErr;
}

OSStatus AppleKeychain::AddGenericPassword(
    UInt32 serviceNameLength,
    const char* serviceName,
    UInt32 accountNameLength,
    const char* accountName,
    UInt32 passwordLength,
    const void* passwordData,
    AppleSecKeychainItemRef* itemRef) const {
  base::apple::ScopedCFTypeRef<CFDictionaryRef> query =
      MakeGenericPasswordQuery(serviceNameLength, serviceName,
                               accountNameLength, accountName);
  // Check that there is not already a password.
  OSStatus status = SecItemCopyMatching(query.get(), /*result=*/nullptr);
  if (status == errSecItemNotFound) {
    // A new entry must be created.
    base::apple::ScopedCFTypeRef<CFDictionaryRef> keychain_data =
        MakeKeychainData(serviceNameLength, serviceName, accountNameLength,
                         accountName, passwordLength, passwordData,
                         kKeychainActionCreate);
    status = SecItemAdd(keychain_data.get(), /*result=*/nullptr);
  } else if (status == noErr) {
    // The entry must be updated.
    base::apple::ScopedCFTypeRef<CFDictionaryRef> keychain_data =
        MakeKeychainData(serviceNameLength, serviceName, accountNameLength,
                         accountName, passwordLength, passwordData,
                         kKeychainActionUpdate);
    status = SecItemUpdate(query.get(), keychain_data.get());
  }

  return status;
}

OSStatus AppleKeychain::FindGenericPassword(
    UInt32 serviceNameLength,
    const char* serviceName,
    UInt32 accountNameLength,
    const char* accountName,
    UInt32* passwordLength,
    void** passwordData,
    AppleSecKeychainItemRef* itemRef) const {
  DCHECK((passwordData && passwordLength) ||
         (!passwordData && !passwordLength));
  base::apple::ScopedCFTypeRef<CFDictionaryRef> query =
      MakeGenericPasswordQuery(serviceNameLength, serviceName,
                               accountNameLength, accountName);

  // Get the keychain item containing the password.
  base::apple::ScopedCFTypeRef<CFTypeRef> result;
  OSStatus status = SecItemCopyMatching(query.get(), result.InitializeInto());

  if (status != noErr) {
    if (passwordData) {
      *passwordData = nullptr;
      *passwordLength = 0;
    }
    return status;
  }

  if (passwordData) {
    CFDataRef data = base::apple::CFCast<CFDataRef>(result.get());
    NSUInteger length = CFDataGetLength(data);
    *passwordData = malloc(length * sizeof(UInt8));
    CFDataGetBytes(data, CFRangeMake(0, length), (UInt8*)*passwordData);
    *passwordLength = length;
  }
  return status;
}

}  // namespace crypto
