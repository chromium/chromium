// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/credential_provider_extension/password_util.h"

#import <Security/Security.h>

#import "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#import "base/apple/scoped_cftyperef.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "crypto/apple/keychain_util.h"

namespace credential_provider_extension {

using base::apple::CFToNSPtrCast;
using base::apple::GetValueFromDictionary;
using base::apple::ScopedCFTypeRef;

NSString* PasswordWithKeychainIdentifier(NSString* identifier) {
  if (!identifier) {
    return nil;
  }
  NSDictionary* query = @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrAccount) : identifier,
    CFToNSPtrCast(kSecReturnData) : @YES,
    CFToNSPtrCast(kSecReturnAttributes) : @YES,
  };

  // Get the keychain item containing the password.
  base::apple::ScopedCFTypeRef<CFTypeRef> result;
  OSStatus status = SecItemCopyMatching(base::apple::NSToCFOwnershipCast(query),
                                        result.InitializeInto());

  if (status != errSecSuccess) {
    DLOG(ERROR) << "Error retrieving password, OSStatus: " << status;
    return nil;
  }

  CFDictionaryRef result_dict =
      base::apple::CFCast<CFDictionaryRef>(result.get());
  CFDataRef password_data_ref = base::apple::GetValueFromDictionary<CFDataRef>(
      result_dict, kSecValueData);

  // If the fetched password data was stored with kSecAttrAccessibleWhenUnlocked
  // accessibility, it should be migrated to instead use
  // kSecAttrAccessibleAfterFirstUnlock accessibility. The following query and
  // call to MigrateKeychainItemAccessibilityIfNeeded() handle this.
  base::apple::ScopedCFTypeRef<CFDictionaryRef> update_query =
      crypto::apple::GenerateGenericPasswordUpdateQuery(
          base::SysNSStringToUTF8(identifier));

  crypto::apple::MigrateKeychainItemAccessibilityIfNeeded(result_dict,
                                                          update_query.get());

  NSData* data = (__bridge NSData*)password_data_ref;
  return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
}

BOOL StorePasswordInKeychain(NSString* password, NSString* identifier) {
  if (!identifier || identifier.length == 0) {
    return NO;
  }

  NSData* passwordData = [password dataUsingEncoding:NSUTF8StringEncoding];

  CFStringRef attr_accessible =
      crypto::apple::GetKeychainAccessibilityAttribute();

  NSDictionary* query = @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrAccessible) : CFToNSPtrCast(attr_accessible),
    CFToNSPtrCast(kSecValueData) : passwordData,
    CFToNSPtrCast(kSecAttrAccount) : identifier,
  };

  OSStatus status =
      SecItemAdd(base::apple::NSToCFOwnershipCast(query), nullptr);
  return status == errSecSuccess;
}

}  // namespace credential_provider_extension
