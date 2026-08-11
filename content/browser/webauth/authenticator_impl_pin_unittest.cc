// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/authenticator_impl_unittest_test_base.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_authenticator_manager_impl.h"
#include "content/public/common/content_client.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_user_verification_requirement.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/multiple_virtual_fido_device_factory.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/public/fido_types.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

namespace content {
namespace {

using device::VirtualCtap2Device;
using device::VirtualFidoDevice;

using PINReason = device::pin::PINEntryReason;
using PINError = device::pin::PINEntryError;

// PINExpectation represent expected |mode|, |attempts|, |min_pin_length| and
// the PIN to answer with.
struct PINExpectation {
  PINReason reason;
  std::u16string pin;
  int attempts;
  uint32_t min_pin_length = device::kMinPinLength;
  PINError error = PINError::kNoError;
};

class PINTestAuthenticatorRequestDelegate
    : public DefaultAuthenticatorRequestClientDelegate {
 public:
  PINTestAuthenticatorRequestDelegate(
      bool supports_pin,
      const std::list<PINExpectation>& pins,
      std::optional<InterestingFailureReason>* failure_reason,
      base::RepeatingCallback<bool()> collect_pin_cb)
      : supports_pin_(supports_pin),
        expected_(pins),
        failure_reason_(failure_reason),
        collect_pin_cb_(collect_pin_cb) {}

  PINTestAuthenticatorRequestDelegate(
      const PINTestAuthenticatorRequestDelegate&) = delete;
  PINTestAuthenticatorRequestDelegate& operator=(
      const PINTestAuthenticatorRequestDelegate&) = delete;

  ~PINTestAuthenticatorRequestDelegate() override {
    DCHECK(expected_.empty())
        << expected_.size() << " unsatisifed PIN expectations";
  }

  bool SupportsPIN() const override { return supports_pin_; }

  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override {
    if (collect_pin_cb_ && !collect_pin_cb_.Run()) {
      return;
    }
    DCHECK(supports_pin_);
    DCHECK(!expected_.empty()) << "unexpected PIN request";
    if (expected_.front().reason == PINReason::kChallenge) {
      DCHECK(options.attempts == expected_.front().attempts)
          << "got: " << options.attempts
          << " expected: " << expected_.front().attempts;
    }
    DCHECK_EQ(expected_.front().min_pin_length, options.min_pin_length);
    DCHECK_EQ(expected_.front().reason, options.reason);
    DCHECK_EQ(expected_.front().error, options.error);
    std::u16string pin = std::move(expected_.front().pin);
    expected_.pop_front();

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(provide_pin_cb), std::move(pin)));
  }

  void FinishCollectToken() override {}

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override {
    *failure_reason_ = reason;
    return AuthenticatorRequestClientDelegate::DoesBlockRequestOnFailure(
        reason);
  }

 private:
  const bool supports_pin_;
  std::list<PINExpectation> expected_;
  const raw_ptr<std::optional<InterestingFailureReason>> failure_reason_;
  // collect_pin_cb_ is optional. If present, it returns whether `CollectPIN`
  // should continue and invoke its main callback.
  base::RepeatingCallback<bool()> collect_pin_cb_;
};

class PINTestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  // ContentBrowserClient:
  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<PINTestAuthenticatorRequestDelegate>(
        supports_pin, expected, &failure_reason, collect_pin_cb);
  }

  TestWebAuthenticationDelegate web_authentication_delegate;

  bool supports_pin = true;
  std::list<PINExpectation> expected;
  std::optional<InterestingFailureReason> failure_reason;
  base::RepeatingCallback<bool()> collect_pin_cb;
};

class PINAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  PINAuthenticatorImplTest() = default;

  PINAuthenticatorImplTest(const PINAuthenticatorImplTest&) = delete;
  PINAuthenticatorImplTest& operator=(const PINAuthenticatorImplTest&) = delete;

  void SetUp() override {
    UVAuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    virtual_device_factory_->SetCtap2Config(config);
    NavigateAndCommit(GURL(kTestOrigin1));
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    UVAuthenticatorImplTest::TearDown();
  }

 protected:
  PINTestAuthenticatorContentBrowserClient test_client_;

  // An enumerate of outcomes for PIN tests.
  enum {
    kFailure,
    kNoPIN,
    kSetPIN,
    kUsePIN,
  };

  void ConfigureVirtualDevice(device::PINUVAuthProtocol pin_protocol,
                              bool pin_uv_auth_token,
                              int support_level) {
    device::VirtualCtap2Device::Config config;
    config.pin_protocol = pin_protocol;
    config.pin_uv_auth_token_support = pin_uv_auth_token;
    config.ctap2_versions = {device::Ctap2Version::kCtap2_0,
                             device::Ctap2Version::kCtap2_1};
    switch (support_level) {
      case 0:
        // No support.
        config.pin_support = false;
        virtual_device_factory_->mutable_state()->pin = "";
        virtual_device_factory_->mutable_state()->pin_retries = 0;
        break;

      case 1:
        // PIN supported, but no PIN set.
        config.pin_support = true;
        virtual_device_factory_->mutable_state()->pin = "";
        virtual_device_factory_->mutable_state()->pin_retries = 0;
        break;

      case 2:
        // PIN set.
        config.pin_support = true;
        virtual_device_factory_->mutable_state()->pin = kTestPIN;
        virtual_device_factory_->mutable_state()->pin_retries =
            device::kMaxPinRetries;
        break;

      default:
        NOTREACHED();
    }

    virtual_device_factory_->SetCtap2Config(config);
  }

 private:
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

static constexpr std::array<device::UserVerificationRequirement, 3> kUVLevel = {
    device::UserVerificationRequirement::kDiscouraged,
    device::UserVerificationRequirement::kPreferred,
    device::UserVerificationRequirement::kRequired,
};

static const std::array<std::string_view, 3> kUVDescription = {
    device::kUserVerificationDiscouraged, device::kUserVerificationPreferred,
    device::kUserVerificationRequired};

static const std::array<std::string_view, 3> kPINSupportDescription = {
    "no PIN support", "PIN not set", "PIN set"};

TEST_F(PINAuthenticatorImplTest, MakeCredential) {
  typedef std::array<int, 3> UvRequirement;
  typedef std::array<UvRequirement, 3> Expectations;
  // kExpectedWithUISupport enumerates the expected behaviour when the embedder
  // supports prompting the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithUISupport = std::to_array<UvRequirement>({
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kSetPIN  },
    /* PIN set */     {    kUsePIN,     kUsePIN,    kUsePIN  },
    //                        ^
    //                        |
    //            VirtualCtap2Device cannot fall back to U2F.
  });
  // clang-format on

  // kExpectedWithoutUISupport enumerates the expected behaviour when the
  // embedder cannot prompt the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithoutUISupport = std::to_array<UvRequirement>({
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN set */     {    kFailure,    kFailure,   kFailure },
    //                        ^            ^
    //                        |            |
    //            VirtualCtap2Device cannot fall back to U2F and
    //            a PIN is required to create credentials once set
    //            in CTAP 2.0.
  });
  // clang-format on

  for (bool pin_uv_auth_token : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "pin_uv_auth_token=" << pin_uv_auth_token);
    for (bool ui_support : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "ui_support=" << ui_support);
      const Expectations& expected =
          ui_support ? kExpectedWithUISupport : kExpectedWithoutUISupport;
      test_client_.supports_pin = ui_support;

      for (int support_level = 0; support_level <= 2; support_level++) {
        for (const auto pin_protocol :
             {device::PINUVAuthProtocol::kV1, device::PINUVAuthProtocol::kV2}) {
          SCOPED_TRACE(testing::Message()
                       << "support_level="
                       << kPINSupportDescription[support_level]
                       << ", pin_protocol=" << static_cast<int>(pin_protocol));
          for (const bool excluded_credentials : {false, true}) {
            SCOPED_TRACE(::testing::Message()
                         << "excluded_credentials=" << excluded_credentials);
            for (const bool appid_exclude : {false, true}) {
              if (appid_exclude && !excluded_credentials) {
                continue;
              }
              SCOPED_TRACE(::testing::Message()
                           << "appid_exclude=" << appid_exclude);

              for (const bool always_uv : {false, true}) {
                if (always_uv &&
                    (!ui_support ||
                     virtual_device_factory_->mutable_state()->pin.empty())) {
                  continue;
                }
                SCOPED_TRACE(::testing::Message() << "always_uv=" << always_uv);

                ConfigureVirtualDevice(pin_protocol, pin_uv_auth_token,
                                       support_level);

                for (int uv_level = 0; uv_level <= 2; uv_level++) {
                  SCOPED_TRACE(kUVDescription[uv_level]);

                  switch (expected[support_level][uv_level]) {
                    case kNoPIN:
                    case kFailure:
                      // There shouldn't be any PIN prompts.
                      test_client_.expected.clear();
                      break;

                    case kSetPIN:
                      // A single PIN prompt to set a PIN is expected.
                      test_client_.expected = {{PINReason::kSet, kTestPIN16}};
                      break;

                    case kUsePIN:
                      // A single PIN prompt to get the PIN is expected.
                      test_client_.expected = {
                          {PINReason::kChallenge, kTestPIN16, 8}};
                      break;

                    default:
                      NOTREACHED();
                  }

                  MakeCredentialResult result =
                      AuthenticatorMakeCredential(make_credential_options(
                          kUVLevel[uv_level], excluded_credentials,
                          appid_exclude));

                  switch (expected[support_level][uv_level]) {
                    case kFailure:
                      EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                                result.status);
                      break;

                    case kNoPIN:
                      ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
                      EXPECT_EQ("",
                                virtual_device_factory_->mutable_state()->pin);
                      EXPECT_FALSE(HasUV(result.response));
                      break;

                    case kSetPIN:
                    case kUsePIN:
                      ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
                      EXPECT_EQ(kTestPIN,
                                virtual_device_factory_->mutable_state()->pin);
                      EXPECT_TRUE(HasUV(result.response));
                      break;

                    default:
                      NOTREACHED();
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialSoftLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  test_client_.expected = {{PINReason::kChallenge, u"wrong", 8},
                           {PINReason::kChallenge, u"wrong", 7,
                            device::kMinPinLength, PINError::kWrongPIN},
                           {PINReason::kChallenge, u"wrong", 6,
                            device::kMinPinLength, PINError::kWrongPIN}};
  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->pin_retries);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->soft_locked);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kSoftPINBlock,
            *test_client_.failure_reason);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kSoftPinBlock,
                                 AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialHardLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries = 1;

  test_client_.expected = {{PINReason::kChallenge, u"wrong", 1}};
  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(0, virtual_device_factory_->mutable_state()->pin_retries);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kHardPINBlock,
            *test_client_.failure_reason);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kHardPinBlock,
                                 AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialWrongPINFirst) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  // Test that we can successfully get a PIN token after a failure.
  test_client_.expected = {{PINReason::kChallenge, u"wrong", 8},
                           {PINReason::kChallenge, kTestPIN16, 7,
                            device::kMinPinLength, PINError::kWrongPIN}};
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(static_cast<int>(device::kMaxPinRetries),
            virtual_device_factory_->mutable_state()->pin_retries);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialSkipPINTouch) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  int taps = 0;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        ++taps;
        return true;
      });
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(taps, 1);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialDontSkipPINTouch) {
  // Create two devices. Both are candidates but only the second one will
  // respond to touches.
  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
  device_1.config.pin_support = true;
  device_1.state->simulate_press_callback =
      base::BindRepeating([](VirtualFidoDevice* ignore) { return false; });
  discovery->AddDevice(std::move(device_1));

  int taps = 0;
  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_2;
  device_2.state->pin = kTestPIN;
  device_2.config.pin_support = true;
  device_2.state->simulate_press_callback =
      base::BindLambdaForTesting([&](VirtualFidoDevice* ignore) {
        ++taps;
        return true;
      });
  discovery->AddDevice(std::move(device_2));

  ReplaceDiscoveryFactory(std::move(discovery));

  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(taps, 2);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialAlwaysUv) {
  // Test that if an authenticator is reporting alwaysUv = 1, UV is attempted
  // even if the user verification requirement is discouraged.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.always_uv = true;

  // Enable u2f support. Normally, this would allow chrome to create a
  // credential without internal user verification, but we should not attempt
  // that with the alwaysUv flag on.
  config.u2f_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};

  MakeCredentialResult result =
      AuthenticatorMakeCredential(make_credential_options(
          device::UserVerificationRequirement::kDiscouraged));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialMinPINLengthNewPIN) {
  // Test that an authenticator advertising a min PIN length other than the
  // default makes it all the way to CollectPIN when setting a new PIN.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.min_pin_length_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->min_pin_length = 6;
  test_client_.expected = {{PINReason::kSet, u"123456", 0, 6}};

  MakeCredentialResult result =
      AuthenticatorMakeCredential(make_credential_options());
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialMinPINLengthExistingPIN) {
  // Test that an authenticator advertising a min PIN length other than the
  // default makes it all the way to CollectPIN when using an existing PIN and
  // the forcePINChange flag is false.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.min_pin_length_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->min_pin_length = 6;
  virtual_device_factory_->mutable_state()->pin = "123456";
  test_client_.expected = {
      {PINReason::kChallenge, u"123456", device::kMaxPinRetries, 6}};

  MakeCredentialResult result =
      AuthenticatorMakeCredential(make_credential_options());
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialForcePINChange) {
  // Test that an authenticator with the forcePINChange flag set to true updates
  // the PIN before attempting to make a credential. When querying for an
  // existing PIN, the default min PIN length should be asked since there is no
  // way to know the current PIN length.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.min_pin_length_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->force_pin_change = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->min_pin_length = 6;
  test_client_.expected = {{PINReason::kChallenge, kTestPIN16,
                            device::kMaxPinRetries, device::kMinPinLength},
                           {PINReason::kChange, u"567890", 0, 6}};

  MakeCredentialResult result =
      AuthenticatorMakeCredential(make_credential_options());
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ("567890", virtual_device_factory_->mutable_state()->pin);
}

TEST_F(PINAuthenticatorImplTest, MakeCredUvNotRqd) {
  // Test that on an authenticator with the makeCredUvNotRqd option enabled,
  // non-discoverable credentials can be created without requiring a PIN.
  for (bool discoverable : {false, true}) {
    for (bool request_uv : {false, true}) {
      SCOPED_TRACE(testing::Message() << "discoverable=" << discoverable
                                      << " request_uv=" << request_uv);

      test_client_.web_authentication_delegate.supports_resident_keys = true;
      ResetVirtualDevice();
      device::VirtualCtap2Device::Config config;
      config.u2f_support = true;
      config.pin_support = true;
      config.resident_key_support = true;
      config.pin_uv_auth_token_support = true;
      config.allow_non_resident_credential_creation_without_uv = true;
      config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
      virtual_device_factory_->SetCtap2Config(config);
      virtual_device_factory_->mutable_state()->pin = kTestPIN;
      // PIN is still required for discoverable credentials, or if the caller
      // requests it.
      if (discoverable || request_uv) {
        test_client_.expected = {{PINReason::kChallenge, kTestPIN16,
                                  device::kMaxPinRetries,
                                  device::kMinPinLength}};
      } else {
        test_client_.expected = {};
      }

      PublicKeyCredentialCreationOptionsPtr request = make_credential_options();
      request->authenticator_selection->user_verification_requirement =
          request_uv ? device::UserVerificationRequirement::kPreferred
                     : device::UserVerificationRequirement::kDiscouraged;
      request->authenticator_selection->resident_key =
          discoverable ? device::ResidentKeyRequirement::kPreferred
                       : device::ResidentKeyRequirement::kDiscouraged;

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(request));
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
      // Requests shouldn't fall back to creating U2F credentials.
      EXPECT_FALSE(virtual_device_factory_->mutable_state()
                       ->registrations.begin()
                       ->second.is_u2f);
    }
  }
}

TEST_F(PINAuthenticatorImplTest, MakeCredUvNotRqdAndAlwaysUv) {
  // makeCredUvNotRqd and alwaysUv can be combined even though they contradict
  // each other. In that case, makeCredUvNotRqd should be ignored and PIN/UV
  // should be collected before creating non-discoverable credentials. If PIN/UV
  // isn't configured, that should be taken care of first.
  for (bool pin_set : {false, true}) {
    SCOPED_TRACE(testing::Message() << "pin_set=" << pin_set);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.pin_uv_auth_token_support = true;
    config.always_uv = true;
    config.allow_non_resident_credential_creation_without_uv = true;
    config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
    virtual_device_factory_->SetCtap2Config(config);
    if (pin_set) {
      virtual_device_factory_->mutable_state()->pin = kTestPIN;
      test_client_.expected = {{PINReason::kChallenge, kTestPIN16,
                                device::kMaxPinRetries, device::kMinPinLength}};
    } else {
      test_client_.expected = {{PINReason::kSet, kTestPIN16,
                                device::kMaxPinRetries, device::kMinPinLength}};
    }

    MakeCredentialResult result =
        AuthenticatorMakeCredential(make_credential_options());
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  }
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialHMACSecret) {
  // uv=preferred is more preferred when hmac-secret is in use so that the
  // PRF is consistent. (Security keys have two PRFs per credential: one for
  // UV and one for non-UV assertions.)
  struct TestCase {
    device::UserVerificationRequirement uv;
    bool hmac_secret;
    bool should_configure_uv;
  };

  constexpr TestCase kTests[] = {
      {device::UserVerificationRequirement::kDiscouraged, false, false},
      {device::UserVerificationRequirement::kPreferred, false, false},
      {device::UserVerificationRequirement::kRequired, false, true},
      {device::UserVerificationRequirement::kDiscouraged, true, true},
      {device::UserVerificationRequirement::kPreferred, true, true},
      {device::UserVerificationRequirement::kRequired, true, true},
  };

  NavigateAndCommit(GURL(kTestOrigin1));
  unsigned index = 0;
  for (const TestCase& test : kTests) {
    SCOPED_TRACE(index++);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.hmac_secret_support = true;
    config.pin_support = true;
    config.pin_uv_auth_token_support = true;
    config.allow_non_resident_credential_creation_without_uv = true;
    config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
    virtual_device_factory_->SetCtap2Config(config);

    if (test.should_configure_uv) {
      test_client_.expected = {{PINReason::kSet, kTestPIN16,
                                device::kMaxPinRetries, device::kMinPinLength}};
    } else {
      test_client_.expected.clear();
    }

    auto options = make_credential_options(test.uv);
    options->hmac_create_secret = test.hmac_secret;
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  }
}

TEST_F(PINAuthenticatorImplTest, PRFOnCreate) {
  // WebAuthn PRF client registration extension allows a Relying Party
  // to evaluate outputs from a PRF associated with the credential to be
  // registered.
  // Some hybrid and platform authenticators (i.e. QR code) supports this
  // with PRF extension, some others (i.e. security keys) supports this
  // with hmac-secret-mc extension.
  enum ExtensionSupport {
    kNone,
    kPRF,
    kHmacSecretMc,
  };

  NavigateAndCommit(GURL(kTestOrigin1));

  // Verifies PRF-on-create with or without hmac-secret-mc support flag set.
  // Verifies PRF-on-create with authenticator's extension support cases.
  for (ExtensionSupport extension_support : {kPRF, kHmacSecretMc, kNone}) {
    // Verifies PRF result values depending on whether authenticator supports
    // user verification or not.
    for (bool uv_required : {true, false}) {
      SCOPED_TRACE(::testing::Message()
                   << "extension_support: " << extension_support << ", "
                   << "uv_required: " << uv_required);
      ResetVirtualDevice();

      // Configure virtual CTAP2 device.
      device::VirtualCtap2Device::Config config;
      switch (extension_support) {
        case kPRF:
          // authenticator supports PRF extension.
          config.prf_support = true;
          config.internal_account_chooser = true;
          break;
        case kHmacSecretMc:
          // authenticator supports hmac-secret-mc extension.
          config.hmac_secret_support = true;
          config.hmac_secret_mc_support = true;
          break;
        case kNone:
          // authenticator doesn't support the extensions.
          break;
      }
      if (uv_required) {
        // Set authenticator supports pin for the case with 'required'
        // user verification requirement option.
        config.pin_support = true;
      }
      config.pin_uv_auth_token_support = true;
      config.ctap2_versions = {device::Ctap2Version::kCtap2_2};
      // Set authenticator creates the hmac secret key for the new
      // credential, filled with 3 for the uv unsupported authenticators,
      // with 4 for the uv supported authenticators.
      config.make_credential_hmac_key_byte.emplace(3, 4);
      virtual_device_factory_->SetCtap2Config(config);

      if (uv_required) {
        test_client_.expected = {{PINReason::kSet, kTestPIN16,
                                  device::kMaxPinRetries,
                                  device::kMinPinLength}};
      } else {
        test_client_.expected.clear();
      }

      // Prepare expected PRF results.
      device::PRFInput prf_input_eval;
      prf_input_eval.input1 = std::vector<uint8_t>(32, 1);
      prf_input_eval.input2 = std::vector<uint8_t>(32, 2);
      prf_input_eval.HashInputsIntoSalts();
      auto make_prf_results_eval =
          [&prf_input_eval](
              uint8_t hmac_key_byte) -> const std::vector<uint8_t> {
        return device::PRFInput::EvaluateHMAC(
            std::vector<uint8_t>(32, hmac_key_byte), prf_input_eval.salt1,
            prf_input_eval.salt2);
      };
      const std::vector<uint8_t> prf_results_eval =
          uv_required ? make_prf_results_eval(
                            config.make_credential_hmac_key_byte->second)
                      : make_prf_results_eval(
                            config.make_credential_hmac_key_byte->first);

      // Make credential with prf input, and get result.
      auto options = make_credential_options(
          uv_required ? device::UserVerificationRequirement::kRequired
                      : device::UserVerificationRequirement::kPreferred);
      options->prf_enable = true;
      options->prf_input = blink::mojom::PRFValues::New();
      options->prf_input->first = prf_input_eval.input1;
      options->prf_input->second = prf_input_eval.input2;
      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));

      // Verify make credential result.
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
      EXPECT_TRUE(result.response->echo_prf);
      switch (extension_support) {
        case kHmacSecretMc:
          ABSL_FALLTHROUGH_INTENDED;
        case kPRF:
          // In case that authenticator supports PRF extension, or
          // authenticator supports hmac-secret-mc extension and
          // the feature flag is enabled, the make credential result
          // contains prf results.
          EXPECT_TRUE(result.response->prf);
          ASSERT_TRUE(result.response->prf_results);
          EXPECT_EQ(result.response->prf_results->first.size(), 32u);
          EXPECT_EQ(result.response->prf_results->second->size(), 32u);
          // validate the prf results in the make credential result.
          EXPECT_EQ(result.response->prf_results->first,
                    std::vector<uint8_t>(&prf_results_eval[0],
                                         &prf_results_eval[32]));
          EXPECT_EQ(result.response->prf_results->second,
                    std::vector<uint8_t>(prf_results_eval.begin() + 32,
                                         prf_results_eval.end()));
          break;
        case kNone:
          EXPECT_FALSE(result.response->prf);
          ASSERT_FALSE(result.response->prf_results);
          break;
      }

      // Verify make credential authentication data.
      device::AuthenticatorData parsed_auth_data =
          AuthDataFromMakeCredentialResponse(result.response);
      bool has_hmac_secret = false;
      bool has_hmac_secret_mc = false;
      const auto& extensions = parsed_auth_data.extensions();
      if (extensions) {
        CHECK(extensions->is_map());
        const cbor::Value::MapValue& extensions_map = extensions->GetMap();

        const auto hmac_secret_it =
            extensions_map.find(cbor::Value(device::kExtensionHmacSecret));
        // Verify hmac-secret extension response if exists.
        if (hmac_secret_it != extensions_map.end()) {
          ASSERT_TRUE(hmac_secret_it->second.is_bool());
          EXPECT_TRUE(hmac_secret_it->second.GetBool());
          has_hmac_secret = true;
        }

        const auto hmac_secret_mc_it =
            extensions_map.find(cbor::Value(device::kExtensionHmacSecretMc));
        // Verify hmac-secret-mc extension response if exists.
        if (hmac_secret_mc_it != extensions_map.end()) {
          ASSERT_TRUE(hmac_secret_mc_it->second.is_bytestring());
          // Since the authenticator encrypts and returns the hashed
          // hmac secret key generated for the requested credentials,
          // the hmac-secret-mc response in the authentication data
          // must be different from the hashed secret key generated
          // by the authenticator.
          EXPECT_NE(prf_results_eval,
                    hmac_secret_mc_it->second.GetBytestring());
          has_hmac_secret_mc = true;
        }
      }
      switch (extension_support) {
        case kPRF:
        case kNone:
          EXPECT_FALSE(has_hmac_secret);
          EXPECT_FALSE(has_hmac_secret_mc);
          break;
        case kHmacSecretMc:
          EXPECT_TRUE(has_hmac_secret);
          EXPECT_TRUE(has_hmac_secret_mc);
          break;
      }
    }
  }
}

TEST_F(PINAuthenticatorImplTest, HmacSecretMcWithVirtualAuthenticator) {
  // Test PRF-on-create with a VirtualAuthenticator that supports
  // hmac-secret-mc extension and doesn't support user verification.

  NavigateAndCommit(GURL(kTestOrigin1));

  virtual_device_factory_ = nullptr;
  content::AuthenticatorEnvironment* authenticator_environment =
      content::AuthenticatorEnvironment::GetInstance();
  authenticator_environment->Reset();
  FrameTreeNode* frame_tree_node =
      static_cast<content::RenderFrameHostImpl*>(main_rfh())->frame_tree_node();
  authenticator_environment->EnableVirtualAuthenticatorFor(frame_tree_node,
                                                           /*enable_ui=*/false);
  VirtualAuthenticatorManagerImpl* virtual_authenticator_manager =
      authenticator_environment->MaybeGetVirtualAuthenticatorManager(
          frame_tree_node);
  VirtualAuthenticator::Options virt_auth_options;
  virt_auth_options.protocol = device::ProtocolVersion::kCtap2;
  virt_auth_options.ctap2_version = device::Ctap2Version::kCtap2_2;
  virt_auth_options.has_hmac_secret = true;
  virt_auth_options.has_hmac_secret_mc = true;
  VirtualAuthenticator* authenticator =
      virtual_authenticator_manager->AddAuthenticatorAndReturnNonOwningPointer(
          virt_auth_options);

  test_client_.expected.clear();

  auto options =
      make_credential_options(device::UserVerificationRequirement::kPreferred);
  options->prf_enable = true;
  options->prf_input = blink::mojom::PRFValues::New();
  options->prf_input->first = std::vector<uint8_t>(32, 1);
  options->prf_input->second = std::vector<uint8_t>(32, 2);

  MakeCredentialResult result = AuthenticatorMakeCredential(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);

  EXPECT_TRUE(result.response->prf);
  ASSERT_TRUE(result.response->prf_results);
  EXPECT_EQ(result.response->prf_results->first.size(), 32u);
  EXPECT_EQ(result.response->prf_results->second->size(), 32u);

  device::AuthenticatorData parsed_auth_data =
      AuthDataFromMakeCredentialResponse(result.response);

  bool has_hmac_secret = false;
  bool has_hmac_secret_mc = false;
  const auto& extensions = parsed_auth_data.extensions();
  if (extensions) {
    CHECK(extensions->is_map());
    const cbor::Value::MapValue& extensions_map = extensions->GetMap();

    const auto hmac_secret_it =
        extensions_map.find(cbor::Value(device::kExtensionHmacSecret));
    if (hmac_secret_it != extensions_map.end()) {
      ASSERT_TRUE(hmac_secret_it->second.is_bool());
      EXPECT_TRUE(hmac_secret_it->second.GetBool());
      has_hmac_secret = true;
    }

    const auto hmac_secret_mc_it =
        extensions_map.find(cbor::Value(device::kExtensionHmacSecretMc));
    if (hmac_secret_mc_it != extensions_map.end()) {
      ASSERT_TRUE(hmac_secret_mc_it->second.is_bytestring());
      has_hmac_secret_mc = true;
    }
  }

  EXPECT_TRUE(has_hmac_secret);
  EXPECT_TRUE(has_hmac_secret_mc);

  EXPECT_TRUE(
      authenticator->registrations().contains(result.response->info->raw_id));
}

TEST_F(PINAuthenticatorImplTest, GetAssertion) {
  typedef std::array<int, 3> UvRequirement;
  typedef std::array<UvRequirement, 3> Expectations;
  // kExpectedWithUISupport enumerates the expected behaviour when the embedder
  // supports prompting the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithUISupport = std::to_array<UvRequirement>({
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN set */     {    kNoPIN,      kUsePIN,    kUsePIN  },
  });
  // clang-format on

  // kExpectedWithoutUISupport enumerates the expected behaviour when the
  // embedder cannot prompt the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithoutUISupport = std::to_array<UvRequirement>({
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN set */     {    kNoPIN,      kNoPIN,     kFailure },
  });
  // clang-format on

  PublicKeyCredentialRequestOptionsPtr dummy_options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      dummy_options->allow_credentials[0].id, kTestRelyingPartyId));

  for (bool pin_uv_auth_token : {false, true}) {
    for (bool ui_support : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "ui_support=" << ui_support);
      const Expectations& expected =
          ui_support ? kExpectedWithUISupport : kExpectedWithoutUISupport;
      test_client_.supports_pin = ui_support;

      for (int support_level = 0; support_level <= 2; support_level++) {
        SCOPED_TRACE(kPINSupportDescription[support_level]);
        for (const auto pin_protocol :
             {device::PINUVAuthProtocol::kV1, device::PINUVAuthProtocol::kV2}) {
          SCOPED_TRACE(testing::Message()
                       << "support_level="
                       << kPINSupportDescription[support_level]
                       << ", pin_protocol=" << static_cast<int>(pin_protocol));
          ConfigureVirtualDevice(pin_protocol, pin_uv_auth_token,
                                 support_level);

          for (int uv_level = 0; uv_level <= 2; uv_level++) {
            SCOPED_TRACE(kUVDescription[uv_level]);

            switch (expected[support_level][uv_level]) {
              case kNoPIN:
              case kFailure:
                // No PIN prompts are expected.
                test_client_.expected.clear();
                break;

              case kUsePIN:
                // A single prompt to get the PIN is expected.
                test_client_.expected = {
                    {PINReason::kChallenge, kTestPIN16, 8}};
                break;

              default:
                NOTREACHED();
            }

            GetAssertionResult result = AuthenticatorGetAssertion(
                get_credential_options(kUVLevel[uv_level]));

            switch (expected[support_level][uv_level]) {
              case kFailure:
                EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                          result.status);
                break;

              case kNoPIN:
                EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
                EXPECT_FALSE(HasUV(result.response));
                break;

              case kUsePIN:
                EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
                EXPECT_EQ(kTestPIN,
                          virtual_device_factory_->mutable_state()->pin);
                EXPECT_TRUE(HasUV(result.response));
                break;

              default:
                NOTREACHED();
            }
          }
        }
      }
    }
  }
}

TEST_F(PINAuthenticatorImplTest, GetAssertionSoftLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, kTestRelyingPartyId));

  test_client_.expected = {{PINReason::kChallenge, u"wrong", 8},
                           {PINReason::kChallenge, u"wrong", 7,
                            device::kMinPinLength, PINError::kWrongPIN},
                           {PINReason::kChallenge, u"wrong", 6,
                            device::kMinPinLength, PINError::kWrongPIN}};
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->pin_retries);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->soft_locked);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kSoftPINBlock,
            *test_client_.failure_reason);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kSoftPinBlock,
                               AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(PINAuthenticatorImplTest, GetAssertionHardLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries = 1;

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, kTestRelyingPartyId));

  test_client_.expected = {{PINReason::kChallenge, u"wrong", 1}};
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(0, virtual_device_factory_->mutable_state()->pin_retries);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kHardPINBlock,
            *test_client_.failure_reason);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kHardPinBlock,
                               AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(PINAuthenticatorImplTest, GetAssertionSkipPINTouch) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  int taps = 0;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        ++taps;
        return true;
      });
  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, kTestRelyingPartyId));
  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(taps, 1);
}

TEST_F(PINAuthenticatorImplTest, GetAssertionDontSkipPINTouch) {
  // Create two devices. Both are candidates but only the second one will
  // respond to touches.
  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
  device_1.config.pin_support = true;
  device_1.state->simulate_press_callback =
      base::BindRepeating([](VirtualFidoDevice* ignore) { return false; });
  discovery->AddDevice(std::move(device_1));

  int taps = 0;
  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_2;
  device_2.state->pin = kTestPIN;
  device_2.config.pin_support = true;
  device_2.state->simulate_press_callback =
      base::BindLambdaForTesting([&](VirtualFidoDevice* ignore) {
        ++taps;
        return true;
      });
  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(device_2.state->InjectRegistration(
      options->allow_credentials[0].id, kTestRelyingPartyId));
  discovery->AddDevice(std::move(device_2));

  ReplaceDiscoveryFactory(std::move(discovery));

  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(taps, 2);
}

TEST_F(PINAuthenticatorImplTest, GetAssertionAlwaysUv) {
  // Test that if an authenticator is reporting alwaysUv = 1, UV is attempted
  // even if the user verification requirement is discouraged.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.always_uv = true;
  config.u2f_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  PublicKeyCredentialRequestOptionsPtr options =
      get_credential_options(device::UserVerificationRequirement::kDiscouraged);
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, kTestRelyingPartyId));
  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialNoSupportedAlgorithm) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (int i = 0; i < 3; i++) {
    SCOPED_TRACE(i);

    test_client_.expected.clear();
    bool expected_to_succeed = false;
    if (i == 0) {
      device::VirtualCtap2Device::Config config;
      // The first config is a CTAP2 device that doesn't support the
      // kInvalidForTesting algorithm. A dummy touch should be requested in this
      // case.
      virtual_device_factory_->SetCtap2Config(config);
    } else if (i == 1) {
      device::VirtualCtap2Device::Config config;
      // The second config is a device with a PIN set that _does_ support the
      // algorithm. Since the PIN is set, we might convert the makeCredential
      // request to U2F, but shouldn't because the algorithm cannot be
      // represented in U2F.
      config.u2f_support = true;
      config.pin_support = true;
      config.advertised_algorithms = {
          device::CoseAlgorithmIdentifier::kInvalidForTesting};
      virtual_device_factory_->mutable_state()->pin = kTestPIN;
      virtual_device_factory_->mutable_state()->pin_retries =
          device::kMaxPinRetries;
      virtual_device_factory_->SetCtap2Config(config);
      test_client_.expected = {
          {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
      // Since converting to U2F isn't possible, this will trigger a PIN prompt
      // and succeed because the device does actually support the algorithm.
      expected_to_succeed = true;
    } else if (i == 2) {
      // The third case is a plain U2F authenticator, which implicitly only
      // supports ES256.
      virtual_device_factory_->SetSupportedProtocol(
          device::ProtocolVersion::kU2f);
    }

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    // Set uv=discouraged so that U2F fallback is possible.
    options->authenticator_selection->user_verification_requirement =
        device::UserVerificationRequirement::kDiscouraged;
    options->public_key_parameters =
        GetTestPublicKeyCredentialParameters(static_cast<int32_t>(
            device::CoseAlgorithmIdentifier::kInvalidForTesting));

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              expected_to_succeed ? AuthenticatorStatus::SUCCESS
                                  : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(PINAuthenticatorImplTest, PRFCreatedOnCTAP2) {
  // Check that credential creation requests that include the PRF extension use
  // CTAP2 if possible.
  NavigateAndCommit(GURL(kTestOrigin1));

  for (int i = 0; i < 3; i++) {
    SCOPED_TRACE(i);

    device::VirtualCtap2Device::Config config;
    config.u2f_support = true;
    config.pin_support = true;
    config.hmac_secret_support = true;
    virtual_device_factory_->mutable_state()->pin = kTestPIN;
    virtual_device_factory_->mutable_state()->pin_retries =
        device::kMaxPinRetries;

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    // Set uv=discouraged so that U2F fallback is possible.
    options->authenticator_selection->user_verification_requirement =
        device::UserVerificationRequirement::kDiscouraged;

    if (i == 0) {
      // Sanity check: request should fallback to U2F. (If it doesn't fallback
      // to U2F then the PIN test infrastructure will CHECK because
      // |test_client_.expected| is empty.)
      test_client_.expected.clear();
    } else if (i == 1) {
      // If PRF is requested, the fallback to U2F should not happen because the
      // PRF request is higher priority than avoiding a PIN prompt. (The PIN
      // test infrastructure will CHECK if |expected| is set and not used.)
      options->prf_enable = true;
      test_client_.expected = {
          {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
    } else {
      // If PRF is requested, but the authenticator doesn't support it, then we
      // should still use U2F.
      options->prf_enable = true;
      config.hmac_secret_support = false;
      test_client_.expected.clear();
    }

    virtual_device_factory_->SetCtap2Config(config);

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
  }
}

// Test that pinUvAuthToken gets sent with every single batch of an exclude
// list. If it wasn't, any batch after the first would be unable to match
// credProtect=uvRequired credentials.
TEST_F(PINAuthenticatorImplTest, ExcludeListBatchesIncludePinToken) {
  NavigateAndCommit(GURL(kTestOrigin1));

  // Set up a CTAP 2.1 authenticator with pinUvAuthToken and exclude list
  // batching.
  device::VirtualCtap2Device::Config config;
  config.max_credential_id_length = kTestCredentialIdLength;
  constexpr size_t kBatchSize = 10;
  config.max_credential_count_in_list = kBatchSize;
  config.pin_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_0,
                           device::Ctap2Version::kCtap2_1};
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};

  // Craft an exclude list that is large enough to trigger batched probing and
  // includes one match for a credProtect=uvRequired credential.
  auto test_credentials = GetTestCredentials(kBatchSize + 1);

  device::VirtualFidoDevice::RegistrationData cred_protect_credential(
      kTestRelyingPartyId);
  cred_protect_credential.protection = device::CredProtect::kUVRequired;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      test_credentials.back().id, std::move(cred_protect_credential)));

  // The request should fail because the exclude list matches.
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = std::move(test_credentials);
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::CREDENTIAL_EXCLUDED);
}

TEST_F(PINAuthenticatorImplTest, RemoveSecondAuthenticator) {
  // Create two PIN-capable devices. Touch one of them to trigger a prompt for
  // a PIN. Remove the other. Don't crash.
  base::RepeatingCallback<void(bool)> disconnect_1, disconnect_2;

  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
  device_1.state->pin = kTestPIN;
  device_1.config.pin_support = true;
  std::tie(disconnect_1, device_1.disconnect_events) =
      device::FidoDiscoveryBase::EventStream<bool>::New();

  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_2;
  device_2.state->pin = kTestPIN;
  device_2.config.pin_support = true;
  std::tie(disconnect_2, device_2.disconnect_events) =
      device::FidoDiscoveryBase::EventStream<bool>::New();

  int callbacks = 0;
  auto touch_callback = [&](int device_num) -> bool {
    callbacks++;
    if (callbacks == 1) {
      // Wait for the other authenticator to be triggered.
      return false;
    } else if (callbacks == 2) {
      // Touch authenticator to collect a PIN.
      return true;
    } else {
      CHECK_EQ(callbacks, 3);

      // Disconnect other authenticator then complete with a touch.
      if (device_num == 1) {
        disconnect_2.Run(false);
      } else {
        disconnect_1.Run(false);
      }
      return true;
    }
  };

  device_1.state->simulate_press_callback = base::BindLambdaForTesting(
      [&](VirtualFidoDevice* ignore) -> bool { return touch_callback(1); });
  device_2.state->simulate_press_callback = base::BindLambdaForTesting(
      [&](VirtualFidoDevice* ignore) -> bool { return touch_callback(2); });

  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  discovery->AddDevice(std::move(device_1));
  discovery->AddDevice(std::move(device_2));
  ReplaceDiscoveryFactory(std::move(discovery));

  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
}

TEST_F(PINAuthenticatorImplTest,
       RemoveAuthenticatorDuringRegistrationPINPrompt) {
  // Regression test for crbug.com/370000838: removing an authenticator while
  // the PIN prompt was showing would crash.
  base::RepeatingCallback<void(bool)> disconnect_1;
  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
  device_1.state->pin = kTestPIN;
  device_1.config.pin_support = true;
  std::tie(disconnect_1, device_1.disconnect_events) =
      device::FidoDiscoveryBase::EventStream<bool>::New();

  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  discovery->AddDevice(std::move(device_1));
  ReplaceDiscoveryFactory(std::move(discovery));

  test_client_.collect_pin_cb =
      base::BindLambdaForTesting([&disconnect_1]() -> bool {
        disconnect_1.Run(false);
        return false;
      });

  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(PINAuthenticatorImplTest, RemoveAuthenticatorDuringAssertionPINPrompt) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id, kTestRelyingPartyId));

  base::RepeatingCallback<void(bool)> disconnect_1;
  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
  device_1.state->pin = kTestPIN;
  device_1.config.pin_support = true;
  std::tie(disconnect_1, device_1.disconnect_events) =
      device::FidoDiscoveryBase::EventStream<bool>::New();

  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  discovery->AddDevice(std::move(device_1));
  ReplaceDiscoveryFactory(std::move(discovery));

  test_client_.collect_pin_cb =
      base::BindLambdaForTesting([&disconnect_1]() -> bool {
        disconnect_1.Run(false);
        return false;
      });

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->user_verification = device::UserVerificationRequirement::kRequired;
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(PINAuthenticatorImplTest, AppIdExcludeExtensionWithPinRequiredError) {
  // Some alwaysUv authenticators apply the alwaysUv logic even when up=false.
  // That causes them to return `kCtap2ErrPinRequired` to appIdExclude probes
  // which broke makeCredential at one point. See crbug.com/1443039.
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.always_uv = true;
  config.always_uv_for_up_false = true;
  config.pin_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  virtual_device_factory_->SetCtap2Config(config);

  test_client_.expected = {{PINReason::kSet, kTestPIN16}};

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->user_verification_requirement =
      device::UserVerificationRequirement::kRequired;
  options->appid_exclude = kTestOrigin1;
  options->exclude_credentials = GetTestCredentials();

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

class InternalUVAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  struct TestCase {
    const bool fingerprints_enrolled;
    const bool supports_pin;
    const device::UserVerificationRequirement uv;
  };

  InternalUVAuthenticatorImplTest() = default;

  InternalUVAuthenticatorImplTest(const InternalUVAuthenticatorImplTest&) =
      delete;
  InternalUVAuthenticatorImplTest& operator=(
      const InternalUVAuthenticatorImplTest&) = delete;

  void SetUp() override {
    UVAuthenticatorImplTest::SetUp();
    NavigateAndCommit(GURL(kTestOrigin1));
  }

  std::vector<TestCase> GetTestCases() {
    std::vector<TestCase> test_cases;
    for (const bool fingerprints_enrolled : {true, false}) {
      for (const bool supports_pin : {true, false}) {
        // Avoid just testing for PIN.
        if (!fingerprints_enrolled && supports_pin) {
          continue;
        }
        for (const auto uv : {device::UserVerificationRequirement::kDiscouraged,
                              device::UserVerificationRequirement::kPreferred,
                              device::UserVerificationRequirement::kRequired}) {
          test_cases.push_back({fingerprints_enrolled, supports_pin, uv});
        }
      }
    }
    return test_cases;
  }

  void ConfigureDevice(const TestCase& test_case) {
    device::VirtualCtap2Device::Config config;
    config.internal_uv_support = true;
    config.u2f_support = true;
    config.pin_support = test_case.supports_pin;
    virtual_device_factory_->mutable_state()->pin = kTestPIN;
    virtual_device_factory_->mutable_state()->pin_retries =
        device::kMaxPinRetries;
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        test_case.fingerprints_enrolled;
    virtual_device_factory_->SetCtap2Config(config);
    SCOPED_TRACE(::testing::Message() << "fingerprints_enrolled="
                                      << test_case.fingerprints_enrolled);
    SCOPED_TRACE(::testing::Message()
                 << "supports_pin=" << test_case.supports_pin);
    SCOPED_TRACE(device::ToString(test_case.uv));
  }
};

TEST_F(InternalUVAuthenticatorImplTest, MakeCredential) {
  for (const auto test_case : GetTestCases()) {
    ConfigureDevice(test_case);

    auto options = make_credential_options(test_case.uv);
    // UV cannot be satisfied without fingerprints.
    const bool should_timeout =
        !test_case.fingerprints_enrolled &&
        test_case.uv == device::UserVerificationRequirement::kRequired;
    if (should_timeout) {
      options->timeout = base::Milliseconds(100);
    }

    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));

    if (should_timeout) {
      EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
    } else {
      EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
      EXPECT_EQ(test_case.fingerprints_enrolled, HasUV(result.response));
    }
  }
}

// Test falling back to PIN for devices that support internal user verification
// but not uv token.
TEST_F(InternalUVAuthenticatorImplTest, MakeCredentialFallBackToPin) {
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.pin_support = true;
  config.user_verification_succeeds = false;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->SetCtap2Config(config);

  auto options =
      make_credential_options(device::UserVerificationRequirement::kRequired);

  MakeCredentialResult result = AuthenticatorMakeCredential(std::move(options));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
}

// Test making a credential on an authenticator that supports biometric
// enrollment but has no fingerprints enrolled.
TEST_F(InternalUVAuthenticatorImplTest, MakeCredentialInlineBioEnrollment) {
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.pin_support = true;
  config.user_verification_succeeds = true;
  config.bio_enrollment_support = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = false;
  virtual_device_factory_->SetCtap2Config(config);

  MakeCredentialResult result = AuthenticatorMakeCredential(
      make_credential_options(device::UserVerificationRequirement::kRequired));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_TRUE(test_client_.did_bio_enrollment);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->fingerprints_enrolled);
}

// Test having an authenticator report an error during inline biometrics
// enrollment. Regression test for crbug.com/410985009.
TEST_F(InternalUVAuthenticatorImplTest,
       MakeCredentialInlineBioEnrollmentError) {
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.pin_support = true;
  config.user_verification_succeeds = true;
  config.bio_enrollment_support = true;
  config.override_response_map
      [device::CtapRequestCommand::kAuthenticatorBioEnrollment] =
      std::make_pair(device::CtapDeviceResponseCode::kCtap2ErrUserActionTimeout,
                     std::nullopt);
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = false;
  virtual_device_factory_->SetCtap2Config(config);

  MakeCredentialResult result = AuthenticatorMakeCredential(
      make_credential_options(device::UserVerificationRequirement::kRequired));

  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_TRUE(test_client_.did_bio_enrollment);
  EXPECT_FALSE(virtual_device_factory_->mutable_state()->fingerprints_enrolled);
}

// Regression test for https://crbug.com/442489275.
// Tests that removing the authenticator during bio enrollment does not leave a
// dangling pointer.
TEST_F(InternalUVAuthenticatorImplTest,
       InlineBioEnrollmentRemoveAuthenticator) {
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.pin_support = true;
  config.user_verification_succeeds = true;
  config.bio_enrollment_support = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = false;
  virtual_device_factory_->SetCtap2Config(config);

  // Stall the biometric enrollment so we can disconnect the authenticator.
  base::test::TestFuture<void> touch_fingerprint_future;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        touch_fingerprint_future.SetValue();
        return false;
      });

  // Block the request on failure, otherwise the request ends immediately and
  // the bug doesn't trigger.
  test_client_.block_request_on_failure_once = true;

  // Make a credential and wait for the touch.
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestMakeCredentialFuture request_future;
  authenticator->MakeCredential(
      make_credential_options(device::UserVerificationRequirement::kRequired),
      request_future.GetCallback());
  ASSERT_TRUE(touch_fingerprint_future.Wait());

  // Disconnect the device, which should clean up the bio enroller.
  virtual_device_factory_->DisconnectDevice();

  // Time the request out.
  task_environment()->FastForwardBy(kTestTimeout);
  ASSERT_TRUE(request_future.Wait());
  // No crashes due to dangling pointers.
}

// Test making a credential skipping biometric enrollment during credential
// creation.
TEST_F(InternalUVAuthenticatorImplTest, MakeCredentialSkipInlineBioEnrollment) {
  test_client_.cancel_bio_enrollment = true;

  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.pin_support = true;
  config.user_verification_succeeds = true;
  config.bio_enrollment_support = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = false;
  virtual_device_factory_->SetCtap2Config(config);

  MakeCredentialResult result = AuthenticatorMakeCredential(
      make_credential_options(device::UserVerificationRequirement::kRequired));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_TRUE(test_client_.did_bio_enrollment);
  EXPECT_FALSE(virtual_device_factory_->mutable_state()->fingerprints_enrolled);
}

TEST_F(InternalUVAuthenticatorImplTest, MakeCredUvNotRqd) {
  // Test that on an authenticator with the makeCredUvNotRqd option enabled,
  // non-discoverable credentials can be created without requiring UV or a PIN.
  for (bool discoverable : {false, true}) {
    for (bool request_uv : {false, true}) {
      SCOPED_TRACE(testing::Message() << "discoverable=" << discoverable
                                      << " request_uv=" << request_uv);

      test_client_.web_authentication_delegate.supports_resident_keys = true;
      ResetVirtualDevice();
      device::VirtualCtap2Device::Config config;
      config.u2f_support = true;
      config.internal_uv_support = true;
      config.user_verification_succeeds = true;
      config.pin_support = true;
      config.resident_key_support = true;
      config.pin_uv_auth_token_support = true;
      config.allow_non_resident_credential_creation_without_uv = true;
      config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
      virtual_device_factory_->SetCtap2Config(config);
      virtual_device_factory_->mutable_state()->pin = kTestPIN;
      virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

      PublicKeyCredentialCreationOptionsPtr request = make_credential_options();
      request->authenticator_selection->user_verification_requirement =
          request_uv ? device::UserVerificationRequirement::kPreferred
                     : device::UserVerificationRequirement::kDiscouraged;
      request->authenticator_selection->resident_key =
          discoverable ? device::ResidentKeyRequirement::kPreferred
                       : device::ResidentKeyRequirement::kDiscouraged;

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(request));
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
      EXPECT_EQ(HasUV(result.response), discoverable || request_uv);
      EXPECT_FALSE(test_client_.collected_pin);
      // Requests shouldn't fall back to creating U2F credentials.
      EXPECT_FALSE(virtual_device_factory_->mutable_state()
                       ->registrations.begin()
                       ->second.is_u2f);
    }
  }
}

TEST_F(InternalUVAuthenticatorImplTest, GetAssertion) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id, kTestRelyingPartyId));

  for (const auto test_case : GetTestCases()) {
    ConfigureDevice(test_case);
    // Without a fingerprint enrolled we assume that a UV=required request
    // cannot be satisfied by an authenticator that cannot do UV. It is
    // possible for a credential to be created without UV and then later
    // asserted with UV=required, but that would be bizarre behaviour from
    // an RP and we currently don't worry about it.
    const bool should_be_unrecognized =
        !test_case.fingerprints_enrolled &&
        test_case.uv == device::UserVerificationRequirement::kRequired;

    GetAssertionResult result =
        AuthenticatorGetAssertion(get_credential_options(test_case.uv));

    if (should_be_unrecognized) {
      EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
    } else {
      EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
      EXPECT_EQ(
          test_case.fingerprints_enrolled &&
              test_case.uv != device::UserVerificationRequirement::kDiscouraged,
          HasUV(result.response));
    }
  }
}

// Test falling back to PIN for devices that support internal user verification
// but not uv token.
TEST_F(InternalUVAuthenticatorImplTest, GetAssertionFallbackToPIN) {
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.pin_support = true;
  config.user_verification_succeeds = false;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->SetCtap2Config(config);

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id, kTestRelyingPartyId));

  GetAssertionResult result = AuthenticatorGetAssertion(
      get_credential_options(device::UserVerificationRequirement::kRequired));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
}

class UVTokenAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  UVTokenAuthenticatorImplTest() = default;
  UVTokenAuthenticatorImplTest(const UVTokenAuthenticatorImplTest&) = delete;

  void SetUp() override {
    UVAuthenticatorImplTest::SetUp();
    device::VirtualCtap2Device::Config config;
    config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
    config.internal_uv_support = true;
    config.pin_uv_auth_token_support = true;
    virtual_device_factory_->SetCtap2Config(config);
    NavigateAndCommit(GURL(kTestOrigin1));
  }
};

TEST_F(UVTokenAuthenticatorImplTest, GetAssertionUVToken) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id, kTestRelyingPartyId));

  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;

    for (auto uv : {device::UserVerificationRequirement::kDiscouraged,
                    device::UserVerificationRequirement::kPreferred,
                    device::UserVerificationRequirement::kRequired}) {
      SCOPED_TRACE(device::ToString(uv));

      // Without a fingerprint enrolled we assume that a UV=required request
      // cannot be satisfied by an authenticator that cannot do UV. It is
      // possible for a credential to be created without UV and then later
      // asserted with UV=required, but that would be bizarre behaviour from
      // an RP and we currently don't worry about it.
      const bool should_be_unrecognized =
          !fingerprints_enrolled &&
          uv == device::UserVerificationRequirement::kRequired;

      GetAssertionResult result =
          AuthenticatorGetAssertion(get_credential_options(uv));

      if (should_be_unrecognized) {
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
      } else {
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
        EXPECT_EQ(fingerprints_enrolled &&
                      uv != device::UserVerificationRequirement::kDiscouraged,
                  HasUV(result.response));
      }
    }
  }
}

// Test exhausting all internal user verification attempts on an authenticator
// that does not support PINs.
TEST_F(UVTokenAuthenticatorImplTest, GetAssertionUvFails) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = false;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id, kTestRelyingPartyId));

  int expected_retries = 5;
  virtual_device_factory_->mutable_state()->uv_retries = expected_retries;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        EXPECT_EQ(--expected_retries,
                  virtual_device_factory_->mutable_state()->uv_retries);
        return true;
      });

  EXPECT_EQ(AuthenticatorGetAssertion(get_credential_options()).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kUvNotSupported,
                               AuthenticationRequestMode::kModalWebAuthn);
  EXPECT_EQ(0, expected_retries);
}

// Test exhausting all internal user verification attempts on an authenticator
// that supports PINs.
TEST_F(UVTokenAuthenticatorImplTest, GetAssertionFallBackToPin) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id, kTestRelyingPartyId));

  int taps = 0;
  virtual_device_factory_->mutable_state()->uv_retries = 5;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        ++taps;
        return true;
      });

  EXPECT_EQ(AuthenticatorGetAssertion(get_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  // 5 retries + 1 tap for the actual get assertion request.
  EXPECT_EQ(taps, 6);
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->uv_retries);
}

// Tests that a device supporting UV token with UV blocked at the start of a get
// assertion request gets a touch and then falls back to PIN.
TEST_F(UVTokenAuthenticatorImplTest, GetAssertionUvBlockedFallBackToPin) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = true;

  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->uv_retries = 0;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id, kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorGetAssertion(get_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->uv_retries);
}

TEST_F(UVTokenAuthenticatorImplTest, MakeCredentialUVToken) {
  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;

    for (const auto uv : {device::UserVerificationRequirement::kDiscouraged,
                          device::UserVerificationRequirement::kPreferred,
                          device::UserVerificationRequirement::kRequired}) {
      SCOPED_TRACE(device::ToString(uv));

      // UV cannot be satisfied without fingerprints.
      const bool should_timeout =
          !fingerprints_enrolled &&
          uv == device::UserVerificationRequirement::kRequired;

      if (should_timeout) {
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                  AuthenticatorMakeCredentialAndWaitForTimeout(
                      make_credential_options(uv))
                      .status);
      } else {
        MakeCredentialResult result =
            AuthenticatorMakeCredential(make_credential_options(uv));
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
        EXPECT_EQ(fingerprints_enrolled, HasUV(result.response));
      }
    }
  }
}

// Test exhausting all internal user verification attempts on an authenticator
// that does not support PINs.
TEST_F(UVTokenAuthenticatorImplTest, MakeCredentialUvFails) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = false;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id, kTestRelyingPartyId));

  int expected_retries = 5;
  virtual_device_factory_->mutable_state()->uv_retries = expected_retries;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        EXPECT_EQ(--expected_retries,
                  virtual_device_factory_->mutable_state()->uv_retries);
        return true;
      });

  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(0, expected_retries);
}

// Test exhausting all internal user verification attempts on an authenticator
// that supports PINs.
TEST_F(UVTokenAuthenticatorImplTest, MakeCredentialFallBackToPin) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id, kTestRelyingPartyId));

  int taps = 0;
  virtual_device_factory_->mutable_state()->uv_retries = 5;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        ++taps;
        return true;
      });

  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  // 5 retries + 1 tap for the actual get assertion request.
  EXPECT_EQ(taps, 6);
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->uv_retries);
}

// Tests that a device supporting UV token with UV blocked at the start of a get
// assertion request gets a touch and then falls back to PIN.
TEST_F(UVTokenAuthenticatorImplTest, MakeCredentialUvBlockedFallBackToPin) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = true;

  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->uv_retries = 0;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id, kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->uv_retries);
}

}  // namespace
}  // namespace content
