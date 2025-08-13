// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "crypto/apple/keychain_secitem.h"

#import <Foundation/Foundation.h>

#import <array>

#import "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#import "base/apple/scoped_cftyperef.h"
#import "base/containers/span.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "crypto/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using base::apple::CFToNSPtrCast;

// Use unique names for the test item to avoid conflicts.
const char kTestServiceName[] = "KeychainAccessibilityMigrationTest.Service";
const char kTestAccountName[] = "KeychainAccessibilityMigrationTest.Account";
const auto kTestPassword =
    std::to_array<uint8_t>({'s', 'e', 'c', 'r', 'e', 't'});

}  // namespace

class KeychainAccessibilityMigrationIOSTest : public PlatformTest {
 public:
  KeychainAccessibilityMigrationIOSTest() = default;

  void SetUp() override {
    PlatformTest::SetUp();

    // Get current test name to distinguish PRE_ phase.
    const std::string& test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();

    if (test_name.rfind("PRE_", 0) == 0) {
      feature_list_.InitAndDisableFeature(
          crypto::features::kMigrateIOSKeychainAccessibility);
    } else {
      feature_list_.InitAndEnableFeature(
          crypto::features::kMigrateIOSKeychainAccessibility);
    }
  }

  static void TearDownTestSuite() {
    // Clean up the keychain item after all tests in the suite have run.
    DeleteGenericPasswordForTest(kTestServiceName, kTestAccountName);
  }

 protected:
  // Gets the accessibility attribute of the test keychain item.
  base::expected<std::string, OSStatus> GetKeychainItemAccessibility() {
    NSDictionary* query = @{
      CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
      CFToNSPtrCast(kSecAttrService) :
          base::SysUTF8ToNSString(kTestServiceName),
      CFToNSPtrCast(kSecAttrAccount) :
          base::SysUTF8ToNSString(kTestAccountName),
      CFToNSPtrCast(kSecReturnAttributes) : @YES,
    };
    base::apple::ScopedCFTypeRef<CFTypeRef> result;
    OSStatus status = SecItemCopyMatching(base::apple::NSToCFPtrCast(query),
                                          result.InitializeInto());
    if (status != noErr) {
      return base::unexpected(status);
    }
    CFDictionaryRef result_dict =
        base::apple::CFCast<CFDictionaryRef>(result.get());
    return base::SysCFStringRefToUTF8(
        base::apple::GetValueFromDictionary<CFStringRef>(result_dict,
                                                         kSecAttrAccessible));
  }

  static void DeleteGenericPasswordForTest(std::string_view service_name,
                                           std::string_view account_name) {
    NSDictionary* query = @{
      CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
      CFToNSPtrCast(kSecAttrService) : base::SysUTF8ToNSString(service_name),
      CFToNSPtrCast(kSecAttrAccount) : base::SysUTF8ToNSString(account_name),
    };
    SecItemDelete(base::apple::NSToCFPtrCast(query));
  }

  crypto::apple::KeychainSecItem keychain_;
  base::test::ScopedFeatureList feature_list_;
};

// Phase 1: Add keychain item with kMigrateIOSKeychainAccessibility disabled.
// The item will be stored using kSecAttrAccessibleWhenUnlocked.
TEST_F(KeychainAccessibilityMigrationIOSTest, PRE_MigrateItem) {
  // Verify that the keychain item doesn't already exist. This is a check
  // that the test suite's cleanup is working correctly.
  base::expected<std::vector<uint8_t>, OSStatus> result_before =
      keychain_.FindGenericPassword(kTestServiceName, kTestAccountName);
  ASSERT_FALSE(result_before.has_value())
      << "Keychain item existed before test. Prior test state not cleaned up.";

  // Verify feature is disabled as configured in SetUp.
  ASSERT_FALSE(base::FeatureList::IsEnabled(
      crypto::features::kMigrateIOSKeychainAccessibility));

  // Add a password to the keychain.
  OSStatus status = keychain_.AddGenericPassword(
      kTestServiceName, kTestAccountName, kTestPassword);
  ASSERT_EQ(status, noErr)
      << "PRE_MigrateItem: AddGenericPassword failed with OSStatus " << status;

  // Sanity check: ensure the password can be retrieved immediately.
  base::expected<std::vector<uint8_t>, OSStatus> result =
      keychain_.FindGenericPassword(kTestServiceName, kTestAccountName);
  ASSERT_TRUE(result.has_value())
      << "PRE_MigrateItem: FindGenericPassword failed. OSStatus: "
      << result.error();
  EXPECT_EQ(base::as_byte_span(result.value()),
            base::as_byte_span(kTestPassword));
}

// Phase 2: Read keychain item with kMigrateIOSKeychainAccessibility enabled.
// This should find the item and trigger an update of its accessibility
// attribute to kSecAttrAccessibleAfterFirstUnlock within FindGenericPassword.
TEST_F(KeychainAccessibilityMigrationIOSTest, MigrateItem) {
  // Verify feature is enabled as configured in SetUp.
  ASSERT_TRUE(base::FeatureList::IsEnabled(
      crypto::features::kMigrateIOSKeychainAccessibility));

  // Check that the accessibility is `kSecAttrAccessibleWhenUnlocked` before
  // migration.
  base::expected<std::string, OSStatus> accessibility_before =
      GetKeychainItemAccessibility();
  ASSERT_TRUE(accessibility_before.has_value());
  EXPECT_EQ(accessibility_before.value(),
            base::SysCFStringRefToUTF8(kSecAttrAccessibleWhenUnlocked));

  // Attempt to find the password. This call should succeed and trigger the
  // migration.
  base::expected<std::vector<uint8_t>, OSStatus> result =
      keychain_.FindGenericPassword(kTestServiceName, kTestAccountName);
  ASSERT_TRUE(result.has_value())
      << "MigrateItem: FindGenericPassword failed. OSStatus: "
      << result.error();
  EXPECT_EQ(base::as_byte_span(result.value()),
            base::as_byte_span(kTestPassword));

  // Check that the accessibility is `kSecAttrAccessibleAfterFirstUnlock` after
  // migration.
  base::expected<std::string, OSStatus> accessibility_after =
      GetKeychainItemAccessibility();
  ASSERT_TRUE(accessibility_after.has_value());
  EXPECT_EQ(accessibility_after.value(),
            base::SysCFStringRefToUTF8(kSecAttrAccessibleAfterFirstUnlock));
}
