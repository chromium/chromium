// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/device_reauth/ios_device_authenticator.h"

#import "base/run_loop.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "base/test/mock_callback.h"
#import "base/test/task_environment.h"
#import "base/test/test.pb.h"
#import "ios/chrome/test/app/mock_reauthentication_module.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

// Test class to ensure the IOSDeviceAuthenticator works correctly.
class IOSDeviceAuthenticatorTest : public PlatformTest {
 public:
  IOSDeviceAuthenticatorTest()
      : mock_reauth_module_([[MockReauthenticationModule alloc] init]) {
    authenticator_ = std::make_unique<IOSDeviceAuthenticator>(
        mock_reauth_module_, &proxy_,
        device_reauth::DeviceAuthParams(
            base::Seconds(60), device_reauth::DeviceAuthSource::kAutofill));
    mock_reauth_module_.shouldSkipReAuth = YES;
    mock_reauth_module_.canAttemptWithBiometrics = YES;
    mock_reauth_module_.canAttempt = YES;
  }

  void SimulateReauthSucceeded() {
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSuccess;
  }
  void SimulateReauthFailed() {
    mock_reauth_module_.expectedResult = ReauthenticationResult::kFailure;
  }
  void SimulateReauthBypassed() {
    mock_reauth_module_.expectedResult = ReauthenticationResult::kSkipped;
  }

  device_reauth::DeviceAuthenticator* authenticator() {
    return authenticator_.get();
  }
  base::MockCallback<base::OnceCallback<void(bool)>>& result_callback() {
    return result_callback_;
  }

 protected:
  MockReauthenticationModule* mock_reauth_module_;

 private:
  DeviceAuthenticatorProxy proxy_;
  std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator_;
  base::MockCallback<base::OnceCallback<void(bool)>> result_callback_;
};

// If the time that passed since the last successful authentication is smaller
// than the auth valid period of time, no reauthentication is needed.
TEST_F(IOSDeviceAuthenticatorTest, SkipReauthIfLessThanAuthValidPeriod) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SimulateReauthSucceeded();

  EXPECT_CALL(result_callback(), Run(/*success=*/true));
  base::RunLoop run_loop;
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"dummy message",
      result_callback().Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  SimulateReauthBypassed();

  EXPECT_CALL(result_callback(), Run(/*success=*/true));
  base::RunLoop run_loop2;
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"dummy message",
      result_callback().Get().Then(run_loop2.QuitClosure()));
  run_loop2.Run();
}

// If the time that passed since the last successful authentication is larger
// than the auth valid period of time, another reauthentication is needed (no
// ReauthenticationResult::kSkipped returned from the ReauthenticationProtocol).
TEST_F(IOSDeviceAuthenticatorTest, DoReauthIfMoreThanAuthValidPeriod) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SimulateReauthSucceeded();

  EXPECT_CALL(result_callback(), Run(/*success=*/true));
  base::RunLoop run_loop;
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"dummy message",
      result_callback().Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  SimulateReauthFailed();

  EXPECT_CALL(result_callback(), Run(/*success=*/false));
  base::RunLoop run_loop2;
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"dummy message",
      result_callback().Get().Then(run_loop2.QuitClosure()));
  run_loop2.Run();
}

// If previous auth failed, another reauthentication is needed anyway.
TEST_F(IOSDeviceAuthenticatorTest, DoReauthIfPreviousFailure) {
  base::test::SingleThreadTaskEnvironment task_environment;
  SimulateReauthFailed();

  EXPECT_CALL(result_callback(), Run(/*success=*/false));
  base::RunLoop run_loop;
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"dummy message",
      result_callback().Get().Then(run_loop.QuitClosure()));
  run_loop.Run();

  SimulateReauthSucceeded();

  EXPECT_CALL(result_callback(), Run(/*success=*/true));
  base::RunLoop run_loop2;
  authenticator()->AuthenticateWithMessage(
      /*message=*/u"dummy message",
      result_callback().Get().Then(run_loop2.QuitClosure()));
  run_loop2.Run();
}

// Param of the IOSDeviceAuthenticatorAvailabilityTest:
// -- bool whether biometric authentication is available;
// -- bool whether screen lock authentication is available.
class IOSDeviceAuthenticatorAvailabilityTest
    : public IOSDeviceAuthenticatorTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  IOSDeviceAuthenticatorAvailabilityTest() {
    mock_reauth_module_.canAttemptWithBiometrics = BiometricAvailable();
    mock_reauth_module_.canAttempt =
        BiometricAvailable() || ScreenLockAvailable();
  }

  bool BiometricAvailable() { return std::get<0>(GetParam()); }
  bool ScreenLockAvailable() { return std::get<1>(GetParam()); }
};

// Tests that auth availability is correctly returned.
TEST_P(IOSDeviceAuthenticatorAvailabilityTest, ReauthAvailability) {
  EXPECT_EQ(authenticator()->CanAuthenticateWithBiometrics(),
            BiometricAvailable());
  EXPECT_EQ(authenticator()->CanAuthenticateWithBiometricOrScreenLock(),
            BiometricAvailable() || ScreenLockAvailable());
}

INSTANTIATE_TEST_SUITE_P(,
                         IOSDeviceAuthenticatorAvailabilityTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));
