// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/connectors/device_trust/model/device_trust_key_manager_ios.h"

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using KeyRotationResult =
    enterprise_connectors::DeviceTrustKeyManager::KeyRotationResult;

}  // namespace

class DeviceTrustKeyManagerIOSTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_;
  DeviceTrustKeyManagerIOS key_manager_;
};

// Tests that `StartInitialization` is a safe no-op on iOS and leaves the key
// manager initialized with no loaded key metadata or permanent failures.
TEST_F(DeviceTrustKeyManagerIOSTest, StartInitialization) {
  key_manager_.StartInitialization();
  EXPECT_FALSE(key_manager_.HasPermanentFailure());
  EXPECT_EQ(key_manager_.GetLoadedKeyMetadata(), std::nullopt);
}

// Tests that `RotateKey` invokes its callback asynchronously with `FAILURE`
// since key rotation is not supported on iOS in the unsigned attestation phase.
TEST_F(DeviceTrustKeyManagerIOSTest, RotateKeyReturnsFailure) {
  base::test::TestFuture<KeyRotationResult> future;
  key_manager_.RotateKey("test_nonce", future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), KeyRotationResult::FAILURE);
}

// Tests that `ExportPublicKeyAsync` completes asynchronously and returns
// `nullopt` because no browser signing key is provisioned.
TEST_F(DeviceTrustKeyManagerIOSTest, ExportPublicKeyAsyncReturnsNullopt) {
  base::test::TestFuture<std::optional<std::string>> future;
  key_manager_.ExportPublicKeyAsync(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `SignStringAsync` completes asynchronously with `nullopt`,
// signaling to the attestation pipeline that an unsigned challenge response
// (`kSuccessNoSignature`) should be generated.
TEST_F(DeviceTrustKeyManagerIOSTest,
       SignStringAsyncReturnsNulloptForUnsignedResponse) {
  base::test::TestFuture<std::optional<std::vector<uint8_t>>> future;
  key_manager_.SignStringAsync("test_challenge_payload", future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that `HasPermanentFailure` returns false by default.
TEST_F(DeviceTrustKeyManagerIOSTest, HasPermanentFailureReturnsFalse) {
  EXPECT_FALSE(key_manager_.HasPermanentFailure());
}
