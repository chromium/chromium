// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/credential_provider_extension/password_util.h"

#import <Security/Security.h>

#import "base/logging.h"

namespace credential_provider_extension {

NSString* PasswordWithKeychainIdentifier(NSString* identifier) {
  if (!identifier) {
    return nil;
  }
  NSDictionary* query = @{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : identifier,
    (__bridge id)kSecReturnData : @YES
  };

  // Get the keychain item containing the password.
  CFDataRef sec_data_ref = nullptr;
  OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query,
                                        (CFTypeRef*)&sec_data_ref);

  if (status != errSecSuccess) {
    DLOG(ERROR) << "Error retrieving password, OSStatus: " << status;
    return nil;
  }

  // This is safe because SecItemCopyMatching either assign an owned reference
  // to sec_data_ref, or leave it unchanged, and bridging maps nullptr to nil.
  NSData* data = (__bridge_transfer NSData*)sec_data_ref;
  return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
}

BOOL StorePasswordInKeychain(NSString* password, NSString* identifier) {
  if (!identifier || identifier.length == 0) {
    return NO;
  }

  NSData* passwordData = [password dataUsingEncoding:NSUTF8StringEncoding];

  NSDictionary* query = @{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)
    kSecAttrAccessible : (__bridge id)kSecAttrAccessibleWhenUnlocked,
    (__bridge id)kSecValueData : passwordData,
    (__bridge id)kSecAttrAccount : identifier,
  };

  OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, NULL);
  return status == errSecSuccess;
}

}  // namespace credential_provider_extension
