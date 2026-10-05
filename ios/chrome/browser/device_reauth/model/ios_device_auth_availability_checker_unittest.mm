// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/model/ios_device_auth_availability_checker.h"

#import <memory>

#import "ios/chrome/common/ui/reauthentication/mock_reauthentication_module.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class IOSDeviceAuthAvailabilityCheckerTest : public PlatformTest {
 public:
  IOSDeviceAuthAvailabilityCheckerTest() {
    mock_reauth_module_ = [[MockReauthenticationModule alloc] init];
    checker_ =
        std::make_unique<IOSDeviceAuthAvailabilityChecker>(mock_reauth_module_);
  }

  IOSDeviceAuthAvailabilityChecker* checker() { return checker_.get(); }
  MockReauthenticationModule* mock_reauth_module() {
    return mock_reauth_module_;
  }

 private:
  MockReauthenticationModule* mock_reauth_module_;
  std::unique_ptr<IOSDeviceAuthAvailabilityChecker> checker_;
};

TEST_F(IOSDeviceAuthAvailabilityCheckerTest, BiometricsAvailable) {
  mock_reauth_module().canAttemptWithBiometrics = YES;
  mock_reauth_module().canAttempt = YES;

  EXPECT_TRUE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_TRUE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

TEST_F(IOSDeviceAuthAvailabilityCheckerTest,
       BiometricsUnavailableScreenLockAvailable) {
  mock_reauth_module().canAttemptWithBiometrics = NO;
  mock_reauth_module().canAttempt = YES;

  EXPECT_FALSE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_TRUE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

TEST_F(IOSDeviceAuthAvailabilityCheckerTest, NeitherAvailable) {
  mock_reauth_module().canAttemptWithBiometrics = NO;
  mock_reauth_module().canAttempt = NO;

  EXPECT_FALSE(checker()->CanAuthenticateWithBiometrics());
  EXPECT_FALSE(checker()->CanAuthenticateWithBiometricOrScreenLock());
}

}  // namespace
