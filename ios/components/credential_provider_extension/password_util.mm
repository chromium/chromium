// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/credential_provider_extension/password_util.h"

#import <Security/Security.h>

#import "base/logging.h"

namespace {

constexpr NSString* kAccountEmailKey = @"AccountEmailKey";
constexpr NSString* kAccountGaiaKey = @"AccountGaiaKey";
constexpr NSString* kAccountInfoIdentifier =
    @"credential_provider_extension.account_info";

NSDictionary* AccountInfoLoadQuery() {
  return @{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : kAccountInfoIdentifier,
    (__bridge id)kSecReturnData : @YES,
  };
}

NSDictionary* AccountInfoStoreQuery(NSData* accountInfo) {
  return @{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)
    kSecAttrAccessible : (__bridge id)kSecAttrAccessibleWhenUnlocked,
    (__bridge id)kSecValueData : accountInfo,
    (__bridge id)kSecAttrAccount : kAccountInfoIdentifier,
  };
}

NSDictionary* AccountInfoUpdateQuery() {
  return @{
    (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge id)kSecAttrAccount : kAccountInfoIdentifier,
  };
}

NSDictionary* AccountInfoUpdateAttribs(NSData* account_info) {
  return @{(__bridge id)kSecValueData : account_info};
}

}  // namespace

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

AccountInfo LoadAccountInfoFromKeychain() {
  NSDictionary* accountInfoLoadQuery = AccountInfoLoadQuery();

  // Get the keychain item containing the password.
  CFDataRef sec_data_ref = nullptr;
  OSStatus status =
      SecItemCopyMatching((__bridge CFDictionaryRef)accountInfoLoadQuery,
                          (CFTypeRef*)&sec_data_ref);

  if (status != errSecSuccess) {
    DLOG(ERROR) << "Error retrieving gaia, OSStatus: " << status;
    return {/*gaia=*/nil, /*email=*/nil};
  }

  // This is safe because SecItemCopyMatching either assign an owned reference
  // to sec_data_ref, or leave it unchanged, and bridging maps nullptr to nil.
  NSData* data = (__bridge_transfer NSData*)sec_data_ref;
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
  NSString* gaia = [unarchiver decodeObjectForKey:kAccountGaiaKey];
  NSString* email = [unarchiver decodeObjectForKey:kAccountEmailKey];
  [unarchiver finishDecoding];
  return {gaia, email};
}

BOOL StoreAccountInfoInKeychain(NSString* gaia, NSString* user_email) {
  NSKeyedArchiver* archiver =
      [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
  [archiver encodeObject:gaia forKey:kAccountGaiaKey];
  [archiver encodeObject:user_email forKey:kAccountEmailKey];
  [archiver finishEncoding];
  NSData* accountData = [archiver encodedData];

  // Check that there is not already a stored account info.
  OSStatus status =
      SecItemCopyMatching((__bridge CFDictionaryRef)AccountInfoLoadQuery(),
                          /*result=*/nullptr);
  if (status == errSecItemNotFound) {
    // A new entry must be created.
    status =
        SecItemAdd((__bridge CFDictionaryRef)AccountInfoStoreQuery(accountData),
                   /*result=*/nullptr);

  } else if (status == noErr) {
    // The entry must be updated.
    status = SecItemUpdate(
        (__bridge CFDictionaryRef)AccountInfoUpdateQuery(),
        (__bridge CFDictionaryRef)AccountInfoUpdateAttribs(accountData));
  }

  return status == errSecSuccess;
}

}  // namespace credential_provider_extension
