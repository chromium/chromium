// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/apple/keychain_util.h"

#import <Foundation/Foundation.h>

#include "base/apple/bridging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "crypto/apple/fake_keychain_v2.h"
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";

class AppleKeychainUtilTest : public testing::Test {
 protected:
  crypto::apple::ScopedFakeKeychainV2 scoped_fake_keychain_{
      kTestKeychainAccessGroup};
};

#if !BUILDFLAG(IS_IOS)
TEST_F(AppleKeychainUtilTest, ExecutableHasKeychainAccessGroupEntitlement) {
  EXPECT_TRUE(crypto::apple::ExecutableHasKeychainAccessGroupEntitlement(
      kTestKeychainAccessGroup));
  EXPECT_FALSE(crypto::apple::ExecutableHasKeychainAccessGroupEntitlement(
      "some-other-keychain-access-group"));
}
#endif  // !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_IOS)
TEST_F(AppleKeychainUtilTest, MigrateKeychainItemAccessibilityIfNeeded) {
  NSDictionary* query = @{
    base::apple::CFToNSPtrCast(kSecClass) :
        base::apple::CFToNSPtrCast(kSecClassGenericPassword),
    base::apple::CFToNSPtrCast(kSecAttrAccount) : @"test-account",
    base::apple::CFToNSPtrCast(kSecAttrAccessGroup) :
        base::SysUTF8ToNSString(kTestKeychainAccessGroup),
  };

  // Add an item to the fake keychain.
  {
    NSDictionary* attributes = @{
      base::apple::CFToNSPtrCast(kSecClass) :
          base::apple::CFToNSPtrCast(kSecClassGenericPassword),
      base::apple::CFToNSPtrCast(kSecAttrAccount) : @"test-account",
      base::apple::CFToNSPtrCast(kSecAttrAccessGroup) :
          base::SysUTF8ToNSString(kTestKeychainAccessGroup),
      base::apple::CFToNSPtrCast(kSecAttrAccessible) :
          base::apple::CFToNSPtrCast(kSecAttrAccessibleWhenUnlocked),
    };
    EXPECT_EQ(scoped_fake_keychain_.keychain()->ItemAdd(
                  base::apple::NSToCFPtrCast(attributes), nullptr),
              noErr);
  }

  // Case 1: Migration needed and successful.
  {
    NSDictionary* attributes = @{
      base::apple::CFToNSPtrCast(kSecAttrAccessible) :
          base::apple::CFToNSPtrCast(kSecAttrAccessibleWhenUnlocked),
    };
    base::HistogramTester histogram_tester;

    EXPECT_TRUE(crypto::apple::MigrateKeychainItemAccessibilityIfNeeded(
        base::apple::NSToCFPtrCast(attributes),
        base::apple::NSToCFPtrCast(query)));
    histogram_tester.ExpectBucketCount("Security.iOS.KeychainMigration.Result",
                                       1, 1);
  }

  // Case 2: Migration needed and failed.
  {
    NSDictionary* attributes = @{
      base::apple::CFToNSPtrCast(kSecAttrAccessible) :
          base::apple::CFToNSPtrCast(kSecAttrAccessibleWhenUnlocked),
    };
    base::HistogramTester histogram_tester;

    scoped_fake_keychain_.keychain()->set_item_update_result(
        errSecInteractionNotAllowed);

    EXPECT_TRUE(crypto::apple::MigrateKeychainItemAccessibilityIfNeeded(
        base::apple::NSToCFPtrCast(attributes),
        base::apple::NSToCFPtrCast(query)));
    histogram_tester.ExpectBucketCount("Security.iOS.KeychainMigration.Result",
                                       2, 1);

    // Reset for next case.
    scoped_fake_keychain_.keychain()->set_item_update_result(noErr);
  }

  // Case 3: Migration not needed.
  {
    NSDictionary* attributes = @{
      base::apple::CFToNSPtrCast(kSecAttrAccessible) :
          base::apple::CFToNSPtrCast(kSecAttrAccessibleAfterFirstUnlock),
    };
    base::HistogramTester histogram_tester;

    EXPECT_FALSE(crypto::apple::MigrateKeychainItemAccessibilityIfNeeded(
        base::apple::NSToCFPtrCast(attributes),
        base::apple::NSToCFPtrCast(query)));
    histogram_tester.ExpectBucketCount("Security.iOS.KeychainMigration.Result",
                                       0, 1);
  }
}

#endif  // BUILDFLAG(IS_IOS)

}  // namespace
