// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/auth_token_requester.h"

#include <list>
#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/pin.h"
#include "device/fido/virtual_ctap2_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using ::testing::ElementsAreArray;

using ClientPinAvailability =
    device::AuthenticatorSupportedOptions::ClientPinAvailability;
using UserVerificationAvailability =
    device::AuthenticatorSupportedOptions::UserVerificationAvailability;

constexpr char kTestPIN[] = "1234";
constexpr char16_t kTestPIN16[] = u"1234";
constexpr char16_t kNewPIN[] = u"5678";

struct TestExpectation {
  pin::PINEntryReason reason;
  pin::PINEntryError error = pin::PINEntryError::kNoError;
  uint32_t min_pin_length = kMinPinLength;
  int attempts = 8;
  std::u16string pin = kTestPIN16;
};

struct TestCase {
  ClientPinAvailability client_pin;
  UserVerificationAvailability user_verification;
  bool success;
  std::list<TestExpectation> expectations;
};

class TestAuthTokenRequesterDelegate : public AuthTokenRequester::Delegate {
 public:
  explicit TestAuthTokenRequesterDelegate(
      std::list<TestExpectation> expectations)
      : expectations_(std::move(expectations)) {}

  void WaitForResult() { wait_for_result_loop_.Run(); }
  std::optional<AuthTokenRequester::Result>& result() { return result_; }
  std::optional<pin::TokenResponse>& response() { return response_; }
  bool internal_uv_was_retried() { return internal_uv_num_retries_ > 0u; }
  size_t internal_uv_num_retries() { return internal_uv_num_retries_; }
  std::list<TestExpectation> expectations() { return expectations_; }
  void set_selectable(bool selectable) { selectable_ = selectable; }

 private:
  // AuthTokenRequester::Delegate:
  bool AuthenticatorSelectedForPINUVAuthToken(
      FidoAuthenticator* authenticator) override {
    DCHECK(!authenticator_selected_);
    if (selectable_) {
      authenticator_selected_ = true;
    }
    return selectable_;
  }
  void CollectPIN(pin::PINEntryReason reason,
                  pin::PINEntryError error,
                  uint32_t min_pin_length,
                  int attempts,
                  ProvidePINCallback provide_pin_cb) override {
    DCHECK(authenticator_selected_);

    DCHECK_NE(expectations_.size(), 0u);
    DCHECK_EQ(reason, expectations_.front().reason);
    DCHECK_EQ(error, expectations_.front().error);
    DCHECK_EQ(min_pin_length, expectations_.front().min_pin_length);
    DCHECK_EQ(attempts, expectations_.front().attempts);

    std::u16string pin = expectations_.front().pin;
    expectations_.pop_front();
    std::move(provide_pin_cb).Run(pin);
  }
  void PromptForInternalUVRetry(int attempts) override {
    DCHECK(authenticator_selected_);
    internal_uv_num_retries_++;
  }
  void HavePINUVAuthTokenResultForAuthenticator(
      FidoAuthenticator* authenticator,
      AuthTokenRequester::Result result,
      std::optional<pin::TokenResponse> response) override {
    if (!base::Contains(
            std::vector<AuthTokenRequester::Result>{
                AuthTokenRequester::Result::
                    kPreTouchAuthenticatorResponseInvalid,
                AuthTokenRequester::Result::kPreTouchUnsatisfiableRequest},
            result)) {
      DCHECK(authenticator_selected_);
    }
    DCHECK(!result_);
    result_ = result;
    response_ = std::move(response);
    wait_for_result_loop_.Quit();
  }

  std::list<TestExpectation> expectations_;

  std::optional<AuthTokenRequester::Result> result_;
  std::optional<pin::TokenResponse> response_;

  bool authenticator_selected_ = false;
  size_t internal_uv_num_retries_ = 0u;
  bool selectable_ = true;

  base::RunLoop wait_for_result_loop_;
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

    delegate_ = std::make_unique<TestAuthTokenRequesterDelegate>(
        std::move(test_case.expectations));
    AuthTokenRequester::Options options;
    options.token_permissions = {pin::Permissions::kMakeCredential};
    options.rp_id = "foobar.com";
    AuthTokenRequester requester(delegate_.get(), authenticator.get(),
                                 std::move(options));
    requester.ObtainPINUVAuthToken();
    delegate_->WaitForResult();
  }

  void TearDown() override {
    if (delegate_) {
      EXPECT_EQ(delegate_->expectations().size(), 0u);
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<VirtualFidoDevice::State> state_;
  std::unique_ptr<TestAuthTokenRequesterDelegate> delegate_;
};

TEST_F(AuthTokenRequesterTest, AuthenticatorWithoutUVTokenSupport) {
  const TestCase kTestCases[]{
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
          {{.reason = pin::PINEntryReason::kSet, .attempts = 0}},
      },
      {
          ClientPinAvailability::kSupportedButPinNotSet,
          UserVerificationAvailability::kSupportedButNotConfigured,
          true,
          {{.reason = pin::PINEntryReason::kSet, .attempts = 0}},
      },
      {
          ClientPinAvailability::kSupportedButPinNotSet,
          UserVerificationAvailability::kSupportedAndConfigured,
          true,
          {{.reason = pin::PINEntryReason::kSet, .attempts = 0}},
      },
      {
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kNotSupported,
          true,
          {{pin::PINEntryReason::kChallenge}},
      },
      {
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kSupportedButNotConfigured,
          true,
          {{pin::PINEntryReason::kChallenge}},
      },
      {
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kSupportedAndConfigured,
          true,
          {{pin::PINEntryReason::kChallenge}},
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
    } else {
      EXPECT_EQ(*delegate_->result(),
                AuthTokenRequester::Result::kPreTouchUnsatisfiableRequest);
      EXPECT_FALSE(delegate_->response());
    }
    EXPECT_FALSE(delegate_->internal_uv_was_retried());
  }
}

TEST_F(AuthTokenRequesterTest, AuthenticatorWithUVTokenSupport) {
  const TestCase kTestCases[]{
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
          {{.reason = pin::PINEntryReason::kSet, .attempts = 0}},
      },
      {
          ClientPinAvailability::kSupportedButPinNotSet,
          UserVerificationAvailability::kSupportedButNotConfigured,
          true,
          {{.reason = pin::PINEntryReason::kSet, .attempts = 0}},
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
          {{pin::PINEntryReason::kChallenge}},
      },
      {
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kSupportedButNotConfigured,
          true,
          {{pin::PINEntryReason::kChallenge}},
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
      EXPECT_FALSE(delegate_->internal_uv_was_retried());
    } else {
      EXPECT_EQ(*delegate_->result(),
                AuthTokenRequester::Result::kPreTouchUnsatisfiableRequest);
      EXPECT_FALSE(delegate_->response());
      EXPECT_FALSE(delegate_->internal_uv_was_retried());
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
              TestCase{ClientPinAvailability::kSupportedAndPinSet,
                       UserVerificationAvailability::kNotSupported,
                       false,
                       {{pin::PINEntryReason::kChallenge}}});

  EXPECT_EQ(*delegate_->result(),
            AuthTokenRequester::Result::kPostTouchAuthenticatorPINSoftLock);
  EXPECT_FALSE(delegate_->response());
  EXPECT_FALSE(delegate_->internal_uv_was_retried());
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
  EXPECT_FALSE(delegate_->internal_uv_was_retried());
}

TEST_F(AuthTokenRequesterTest, PINInvalid) {
  VirtualCtap2Device::Config config;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {std::begin(kCtap2Versions2_1),
                           std::end(kCtap2Versions2_1)};
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  RunTestCase(
      std::move(config), state,
      TestCase{ClientPinAvailability::kSupportedAndPinSet,
               UserVerificationAvailability::kNotSupported,
               true,
               {{.reason = pin::PINEntryReason::kChallenge,
                 .pin = std::u16string({0xd800, 0xd800, 0xd800, 0xd800})},
                {pin::PINEntryReason::kChallenge,
                 pin::PINEntryError::kInvalidCharacters}}});
}

TEST_F(AuthTokenRequesterTest, PINTooShort) {
  VirtualCtap2Device::Config config;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {std::begin(kCtap2Versions2_1),
                           std::end(kCtap2Versions2_1)};
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  RunTestCase(
      std::move(config), state,
      TestCase{
          ClientPinAvailability::kSupportedAndPinSet,
          UserVerificationAvailability::kNotSupported,
          true,
          {{.reason = pin::PINEntryReason::kChallenge, .pin = u"まどか"},
           {pin::PINEntryReason::kChallenge, pin::PINEntryError::kTooShort}}});
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
              TestCase{ClientPinAvailability::kSupportedAndPinSet,
                       UserVerificationAvailability::kSupportedAndConfigured,
                       true,
                       {{pin::PINEntryReason::kChallenge,
                         pin::PINEntryError::kInternalUvLocked}}});

  EXPECT_EQ(*delegate_->result(), AuthTokenRequester::Result::kSuccess);
  EXPECT_TRUE(delegate_->response());
  EXPECT_EQ(delegate_->internal_uv_num_retries(), 2u);
}

TEST_F(AuthTokenRequesterTest, UVAlreadyLockedPINFallback) {
  VirtualCtap2Device::Config config;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {std::begin(kCtap2Versions2_1),
                           std::end(kCtap2Versions2_1)};
  config.user_verification_succeeds = false;
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  state->uv_retries = 0;

  RunTestCase(std::move(config), state,
              TestCase{ClientPinAvailability::kSupportedAndPinSet,
                       UserVerificationAvailability::kSupportedAndConfigured,
                       true,
                       {{pin::PINEntryReason::kChallenge,
                         pin::PINEntryError::kInternalUvLocked}}});

  EXPECT_EQ(*delegate_->result(), AuthTokenRequester::Result::kSuccess);
  EXPECT_TRUE(delegate_->response());
  EXPECT_EQ(delegate_->internal_uv_num_retries(), 0u);
}

TEST_F(AuthTokenRequesterTest, ForcePINChange) {
  VirtualCtap2Device::Config config;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {std::begin(kCtap2Versions2_1),
                           std::end(kCtap2Versions2_1)};
  config.min_pin_length_support = true;
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  state->force_pin_change = true;

  RunTestCase(std::move(config), state,
              TestCase{ClientPinAvailability::kSupportedAndPinSet,
                       UserVerificationAvailability::kNotSupported,
                       true,
                       {{pin::PINEntryReason::kChallenge},
                        {
                            .reason = pin::PINEntryReason::kChange,
                            .attempts = 0,
                            .pin = kNewPIN,
                        }}});

  EXPECT_EQ(*delegate_->result(), AuthTokenRequester::Result::kSuccess);
  EXPECT_TRUE(delegate_->response());
}

TEST_F(AuthTokenRequesterTest, ForcePINChangeSameAsCurrent) {
  VirtualCtap2Device::Config config;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {std::begin(kCtap2Versions2_1),
                           std::end(kCtap2Versions2_1)};
  config.min_pin_length_support = true;
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  state->force_pin_change = true;

  RunTestCase(std::move(config), state,
              TestCase{ClientPinAvailability::kSupportedAndPinSet,
                       UserVerificationAvailability::kNotSupported,
                       true,
                       {{pin::PINEntryReason::kChallenge},
                        {
                            .reason = pin::PINEntryReason::kChange,
                            .attempts = 0,
                        },
                        {
                            .reason = pin::PINEntryReason::kChange,
                            .error = pin::PINEntryError::kSameAsCurrentPIN,
                            .attempts = 0,
                            .pin = kNewPIN,
                        }}});

  EXPECT_EQ(*delegate_->result(), AuthTokenRequester::Result::kSuccess);
  EXPECT_TRUE(delegate_->response());
}

TEST_F(AuthTokenRequesterTest, NoCallsIfNotSelected) {
  // Test that a failure to select an authenticator stops processing.

  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  VirtualCtap2Device::Config config;

  config.pin_support = true;
  state->pin = kTestPIN;
  config.internal_uv_support = true;
  state->fingerprints_enrolled = true;

  auto authenticator = std::make_unique<FidoDeviceAuthenticator>(
      std::make_unique<VirtualCtap2Device>(state, std::move(config)));

  base::RunLoop init_loop;
  authenticator->InitializeAuthenticator(init_loop.QuitClosure());
  init_loop.Run();

  auto delegate = std::make_unique<TestAuthTokenRequesterDelegate>(
      std::list<TestExpectation>());
  delegate->set_selectable(false);
  AuthTokenRequester::Options options;
  options.token_permissions = {pin::Permissions::kMakeCredential};
  options.rp_id = "foobar.com";
  AuthTokenRequester requester(delegate.get(), authenticator.get(),
                               std::move(options));
  requester.ObtainPINUVAuthToken();
}

}  // namespace
}  // namespace device
