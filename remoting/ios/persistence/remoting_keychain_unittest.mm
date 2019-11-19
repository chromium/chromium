// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "remoting/ios/persistence/remoting_keychain.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include "base/base64.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/rand_util.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace remoting {

const char kTestServicePrefix[] =
    "com.google.ChromeRemoteDesktop.RemotingKeychainTest.";

std::string RandomBase64String(int byte_length) {
  std::string random_bytes = base::RandBytesAsString(byte_length);
  std::string random_string;
  base::Base64Encode(random_bytes, &random_string);
  return random_string;
}

NSString* KeyToService(Keychain::Key key) {
  return [NSString stringWithFormat:@"%s%s", kTestServicePrefix,
                                    Keychain::KeyToString(key).c_str()];
}

void RemoveAllKeychainsForKey(Keychain::Key key) {
  NSDictionary* remove_all_query = @{
    (__bridge NSString*)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge NSString*)kSecAttrService : KeyToService(key)
  };
  OSStatus status = SecItemDelete((__bridge CFDictionaryRef)remove_all_query);
  ASSERT_TRUE(status == errSecSuccess || status == errSecItemNotFound);
}

void VerifyNoKeychainForKey(Keychain::Key key) {
  NSDictionary* get_all_query = @{
    (__bridge NSString*)kSecClass : (__bridge id)kSecClassGenericPassword,
    (__bridge NSString*)kSecAttrService : KeyToService(key),
    (__bridge NSString*)kSecReturnData : @YES,
  };
  base::ScopedCFTypeRef<CFTypeRef> cf_result;
  OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)get_all_query,
                                        cf_result.InitializeInto());
  ASSERT_EQ(errSecItemNotFound, status);
}

#pragma mark - RemotingKeychainTest

class RemotingKeychainTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  void SetKeychainAndVerify(Keychain::Key key,
                            const std::string& account,
                            const std::string& data);
  void VerifyKeychain(Keychain::Key key,
                      const std::string& account,
                      const std::string& expected_data);
  void RemoveKeychainAndVerify(Keychain::Key key, const std::string& account);

  RemotingKeychain* keychain_;
};

void RemotingKeychainTest::SetUp() {
  RemoveAllKeychainsForKey(Keychain::Key::PAIRING_INFO);
  RemoveAllKeychainsForKey(Keychain::Key::REFRESH_TOKEN);

  keychain_ = RemotingKeychain::GetInstance();
  keychain_->SetServicePrefixForTesting(kTestServicePrefix);
}

void RemotingKeychainTest::TearDown() {
  VerifyNoKeychainForKey(Keychain::Key::PAIRING_INFO);
  VerifyNoKeychainForKey(Keychain::Key::REFRESH_TOKEN);
}

void RemotingKeychainTest::SetKeychainAndVerify(Keychain::Key key,
                                                const std::string& account,
                                                const std::string& data) {
  keychain_->SetData(key, account, data);
  VerifyKeychain(key, account, data);
}

void RemotingKeychainTest::VerifyKeychain(Keychain::Key key,
                                          const std::string& account,
                                          const std::string& expected_data) {
  std::string data = keychain_->GetData(key, account);
  EXPECT_EQ(expected_data, data);
}

void RemotingKeychainTest::RemoveKeychainAndVerify(Keychain::Key key,
                                                   const std::string& account) {
  keychain_->RemoveData(key, account);
  VerifyKeychain(key, account, "");
}

#pragma mark - Tests

// Tests to verify that the interface is doing the right thing on iOS.

TEST_F(RemotingKeychainTest,
       AddThenUpdateAndRemoveOneKeychain_dataAddedThenDeleted) {
  std::string account = RandomBase64String(16);
  SetKeychainAndVerify(Keychain::Key::PAIRING_INFO, account,
                       base::RandBytesAsString(128));
  SetKeychainAndVerify(Keychain::Key::PAIRING_INFO, account,
                       base::RandBytesAsString(128));
  RemoveKeychainAndVerify(Keychain::Key::PAIRING_INFO, account);
}

TEST_F(
    RemotingKeychainTest,
    AddThenUpdateAndRemoveOneKeychainWithUnspecifiedAccount_dataAddedThenDeleted) {
  SetKeychainAndVerify(Keychain::Key::PAIRING_INFO,
                       Keychain::kUnspecifiedAccount,
                       base::RandBytesAsString(128));
  SetKeychainAndVerify(Keychain::Key::PAIRING_INFO,
                       Keychain::kUnspecifiedAccount,
                       base::RandBytesAsString(128));
  RemoveKeychainAndVerify(Keychain::Key::PAIRING_INFO,
                          Keychain::kUnspecifiedAccount);
}

TEST_F(
    RemotingKeychainTest,
    AddAndRemoveTwoKeychainsWithSameAccountButDifferentKey_rightDataIsReturned) {
  std::string account = RandomBase64String(16);
  std::string data_1 = base::RandBytesAsString(128);
  std::string data_2 = base::RandBytesAsString(128);
  SetKeychainAndVerify(Keychain::Key::PAIRING_INFO, account, data_1);
  SetKeychainAndVerify(Keychain::Key::REFRESH_TOKEN, account, data_2);

  VerifyKeychain(Keychain::Key::PAIRING_INFO, account, data_1);
  VerifyKeychain(Keychain::Key::REFRESH_TOKEN, account, data_2);

  RemoveKeychainAndVerify(Keychain::Key::PAIRING_INFO, account);
  RemoveKeychainAndVerify(Keychain::Key::REFRESH_TOKEN, account);
}

TEST_F(
    RemotingKeychainTest,
    AddAndRemoveTwoKeychainsWithSameKeyButDifferentAccount_rightDataIsReturned) {
  std::string account_1 = RandomBase64String(16);
  std::string account_2 = RandomBase64String(16);
  std::string data_1 = base::RandBytesAsString(128);
  std::string data_2 = base::RandBytesAsString(128);
  SetKeychainAndVerify(Keychain::Key::PAIRING_INFO, account_1, data_1);
  SetKeychainAndVerify(Keychain::Key::PAIRING_INFO, account_2, data_2);

  VerifyKeychain(Keychain::Key::PAIRING_INFO, account_1, data_1);
  VerifyKeychain(Keychain::Key::PAIRING_INFO, account_2, data_2);

  RemoveKeychainAndVerify(Keychain::Key::PAIRING_INFO, account_1);
  RemoveKeychainAndVerify(Keychain::Key::PAIRING_INFO, account_2);
}

}  // namespace remoting
