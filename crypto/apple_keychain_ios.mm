// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple_keychain.h"

#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"

namespace {

enum KeychainAction {
  kKeychainActionCreate,
  kKeychainActionUpdate
};

base::ScopedCFTypeRef<CFStringRef> StringWithBytesAndLength(const char* bytes,
                                                            UInt32 length) {
  return base::ScopedCFTypeRef<CFStringRef>(
      CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8*>(bytes),
                              length, kCFStringEncodingUTF8,
                              /*isExternalRepresentation=*/false));
}

// Creates a dictionary that can be used to query the keystore.
base::ScopedCFTypeRef<CFDictionaryRef> MakeGenericPasswordQuery(
    UInt32 serviceNameLength,
    const char* serviceName,
    UInt32 accountNameLength,
    const char* accountName) {
  CFMutableDictionaryRef query =
      CFDictionaryCreateMutable(nullptr, 5, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);
  // Type of element is generic password.
  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);

  // Set the service name.
  CFDictionarySetValue(
      query, kSecAttrService,
      StringWithBytesAndLength(serviceName, serviceNameLength));

  // Set the account name.
  CFDictionarySetValue(
      query, kSecAttrAccount,
      StringWithBytesAndLength(accountName, accountNameLength));

  // Use the proper search constants, return only the data of the first match.
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);
  CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);

  return base::ScopedCFTypeRef<CFDictionaryRef>(query);
}

// Creates a dictionary containing the data to save into the keychain.
base::ScopedCFTypeRef<CFDictionaryRef> MakeKeychainData(
    UInt32 serviceNameLength,
    const char* serviceName,
    UInt32 accountNameLength,
    const char* accountName,
    UInt32 passwordLength,
    const void* passwordData,
    KeychainAction action) {
  CFMutableDictionaryRef keychain_data =
      CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks);

  // Set the password.
  NSData* password = [NSData dataWithBytes:passwordData length:passwordLength];
  CFDictionarySetValue(keychain_data, kSecValueData,
                       base::apple::NSToCFPtrCast(password));

  // If this is not a creation, no structural information is needed.
  if (action != kKeychainActionCreate) {
    return base::ScopedCFTypeRef<CFDictionaryRef>(keychain_data);
  }

  // Set the type of the data.
  CFDictionarySetValue(keychain_data, kSecClass, kSecClassGenericPassword);

  // Only allow access when the device has been unlocked.
  CFDictionarySetValue(keychain_data,
                       kSecAttrAccessible,
                       kSecAttrAccessibleWhenUnlocked);

  // Set the service name.
  CFDictionarySetValue(
      keychain_data, kSecAttrService,
      StringWithBytesAndLength(serviceName, serviceNameLength));

  // Set the account name.
  CFDictionarySetValue(
      keychain_data, kSecAttrAccount,
      StringWithBytesAndLength(accountName, accountNameLength));

  return base::ScopedCFTypeRef<CFDictionaryRef>(keychain_data);
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
  base::ScopedCFTypeRef<CFDictionaryRef> query = MakeGenericPasswordQuery(
      serviceNameLength, serviceName, accountNameLength, accountName);
  // Check that there is not already a password.
  OSStatus status = SecItemCopyMatching(query, /*result=*/nullptr);
  if (status == errSecItemNotFound) {
    // A new entry must be created.
    base::ScopedCFTypeRef<CFDictionaryRef> keychain_data = MakeKeychainData(
        serviceNameLength, serviceName, accountNameLength, accountName,
        passwordLength, passwordData, kKeychainActionCreate);
    status = SecItemAdd(keychain_data, /*result=*/nullptr);
  } else if (status == noErr) {
    // The entry must be updated.
    base::ScopedCFTypeRef<CFDictionaryRef> keychain_data = MakeKeychainData(
        serviceNameLength, serviceName, accountNameLength, accountName,
        passwordLength, passwordData, kKeychainActionUpdate);
    status = SecItemUpdate(query, keychain_data);
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
  base::ScopedCFTypeRef<CFDictionaryRef> query = MakeGenericPasswordQuery(
      serviceNameLength, serviceName, accountNameLength, accountName);

  // Get the keychain item containing the password.
  base::ScopedCFTypeRef<CFTypeRef> result;
  OSStatus status = SecItemCopyMatching(query, result.InitializeInto());

  if (status != noErr) {
    if (passwordData) {
      *passwordData = nullptr;
      *passwordLength = 0;
    }
    return status;
  }

  if (passwordData) {
    CFDataRef data = base::mac::CFCast<CFDataRef>(result);
    NSUInteger length = CFDataGetLength(data);
    *passwordData = malloc(length * sizeof(UInt8));
    CFDataGetBytes(data, CFRangeMake(0, length), (UInt8*)*passwordData);
    *passwordLength = length;
  }
  return status;
}

}  // namespace crypto
