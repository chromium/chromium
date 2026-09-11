// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_device_authorization_client.h"

#import <Security/Security.h>

#import <string>

#import "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_client.h"
#import "components/webauthn/core/browser/device_authorization/proto/device_authorization_key.pb.h"
#import "google_apis/gaia/gaia_id.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::base::apple::CFToNSPtrCast;
using ::base::apple::NSToCFPtrCast;
using ::testing::SizeIs;

constexpr std::string_view kTestGaiaId = "123456789012345678901";
constexpr std::string_view kTestKey = "test-key-data";
constexpr int kTestKeyVersion = 1;

void WipeAllDeviceAuthorizationKeys() {
  NSDictionary* query = @{
    CFToNSPtrCast(kSecClass) : CFToNSPtrCast(kSecClassGenericPassword),
    CFToNSPtrCast(kSecAttrService) :
        @"com.google.chrome.DeviceAuthorizationKey",
    CFToNSPtrCast(kSecAttrSynchronizable) : @NO,
  };
  SecItemDelete(NSToCFPtrCast(query));
}

}  // namespace

class IOSDeviceAuthorizationClientTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();
    WipeAllDeviceAuthorizationKeys();
  }

  void TearDown() override {
    WipeAllDeviceAuthorizationKeys();
    PlatformTest::TearDown();
  }

 protected:
  IOSDeviceAuthorizationClient client_;
};

// Tests that GetCachedKeys returns nullopt when no keys are in Keychain.
TEST_F(IOSDeviceAuthorizationClientTest, TestGetCachedKeysEmpty) {
  GaiaId gaia_id = GaiaId(std::string(kTestGaiaId));
  std::optional<webauthn::DeviceAuthorizationKeys> keys =
      client_.GetCachedKeys(gaia_id);
  EXPECT_FALSE(keys.has_value());
}

// Tests storing and retrieving keys from Keychain.
TEST_F(IOSDeviceAuthorizationClientTest, TestStoreAndGetCachedKeys) {
  GaiaId gaia_id = GaiaId(std::string(kTestGaiaId));
  webauthn::DeviceAuthorizationKeys keys_to_store;
  webauthn::DeviceAuthorizationKey* key = keys_to_store.add_keys();
  key->set_version(kTestKeyVersion);
  key->set_key(std::string(kTestKey));
  EXPECT_TRUE(client_.StoreKeys(gaia_id, keys_to_store));

  std::optional<webauthn::DeviceAuthorizationKeys> retrieved_keys =
      client_.GetCachedKeys(gaia_id);
  ASSERT_TRUE(retrieved_keys.has_value());
  ASSERT_THAT(retrieved_keys->keys(), SizeIs(1));
  EXPECT_EQ(retrieved_keys->keys(0).version(), kTestKeyVersion);
  EXPECT_EQ(retrieved_keys->keys(0).key(), kTestKey);
}
