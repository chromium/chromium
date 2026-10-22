// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/ios_device_auth_availability_checker.h"

#import <memory>

#import "base/test/task_environment.h"
#import "base/test/test_future.h"
#import "ios/chrome/common/ui/reauthentication/fake_reauthentication_module.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class IOSDeviceAuthAvailabilityCheckerTest : public PlatformTest {
 public:
  IOSDeviceAuthAvailabilityCheckerTest() {
    fake_reauth_module_ = [[FakeReauthenticationModule alloc] init];
    checker_ =
        std::make_unique<IOSDeviceAuthAvailabilityChecker>(fake_reauth_module_);
  }

  IOSDeviceAuthAvailabilityChecker* checker() { return checker_.get(); }
  FakeReauthenticationModule* fake_reauth_module() {
    return fake_reauth_module_;
  }

 private:
  FakeReauthenticationModule* fake_reauth_module_;
  std::unique_ptr<IOSDeviceAuthAvailabilityChecker> checker_;
};

TEST_F(IOSDeviceAuthAvailabilityCheckerTest, BiometricsAvailable) {
  fake_reauth_module().canAttemptWithBiometrics = YES;
  fake_reauth_module().canAttempt = YES;

  EXPECT_TRUE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_TRUE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

TEST_F(IOSDeviceAuthAvailabilityCheckerTest,
       BiometricsUnavailableScreenLockAvailable) {
  fake_reauth_module().canAttemptWithBiometrics = NO;
  fake_reauth_module().canAttempt = YES;

  EXPECT_FALSE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_TRUE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

TEST_F(IOSDeviceAuthAvailabilityCheckerTest, NeitherAvailable) {
  fake_reauth_module().canAttemptWithBiometrics = NO;
  fake_reauth_module().canAttempt = NO;

  EXPECT_FALSE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_FALSE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

// Tests that async auth availability is correctly returned.
TEST_F(IOSDeviceAuthAvailabilityCheckerTest, ReauthAvailabilityAsync) {
  base::test::TaskEnvironment task_environment;

  fake_reauth_module().canAttempt = YES;
  base::test::TestFuture<bool> future_available;
  checker()->CanAuthenticateWithBiometricOrScreenLock(
      future_available.GetCallback());
  EXPECT_TRUE(future_available.Get());

  fake_reauth_module().canAttempt = NO;
  base::test::TestFuture<bool> future_unavailable;
  checker()->CanAuthenticateWithBiometricOrScreenLock(
      future_unavailable.GetCallback());
  EXPECT_FALSE(future_unavailable.Get());
}

}  // namespace
