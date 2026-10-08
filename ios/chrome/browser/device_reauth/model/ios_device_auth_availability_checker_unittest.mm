// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/ios_device_auth_availability_checker.h"

#import <memory>

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

}  // namespace
