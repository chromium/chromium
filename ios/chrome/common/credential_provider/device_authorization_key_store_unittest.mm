// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/device_authorization_key_store.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#import <string>
#import <utility>
#import <vector>

#import "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#import "base/apple/scoped_cftyperef.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/protobuf_matchers.h"
#import "components/webauthn/core/browser/device_authorization/proto/device_authorization_key.pb.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using ::base::apple::CFToNSPtrCast;
using ::base::apple::NSToCFOwnershipCast;
using ::base::test::EqualsProto;

constexpr char kGaiaId1[] = "123456789012345678901";
constexpr char kGaiaId2[] = "987654321098765432109";
constexpr char kTestKey1[] = "device-auth-key-version-1";
constexpr char kTestKey2[] = "device-auth-key-version-2";
constexpr char kTestKey3[] = "device-auth-key-version-3";
constexpr int kKeyVersion1 = 1;
constexpr int kKeyVersion2 = 2;
constexpr int kKeyVersion3 = 3;

DeviceAuthorizationKeys CreateTestKeys(
    const std::vector<std::pair<int, std::string>>& version_key_pairs) {
  DeviceAuthorizationKeys keys;
  for (const std::pair<int, std::string>& version_key_pair :
       version_key_pairs) {
    DeviceAuthorizationKey* key = keys.add_keys();
    key->set_version(version_key_pair.first);
    key->set_key(version_key_pair.second);
  }
  return keys;
}

// Deletes all device authorization keys stored in the Keychain.
void WipeAllDeviceAuthorizationKeys() {
  NSDictionary* query = @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrService) :
        @"com.google.chrome.DeviceAuthorizationKey",
    CFToNSPtrCast(kSecAttrSynchronizable) : @NO,
  };
  SecItemDelete(NSToCFOwnershipCast(query));
}

class DeviceAuthorizationKeyStoreTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    WipeAllDeviceAuthorizationKeys();
  }

  void TearDown() override {
    WipeAllDeviceAuthorizationKeys();
    PlatformTest::TearDown();
  }
};

// Tests storing and successfully fetching a single device authorization key.
TEST_F(DeviceAuthorizationKeyStoreTest, StoreAndFetchSingleKeySucceeds) {
  DeviceAuthorizationKeys keys = CreateTestKeys({{kKeyVersion1, kTestKey1}});
  EXPECT_TRUE(StoreDeviceAuthorizationKeys(kGaiaId1, keys));

  std::optional<DeviceAuthorizationKeys> fetched_keys =
      GetDeviceAuthorizationKeys(kGaiaId1);
  ASSERT_TRUE(fetched_keys.has_value());
  EXPECT_THAT(*fetched_keys, EqualsProto(keys));
}

// Tests storing and fetching multiple key versions in a batch.
TEST_F(DeviceAuthorizationKeyStoreTest, StoreAndFetchMultipleKeysInBatch) {
  DeviceAuthorizationKeys keys = CreateTestKeys({{kKeyVersion1, kTestKey1},
                                                 {kKeyVersion2, kTestKey2},
                                                 {kKeyVersion3, kTestKey3}});
  EXPECT_TRUE(StoreDeviceAuthorizationKeys(kGaiaId1, keys));

  std::optional<DeviceAuthorizationKeys> fetched_keys =
      GetDeviceAuthorizationKeys(kGaiaId1);
  ASSERT_TRUE(fetched_keys.has_value());
  EXPECT_THAT(*fetched_keys, EqualsProto(keys));
}

// Tests multi-profile isolation partitioned by Gaia ID.
TEST_F(DeviceAuthorizationKeyStoreTest, IsolatesKeysByGaiaId) {
  DeviceAuthorizationKeys keys_account_1 =
      CreateTestKeys({{kKeyVersion1, kTestKey1}});
  DeviceAuthorizationKeys keys_account_2 =
      CreateTestKeys({{kKeyVersion2, kTestKey2}});

  // Store keys for account 1.
  EXPECT_TRUE(StoreDeviceAuthorizationKeys(kGaiaId1, keys_account_1));

  // Account 2 should have no keys initially.
  EXPECT_FALSE(GetDeviceAuthorizationKeys(kGaiaId2).has_value());

  // Store keys for account 2.
  EXPECT_TRUE(StoreDeviceAuthorizationKeys(kGaiaId2, keys_account_2));

  // Both accounts should return their respective distinct keys.
  std::optional<DeviceAuthorizationKeys> fetched_account_1 =
      GetDeviceAuthorizationKeys(kGaiaId1);
  ASSERT_TRUE(fetched_account_1.has_value());
  EXPECT_THAT(*fetched_account_1, EqualsProto(keys_account_1));

  std::optional<DeviceAuthorizationKeys> fetched_account_2 =
      GetDeviceAuthorizationKeys(kGaiaId2);
  ASSERT_TRUE(fetched_account_2.has_value());
  EXPECT_THAT(*fetched_account_2, EqualsProto(keys_account_2));
}

// Tests overwriting/updating existing keys for the same account.
TEST_F(DeviceAuthorizationKeyStoreTest, OverwritesExistingKeysForAccount) {
  DeviceAuthorizationKeys initial_keys =
      CreateTestKeys({{kKeyVersion1, kTestKey1}});
  EXPECT_TRUE(StoreDeviceAuthorizationKeys(kGaiaId1, initial_keys));

  std::optional<DeviceAuthorizationKeys> fetched_initial =
      GetDeviceAuthorizationKeys(kGaiaId1);
  ASSERT_TRUE(fetched_initial.has_value());
  EXPECT_THAT(*fetched_initial, EqualsProto(initial_keys));

  // Overwrite with a new set of keys.
  DeviceAuthorizationKeys updated_keys =
      CreateTestKeys({{kKeyVersion2, kTestKey2}, {kKeyVersion3, kTestKey3}});
  EXPECT_TRUE(StoreDeviceAuthorizationKeys(kGaiaId1, updated_keys));

  std::optional<DeviceAuthorizationKeys> fetched_updated =
      GetDeviceAuthorizationKeys(kGaiaId1);
  ASSERT_TRUE(fetched_updated.has_value());
  EXPECT_THAT(*fetched_updated, EqualsProto(updated_keys));
}

// Tests edge cases with empty and invalid arguments.
TEST_F(DeviceAuthorizationKeyStoreTest, RejectsEmptyAndInvalidArguments) {
  DeviceAuthorizationKeys valid_keys =
      CreateTestKeys({{kKeyVersion1, kTestKey1}});
  DeviceAuthorizationKeys empty_keys;
  DeviceAuthorizationKeys empty_key_bytes =
      CreateTestKeys({{kKeyVersion1, ""}});
  DeviceAuthorizationKeys negative_version = CreateTestKeys({{-1, kTestKey1}});

  EXPECT_FALSE(StoreDeviceAuthorizationKeys("", valid_keys));
  EXPECT_FALSE(StoreDeviceAuthorizationKeys(kGaiaId1, empty_keys));
  EXPECT_FALSE(StoreDeviceAuthorizationKeys(kGaiaId1, empty_key_bytes));
  EXPECT_FALSE(StoreDeviceAuthorizationKeys(kGaiaId1, negative_version));
  EXPECT_FALSE(GetDeviceAuthorizationKeys("").has_value());
}

// Tests that Keychain query attributes (synchronizable, accessible, service,
// account) are correctly applied to the stored item.
TEST_F(DeviceAuthorizationKeyStoreTest, AppliesExpectedKeychainAttributes) {
  DeviceAuthorizationKeys keys = CreateTestKeys({{kKeyVersion1, kTestKey1}});
  EXPECT_TRUE(StoreDeviceAuthorizationKeys(kGaiaId1, keys));

  // Query raw attributes from the iOS Keychain.
  NSString* expected_account = base::SysUTF8ToNSString(kGaiaId1);
  NSMutableDictionary* query = [NSMutableDictionary dictionaryWithDictionary:@{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrService) :
        @"com.google.chrome.DeviceAuthorizationKey",
    CFToNSPtrCast(kSecAttrAccount) : expected_account,
    CFToNSPtrCast(kSecReturnAttributes) : @YES,
    CFToNSPtrCast(kSecMatchLimit) : CFToNSPtrCast(kSecMatchLimitOne),
    CFToNSPtrCast(kSecAttrSynchronizable) : @NO,
  }];

  base::apple::ScopedCFTypeRef<CFTypeRef> result;
  OSStatus status =
      SecItemCopyMatching(NSToCFOwnershipCast(query), result.InitializeInto());
  ASSERT_EQ(status, errSecSuccess);
  ASSERT_TRUE(result);

  NSDictionary* dict =
      CFToNSPtrCast(base::apple::CFCast<CFDictionaryRef>(result.get()));
  ASSERT_TRUE(dict);

  EXPECT_NSEQ(dict[CFToNSPtrCast(kSecAttrAccount)], expected_account);
  EXPECT_NSEQ(dict[CFToNSPtrCast(kSecAttrService)],
              @"com.google.chrome.DeviceAuthorizationKey");
  EXPECT_NSEQ(dict[CFToNSPtrCast(kSecAttrAccessible)],
              CFToNSPtrCast(kSecAttrAccessibleWhenUnlockedThisDeviceOnly));
  if (dict[CFToNSPtrCast(kSecAttrSynchronizable)]) {
    EXPECT_NSEQ(dict[CFToNSPtrCast(kSecAttrSynchronizable)], @NO);
  }
}

}  // namespace
