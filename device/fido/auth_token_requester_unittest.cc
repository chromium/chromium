// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/auth_token_requester.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/pin.h"
#include "device/fido/virtual_ctap2_device.h"

namespace device {
namespace {

using ::testing::ElementsAreArray;

using ClientPinAvailability =
    device::AuthenticatorSupportedOptions::ClientPinAvailability;
using UserVerificationAvailability =
    device::AuthenticatorSupportedOptions::UserVerificationAvailability;

constexpr char kTestPIN[] = "1234";

class TestAuthTokenRequesterDelegate : public AuthTokenRequester::Delegate {
 public:
  explicit TestAuthTokenRequesterDelegate(std::string pin)
      : pin_(std::move(pin)) {}

  void WaitForResult() { wait_for_result_loop_.Run(); }
  base::Optional<AuthTokenRequester::Result>& result() { return result_; }
  base::Optional<pin::TokenResponse>& response() { return response_; }
  bool pin_was_set() { return pin_was_set_; }
  bool pin_was_collected() { return pin_was_collected_; }
  bool internal_uv_was_retried() { return internal_uv_num_retries_ > 0u; }
  size_t internal_uv_num_retries() { return internal_uv_num_retries_; }
  bool internal_uv_was_locked() { return internal_uv_was_locked_; }

 private:
  // AuthTokenRequester::Delegate:
  void AuthenticatorSelectedForPINUVAuthToken(
      FidoAuthenticator* authenticator) override {}
  void CollectNewPIN(ProvidePINCallback provide_pin_cb) override {
    DCHECK(!pin_.empty());
    pin_was_set_ = true;
    std::move(provide_pin_cb).Run(pin_);
  }
  void CollectExistingPIN(int attempts,
                          ProvidePINCallback provide_pin_cb) override {
    DCHECK(!pin_.empty());
    pin_was_collected_ = true;
    std::move(provide_pin_cb).Run(pin_);
  }
  void PromptForInternalUVRetry(int attempts) override {
    internal_uv_num_retries_++;
  }
  void InternalUVLockedForAuthToken() override {
    internal_uv_was_locked_ = true;
  }
  void HavePINUVAuthTokenResultForAuthenticator(
      FidoAuthenticator* authenticator,
      AuthTokenRequester::Result result,
      base::Optional<pin::TokenResponse> response) override {
    DCHECK(!result_);
    result_ = result;
    response_ = std::move(response);
    wait_for_result_loop_.Quit();
  }

  std::string pin_;

  base::Optional<AuthTokenRequester::Result> result_;
  base::Optional<pin::TokenResponse> response_;

  bool pin_was_collected_ = false;
  bool pin_was_set_ = false;
  size_t internal_uv_num_retries_ = 0u;
  bool internal_uv_was_locked_ = false;

  base::RunLoop wait_for_result_loop_;
};

struct TestCase {
  ClientPinAvailability client_pin;
  UserVerificationAvailability user_verification;
  bool success;
};

class AuthTokenRequesterTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void RunTestCase(VirtualCtap2Device::Config config,
                   scoped_refptr<VirtualFidoDevice::State> state,
                   const TestCase& test_case) {
    state_ = state;

    switch (test_case.client_pin) {
      case ClientPinAvailability::kNotSupported:
        config.pin_support = false;
        break;
      case ClientPinAvailability::kSupportedButPinNotSet:
        config.pin_support = true;
        break;
      case ClientPinAvailability::kSupportedAndPinSet:
        config.pin_support = true;
        state_->pin = kTestPIN;
        break;
    }
    switch (test_case.user_verification) {
      case UserVerificationAvailability::kNotSupported:
        config.internal_uv_support = false;
        break;
      case UserVerificationAvailability::kSupportedButNotConfigured:
        config.internal_uv_support = true;
        break;
      case UserVerificationAvailability::kSupportedAndConfigured:
        config.internal_uv_support = true;
        state_->fingerprints_enrolled = true;
        break;
    }

    auto authenticator = std::make_unique<FidoDeviceAuthenticator>(
        std::make_unique<VirtualCtap2Device>(state_, std::move(config)));

    base::RunLoop init_loop;
    authenticator->InitializeAuthenticator(init_loop.QuitClosure());
    init_loop.Run();

    delegate_ = std::make_unique<TestAuthTokenRequesterDelegate>(kTestPIN);
    AuthTokenRequester::Options options;
    options.token_permissions = {pin::Permissions::kMakeCredential};
    options.rp_id = "foobar.com";
    AuthTokenRequester requester(delegate_.get(), authenticator.get(),
                                 std::move(options));
    requester.ObtainPINUVAuthToken();
    delegate_->WaitForResult();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<VirtualFidoDevice::State> state_;
  std::unique_ptr<TestAuthTokenRequesterDelegate> delegate_;
};

TEST_F(AuthTokenRequesterTest, AuthenticatorWithoutUVTokenSupport) {
  constexpr TestCase kTestCases[]{
      {
          ClientPinAvailability::kNotSupported,
          UserVerificationAvailability::kNotSupported,
          false,
      },
      {
          ClientPinAvailability::kNotSupported,
          UserVerificationAvailability::kSupportedButNotConfigured,
          false,
      },
      {
          ClientPinAvailability::kNotSupported,
          UserVerificationAvailability::kSupportedAndConfigured,
          false,
      },
      {
          ClientPinAvailability::kSupportedButPinNotSet,
          UserVerificationAvailability::kNotSupported,
          true,
      },
      {
          ClientPinAvailability::kSupportedButPinNotSet,
          UserVerificationAvailability::kSupportedButNotConfigured,
          true,
      },
      {
          ClientPinAvailability::kSupportedButPinNotSet,
          UserVerificationAvailability::kSupportedAndConfigured,
          true,
      },
      {
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kNotSupported,
          true,
      },
      {
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kSupportedButNotConfigured,
          true,
      },
      {
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kSupportedAndConfigured,
          true,
      },
  };

  int i = 0;
  for (const TestCase& t : kTestCases) {
    SCOPED_TRACE(i++);
    VirtualCtap2Device::Config config;
    config.pin_uv_auth_token_support = false;
    RunTestCase(std::move(config),
                base::MakeRefCounted<VirtualFidoDevice::State>(), t);

    if (t.success) {
      EXPECT_EQ(*delegate_->result(), AuthTokenRequester::Result::kSuccess);
      EXPECT_THAT(delegate_->response()->token_for_testing(),
                  ElementsAreArray(state_->pin_token));
      EXPECT_EQ(delegate_->pin_was_set(),
                t.client_pin == ClientPinAvailability::kSupportedButPinNotSet);
      EXPECT_EQ(delegate_->pin_was_collected(),
                t.client_pin == ClientPinAvailability::kSupportedAndPinSet);
    } else {
      EXPECT_EQ(*delegate_->result(),
                AuthTokenRequester::Result::kPreTouchUnsatisfiableRequest);
      EXPECT_FALSE(delegate_->response());
      EXPECT_FALSE(delegate_->pin_was_set());
      EXPECT_FALSE(delegate_->pin_was_collected());
    }
    EXPECT_FALSE(delegate_->internal_uv_was_retried());
    EXPECT_FALSE(delegate_->internal_uv_was_locked());
  }
}

TEST_F(AuthTokenRequesterTest, AuthenticatorWithUVTokenSupport) {
  constexpr TestCase kTestCases[]{
      {
          ClientPinAvailability::kNotSupported,
          UserVerificationAvailability::kNotSupported,
          false,
      },
      {
          ClientPinAvailability::kNotSupported,
          UserVerificationAvailability::kSupportedButNotConfigured,
          false,
      },
      {
          ClientPinAvailability::kNotSupported,
          UserVerificationAvailability::kSupportedAndConfigured,
          true,
      },
      {
          ClientPinAvailability::kSupportedButPinNotSet,
          UserVerificationAvailability::kNotSupported,
          true,
      },
      {
          ClientPinAvailability::kSupportedButPinNotSet,
          UserVerificationAvailability::kSupportedButNotConfigured,
          true,
      },
      {
          ClientPinAvailability::kSupportedButPinNotSet,
          UserVerificationAvailability::kSupportedAndConfigured,
          true,
      },
      {
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kNotSupported,
          true,
      },
      {
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kSupportedButNotConfigured,
          true,
      },
      {
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kSupportedAndConfigured,
          true,
      },
  };

  int i = 0;
  for (const TestCase& t : kTestCases) {
    SCOPED_TRACE(i++);

    VirtualCtap2Device::Config config;
    config.pin_uv_auth_token_support = true;
    config.ctap2_versions = {std::begin(kCtap2Versions2_1),
                             std::end(kCtap2Versions2_1)};
    RunTestCase(std::move(config),
                base::MakeRefCounted<VirtualFidoDevice::State>(), t);

    if (t.success) {
      EXPECT_EQ(*delegate_->result(), AuthTokenRequester::Result::kSuccess);
      EXPECT_EQ(state_->pin_uv_token_rpid, "foobar.com");
      EXPECT_EQ(state_->pin_uv_token_permissions,
                static_cast<uint8_t>(pin::Permissions::kMakeCredential));
      EXPECT_THAT(delegate_->response()->token_for_testing(),
                  ElementsAreArray(state_->pin_token));
      EXPECT_EQ(delegate_->pin_was_set(),
                t.client_pin == ClientPinAvailability::kSupportedButPinNotSet &&
                    t.user_verification !=
                        UserVerificationAvailability::kSupportedAndConfigured);
      EXPECT_EQ(delegate_->pin_was_collected(),
                t.client_pin == ClientPinAvailability::kSupportedAndPinSet &&
                    t.user_verification !=
                        UserVerificationAvailability::kSupportedAndConfigured);
      EXPECT_FALSE(delegate_->internal_uv_was_retried());
      EXPECT_FALSE(delegate_->internal_uv_was_locked());
    } else {
      EXPECT_EQ(*delegate_->result(),
                AuthTokenRequester::Result::kPreTouchUnsatisfiableRequest);
      EXPECT_FALSE(delegate_->response());
      EXPECT_FALSE(delegate_->pin_was_set());
      EXPECT_FALSE(delegate_->pin_was_collected());
      EXPECT_FALSE(delegate_->internal_uv_was_retried());
      EXPECT_FALSE(delegate_->internal_uv_was_locked());
    }
  }
}

TEST_F(AuthTokenRequesterTest, PINSoftLock) {
  VirtualCtap2Device::Config config;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {std::begin(kCtap2Versions2_1),
                           std::end(kCtap2Versions2_1)};
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  state->soft_locked = true;

  RunTestCase(std::move(config), state,
              TestCase{
                  ClientPinAvailability::kSupportedAndPinSet,
                  UserVerificationAvailability::kNotSupported,
                  false,
              });

  EXPECT_EQ(*delegate_->result(),
            AuthTokenRequester::Result::kPostTouchAuthenticatorPINSoftLock);
  EXPECT_FALSE(delegate_->response());
  EXPECT_FALSE(delegate_->pin_was_set());
  EXPECT_TRUE(delegate_->pin_was_collected());
  EXPECT_FALSE(delegate_->internal_uv_was_retried());
  EXPECT_FALSE(delegate_->internal_uv_was_locked());
}

TEST_F(AuthTokenRequesterTest, PINHardLock) {
  VirtualCtap2Device::Config config;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {std::begin(kCtap2Versions2_1),
                           std::end(kCtap2Versions2_1)};
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  state->pin_retries = 0;

  RunTestCase(std::move(config), state,
              TestCase{
                  ClientPinAvailability::kSupportedAndPinSet,
                  UserVerificationAvailability::kNotSupported,
                  false,
              });

  EXPECT_EQ(*delegate_->result(),
            AuthTokenRequester::Result::kPostTouchAuthenticatorPINHardLock);
  EXPECT_FALSE(delegate_->response());
  EXPECT_FALSE(delegate_->pin_was_set());
  EXPECT_FALSE(delegate_->pin_was_collected());
  EXPECT_FALSE(delegate_->internal_uv_was_retried());
  EXPECT_FALSE(delegate_->internal_uv_was_locked());
}

TEST_F(AuthTokenRequesterTest, UVLockedPINFallback) {
  VirtualCtap2Device::Config config;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {std::begin(kCtap2Versions2_1),
                           std::end(kCtap2Versions2_1)};
  config.user_verification_succeeds = false;
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  state->uv_retries = 3;

  RunTestCase(std::move(config), state,
              TestCase{
                  ClientPinAvailability::kSupportedAndPinSet,
                  UserVerificationAvailability::kSupportedAndConfigured,
                  true,
              });

  EXPECT_EQ(*delegate_->result(), AuthTokenRequester::Result::kSuccess);
  EXPECT_TRUE(delegate_->response());
  EXPECT_FALSE(delegate_->pin_was_set());
  EXPECT_TRUE(delegate_->pin_was_collected());
  EXPECT_EQ(delegate_->internal_uv_num_retries(), 2u);
  EXPECT_TRUE(delegate_->internal_uv_was_locked());
}

}  // namespace
}  // namespace device
