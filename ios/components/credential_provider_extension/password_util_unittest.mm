// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/credential_provider_extension/password_util.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import "base/apple/scoped_cftyperef.h"
#import "base/strings/sys_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace credential_provider_extension {

NSString* kCredentialKey1 = @"key1";
NSString* kCredentialKey2 = @"key2";

NSString* kAccountInfoEmail1 = @"peter.parker@gmail.com";
NSString* kAccountInfoEmail2 = @"mary.jane@gmail.com";
NSString* kAccountInfoGaia1 = @"123456789";
NSString* kAccountInfoGaia2 = @"987654321";
NSString* kCredentialPassword1 = @"pa55word1";
NSString* kCredentialPassword2 = @"p4ssw0rd2";

NSString* kTestPrefix = @"com.google.common.SSO.KeychainTest.";

NSString* KeyWithPrefix(NSString* key) {
  return [NSString stringWithFormat:@"%@%@", kTestPrefix, key];
}

void RemovePasswordForKey(NSString* key) {
  NSDictionary* query = @{
    (__bridge NSString*)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge NSString*)kSecAttrAccount : KeyWithPrefix(key)
  };
  OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
  ASSERT_TRUE(status == errSecSuccess || status == errSecItemNotFound);
}

void AddPasswordForKey(NSString* key, NSString* password) {
  std::string utf8_password = base::SysNSStringToUTF8(password);
  base::apple::ScopedCFTypeRef<CFDataRef> data(CFDataCreate(
      nullptr, reinterpret_cast<const UInt8*>(utf8_password.data()),
      utf8_password.size()));

  NSDictionary* query = @{
    (__bridge NSString*)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge NSString*)
    kSecAttrAccessible : (__bridge id)kSecAttrAccessibleWhenUnlocked,
    (__bridge NSString*)kSecAttrAccount : KeyWithPrefix(key),
    (__bridge NSString*)kSecValueData : (__bridge id)data.get(),
  };

  OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, nullptr);
  ASSERT_TRUE(status == errSecSuccess);
}

void VerifyKeyNotFound(NSString* key) {
  NSDictionary* query = @{
    (__bridge NSString*)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge NSString*)kSecAttrAccount : KeyWithPrefix(key),
    (__bridge NSString*)kSecReturnData : @YES,
  };
  base::apple::ScopedCFTypeRef<CFTypeRef> cf_result;
  OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query,
                                        cf_result.InitializeInto());
  ASSERT_EQ(errSecItemNotFound, status);
}

class PasswordUtilKeychainTest : public PlatformTest {
 public:
  void SetUp() override;
  void TearDown() override;
};

void PasswordUtilKeychainTest::SetUp() {
  // Make sure it doesn't fail if previous run did.
  RemovePasswordForKey(kCredentialKey1);
  RemovePasswordForKey(kCredentialKey2);
}

void PasswordUtilKeychainTest::TearDown() {
  // Check there's nothing left of the test.
  VerifyKeyNotFound(kCredentialKey1);
  VerifyKeyNotFound(kCredentialKey2);
}

// Tests retrieval of saved passwords, using PasswordWithKeychainIdentifier.
TEST_F(PasswordUtilKeychainTest, CheckRestoreOfSavedPasswords) {
  AddPasswordForKey(kCredentialKey1, kCredentialPassword1);
  AddPasswordForKey(kCredentialKey2, kCredentialPassword2);
  EXPECT_NSEQ(PasswordWithKeychainIdentifier(KeyWithPrefix(kCredentialKey2)),
              kCredentialPassword2);
  EXPECT_NSEQ(PasswordWithKeychainIdentifier(KeyWithPrefix(kCredentialKey1)),
              kCredentialPassword1);
  RemovePasswordForKey(kCredentialKey1);
  EXPECT_NSEQ(PasswordWithKeychainIdentifier(KeyWithPrefix(kCredentialKey2)),
              kCredentialPassword2);
  RemovePasswordForKey(kCredentialKey2);
}

// Tests retrieval of saved passwords, using an empty string as arg.
TEST_F(PasswordUtilKeychainTest, EmptyArgument) {
  EXPECT_NSEQ(PasswordWithKeychainIdentifier(@""), nil);
}

// Tests retrieval of saved passwords, nil as arg.
TEST_F(PasswordUtilKeychainTest, NilArgument) {
  EXPECT_NSEQ(PasswordWithKeychainIdentifier(nil), nil);
}

// Tests storing passwords with StorePassword.
TEST_F(PasswordUtilKeychainTest, CheckSavingPasswords) {
  EXPECT_TRUE(StorePasswordInKeychain(kCredentialPassword1,
                                      KeyWithPrefix(kCredentialKey1)));
  EXPECT_TRUE(StorePasswordInKeychain(kCredentialPassword2,
                                      KeyWithPrefix(kCredentialKey2)));

  EXPECT_NSEQ(PasswordWithKeychainIdentifier(KeyWithPrefix(kCredentialKey2)),
              kCredentialPassword2);
  EXPECT_NSEQ(PasswordWithKeychainIdentifier(KeyWithPrefix(kCredentialKey1)),
              kCredentialPassword1);
  RemovePasswordForKey(kCredentialKey1);
  EXPECT_NSEQ(PasswordWithKeychainIdentifier(KeyWithPrefix(kCredentialKey2)),
              kCredentialPassword2);
  RemovePasswordForKey(kCredentialKey2);
}

// Tests storing a password with an empty identifier
TEST_F(PasswordUtilKeychainTest, StoreEmptyIdentifier) {
  EXPECT_FALSE(StorePasswordInKeychain(kCredentialPassword1, @""));
}

// Tests storing and loading the account info (gaia and email).
TEST_F(PasswordUtilKeychainTest, StoreAccountInfo) {
  EXPECT_TRUE(
      StoreAccountInfoInKeychain(kAccountInfoGaia1, kAccountInfoEmail1));
  EXPECT_NSEQ(LoadAccountInfoFromKeychain().gaia, kAccountInfoGaia1);
  EXPECT_NSEQ(LoadAccountInfoFromKeychain().email, kAccountInfoEmail1);
}

// Tests updating an existing account info (gaia and email).
TEST_F(PasswordUtilKeychainTest, UpdateAccountInfo) {
  EXPECT_TRUE(
      StoreAccountInfoInKeychain(kAccountInfoGaia2, kAccountInfoEmail2));
  EXPECT_NSEQ(LoadAccountInfoFromKeychain().gaia, kAccountInfoGaia2);
  EXPECT_NSEQ(LoadAccountInfoFromKeychain().email, kAccountInfoEmail2);
  EXPECT_TRUE(
      StoreAccountInfoInKeychain(kAccountInfoGaia2, kAccountInfoEmail2));
  EXPECT_NSEQ(LoadAccountInfoFromKeychain().gaia, kAccountInfoGaia2);
  EXPECT_NSEQ(LoadAccountInfoFromKeychain().email, kAccountInfoEmail2);
}

}  // credential_provider_extension
