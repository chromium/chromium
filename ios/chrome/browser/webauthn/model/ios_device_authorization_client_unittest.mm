// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/model/ios_device_authorization_client.h"

#import <Security/Security.h>

#import <optional>
#import <string>

#import "base/apple/bridging.h"
#import "base/apple/foundation_util.h"
#import "base/test/protobuf_matchers.h"
#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "components/webauthn/core/browser/device_authorization/device_authorization_client.h"
#import "components/webauthn/core/browser/device_authorization/proto/device_authorization_key.pb.h"
#import "components/webauthn/core/browser/device_authorization/proto/device_authorization_local_storage.pb.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::base::apple::CFToNSPtrCast;
using ::base::apple::NSToCFPtrCast;
using ::base::test::EqualsProto;
using ::base::test::TestFuture;

constexpr std::string_view kTestGaiaId = "123456789012345678901";
constexpr std::string_view kTestKey = "test-key-data";
constexpr int kTestKeyVersion = 1;
constexpr int kTestCacheVersion = 1;

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
  base::test::TaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  IOSDeviceAuthorizationClient client_;
};

// Tests that GetCachedKeys returns nullopt when no keys are in Keychain.
TEST_F(IOSDeviceAuthorizationClientTest, TestGetCachedKeysEmpty) {
  GaiaId gaia_id = GaiaId(std::string(kTestGaiaId));
  TestFuture<std::optional<webauthn::CachedDeviceAuthorizationKeys>> future;
  client_.GetCachedKeys(gaia_id, future.GetCallback());
  EXPECT_FALSE(future.Get().has_value());
}

// Tests storing and retrieving keys from Keychain.
TEST_F(IOSDeviceAuthorizationClientTest, TestStoreAndGetCachedKeys) {
  GaiaId gaia_id = GaiaId(std::string(kTestGaiaId));
  webauthn::CachedDeviceAuthorizationKeys keys_to_store;
  keys_to_store.set_cache_version(kTestCacheVersion);
  webauthn::DeviceAuthorizationKey* key =
      keys_to_store.mutable_keys()->add_keys();
  key->set_version(kTestKeyVersion);
  key->set_key(std::string(kTestKey));
  TestFuture<bool> store_future;
  client_.StoreKeys(gaia_id, keys_to_store, store_future.GetCallback());
  EXPECT_TRUE(store_future.Get());

  TestFuture<std::optional<webauthn::CachedDeviceAuthorizationKeys>> get_future;
  client_.GetCachedKeys(gaia_id, get_future.GetCallback());
  std::optional<webauthn::CachedDeviceAuthorizationKeys> retrieved =
      get_future.Take();
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_THAT(*retrieved, EqualsProto(keys_to_store));
}
