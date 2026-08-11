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
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/authenticator_impl_unittest_test_base.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_authenticator_manager_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_authentication_delegate.h"
#include "content/public/common/content_client.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_user_verification_requirement.h"
#include "device/fido/large_blob.h"
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
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/apple/scoped_fake_keychain_v2.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/icloud_keychain.h"
#include "device/fido/mac/scoped_icloud_keychain_test_environment.h"
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "content/public/test/test_browser_context.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/util.h"
#include "third_party/microsoft_webauthn/src/webauthn.h"  // nogncheck
#endif

namespace content {
namespace {

using device::VirtualCtap2Device;
using device::VirtualFidoDevice;

// ResidentKeyTestAuthenticatorRequestDelegate is a delegate that:
//   a) always returns |kTestPIN| when asked for a PIN.
//   b) sorts potential resident-key accounts by user ID, maps them to a string
//      form ("<hex user ID>:<user name>:<display name>"), joins the strings
//      with "/", and compares the result against |expected_accounts|.
//   c) auto-selects the account with the user ID matching |selected_user_id|.
class ResidentKeyTestAuthenticatorRequestDelegate
    : public DefaultAuthenticatorRequestClientDelegate {
 public:
  struct Config {
    // A string representation of the accounts expected to be passed to
    // `SelectAccount()`.
    std::string expected_accounts;

    // The user ID of the account that should be selected by `SelectAccount()`.
    std::vector<uint8_t> selected_user_id;

    // Indicates whether `SetUIPresentation(kAutofill)` is expected to be
    // called.
    bool expect_conditional = false;

    // Indicates whether `RegisterActionCallbacks()` should run the cancel UI
    // timeout callback.
    bool run_cancel_ui_timeout_callback = false;

    // If set, indicates that `DoesBlockRequestOnFailure()` is expected to be
    // called with this value.
    std::optional<AuthenticatorRequestClientDelegate::InterestingFailureReason>
        expected_failure_reason;

    // If set, indicates that the `AccountPreselectCallback` should be invoked
    // with this credential ID at the beginning of the request.
    // `preselected_authenticator_id` contains the authenticator ID to which the
    // request should be dispatched in this case.
    std::optional<std::vector<uint8_t>> preselected_credential_id;
    std::optional<std::string> preselected_authenticator_id;
  };

  explicit ResidentKeyTestAuthenticatorRequestDelegate(Config config)
      : config_(std::move(config)) {}

  ~ResidentKeyTestAuthenticatorRequestDelegate() override {
    CHECK(!config_.expect_conditional || expect_conditional_satisfied_)
        << "SetUIPresentation(kAutofill) expected but not called";
    DCHECK(!config_.expected_failure_reason ||
           expected_failure_reason_satisfied_)
        << "DoesRequestBlockOnFailure() expected but not called";
  }

  ResidentKeyTestAuthenticatorRequestDelegate(
      const ResidentKeyTestAuthenticatorRequestDelegate&) = delete;
  ResidentKeyTestAuthenticatorRequestDelegate& operator=(
      const ResidentKeyTestAuthenticatorRequestDelegate&) = delete;

  bool SupportsPIN() const override { return true; }

  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(provide_pin_cb),
                                  AuthenticatorImplTest::kTestPIN16));
  }

  void FinishCollectToken() override {}

  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::OnceClosure immediate_not_found_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      PasswordSelectedCallback password_selected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::OnceClosure cancel_ui_timeout_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback,
      base::RepeatingCallback<
          void(device::FidoRequestHandlerBase::BlePermissionCallback)>
          ble_status_callback) override {
    account_preselected_callback_ = account_preselected_callback;
    request_callback_ = request_callback;
    if (config_.run_cancel_ui_timeout_callback) {
      std::move(cancel_ui_timeout_callback).Run();
    }
  }

  void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback) override {
    std::sort(responses.begin(), responses.end(),
              [](const device::AuthenticatorGetAssertionResponse& a,
                 const device::AuthenticatorGetAssertionResponse& b) {
                return a.user_entity->id < b.user_entity->id;
              });

    std::vector<std::string> string_reps;
    std::ranges::transform(
        responses, std::back_inserter(string_reps),
        [](const device::AuthenticatorGetAssertionResponse& response) {
          const device::PublicKeyCredentialUserEntity& user =
              response.user_entity.value();
          return base::HexEncode(user.id) + ":" + user.name.value_or("") + ":" +
                 user.display_name.value_or("");
        });

    EXPECT_EQ(config_.expected_accounts, base::JoinString(string_reps, "/"));

    const auto selected = std::ranges::find(
        responses, config_.selected_user_id,
        [](const device::AuthenticatorGetAssertionResponse& response) {
          return response.user_entity->id;
        });
    ASSERT_TRUE(selected != responses.end());

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(*selected)));
  }

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override {
    if (config_.expected_failure_reason) {
      EXPECT_EQ(*config_.expected_failure_reason, reason);
      expected_failure_reason_satisfied_ = true;
    }
    return AuthenticatorRequestClientDelegate::DoesBlockRequestOnFailure(
        reason);
  }

  void SetUIPresentation(UIPresentation ui_presentation) override {
    if (config_.expect_conditional) {
      EXPECT_EQ(ui_presentation, UIPresentation::kAutofill);
    } else {
      EXPECT_TRUE(ui_presentation == UIPresentation::kModal ||
                  ui_presentation == UIPresentation::kModalImmediate);
    }
    EXPECT_TRUE(!expect_conditional_satisfied_);
    expect_conditional_satisfied_ = true;
  }

  bool EmbedderControlsAuthenticatorDispatch(
      const device::FidoAuthenticator& authenticator) override {
    // Don't instantly dispatch platform authenticator requests if the test is
    // exercising platform credential preselection.
    // `OnTransportAvailabilityEnumerated()` will run the `request_callback_` in
    // this case to mimic behavior of the real UI.
    return authenticator.AuthenticatorTransport() ==
               device::FidoTransportProtocol::kInternal &&
           config_.preselected_credential_id;
  }

  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo info) override {
    if (config_.preselected_credential_id) {
      DCHECK(config_.preselected_authenticator_id);
      EXPECT_EQ(info.has_platform_authenticator_credential,
                device::FidoRequestHandlerBase::RecognizedCredential::
                    kHasRecognizedCredential);
      const auto cred = std::ranges::find(
          info.recognized_credentials, *config_.preselected_credential_id,
          &device::DiscoverableCredentialMetadata::cred_id);
      ASSERT_NE(cred, info.recognized_credentials.end());
      std::move(account_preselected_callback_).Run(*cred);
      request_callback_.Run(*config_.preselected_authenticator_id);
    }
  }

 private:
  const Config config_;
  bool expect_conditional_satisfied_ = false;
  bool expected_failure_reason_satisfied_ = false;
  device::FidoRequestHandlerBase::RequestCallback request_callback_;
  AccountPreselectedCallback account_preselected_callback_;
  base::OnceClosure cancel_ui_timeout_callback_;
};

class ResidentKeyTestAuthenticatorContentBrowserClient
    : public ContentBrowserClient {
 public:
  ResidentKeyTestAuthenticatorContentBrowserClient() {
    web_authentication_delegate.supports_resident_keys = true;
  }

  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<ResidentKeyTestAuthenticatorRequestDelegate>(
        delegate_config);
  }

  TestWebAuthenticationDelegate web_authentication_delegate;

  ResidentKeyTestAuthenticatorRequestDelegate::Config delegate_config;
};

class ResidentKeyAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  ResidentKeyAuthenticatorImplTest(const ResidentKeyAuthenticatorImplTest&) =
      delete;
  ResidentKeyAuthenticatorImplTest& operator=(
      const ResidentKeyAuthenticatorImplTest&) = delete;

 protected:
  ResidentKeyAuthenticatorImplTest() = default;

  void SetUp() override {
    UVAuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.resident_key_support = true;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->pin = kTestPIN;
    virtual_device_factory_->mutable_state()->pin_retries =
        device::kMaxPinRetries;
    NavigateAndCommit(GURL(kTestOrigin1));
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    UVAuthenticatorImplTest::TearDown();
  }

  static PublicKeyCredentialCreationOptionsPtr make_credential_options(
      device::ResidentKeyRequirement resident_key =
          device::ResidentKeyRequirement::kRequired) {
    PublicKeyCredentialCreationOptionsPtr options =
        UVAuthenticatorImplTest::make_credential_options();
    options->authenticator_selection->resident_key = resident_key;
    options->user.id = {1, 2, 3, 4};
    return options;
  }

  static PublicKeyCredentialRequestOptionsPtr get_credential_options() {
    PublicKeyCredentialRequestOptionsPtr options =
        UVAuthenticatorImplTest::get_credential_options();
    options->allow_credentials.clear();
    return options;
  }

  ResidentKeyTestAuthenticatorContentBrowserClient test_client_;

 private:
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialRkRequired) {
  for (const bool internal_uv : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "internal_uv=" << internal_uv);

    if (internal_uv) {
      device::VirtualCtap2Device::Config config;
      config.resident_key_support = true;
      config.internal_uv_support = true;
      virtual_device_factory_->SetCtap2Config(config);
      virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
    }

    MakeCredentialResult result =
        AuthenticatorMakeCredential(make_credential_options());

    EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(HasUV(result.response));
    ASSERT_EQ(1u,
              virtual_device_factory_->mutable_state()->registrations.size());
    const device::VirtualFidoDevice::RegistrationData& registration =
        virtual_device_factory_->mutable_state()->registrations.begin()->second;
    EXPECT_TRUE(registration.is_resident);
    ASSERT_TRUE(registration.user.has_value());
    const auto options = make_credential_options();
    EXPECT_EQ(options->user.name, registration.user->name);
    EXPECT_EQ(options->user.display_name, registration.user->display_name);
    EXPECT_EQ(options->user.id, registration.user->id);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialRkPreferred) {
  for (const bool supports_rk : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "supports_rk=" << supports_rk);
    ResetVirtualDevice();

    device::VirtualCtap2Device::Config config;
    config.internal_uv_support = true;
    config.resident_key_support = supports_rk;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

    MakeCredentialResult result = AuthenticatorMakeCredential(
        make_credential_options(device::ResidentKeyRequirement::kPreferred));

    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(HasUV(result.response));
    ASSERT_EQ(1u,
              virtual_device_factory_->mutable_state()->registrations.size());
    const device::VirtualFidoDevice::RegistrationData& registration =
        virtual_device_factory_->mutable_state()->registrations.begin()->second;
    EXPECT_EQ(registration.is_resident, supports_rk);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialRkPreferredStorageFull) {
  // Making a credential on an authenticator with full storage falls back to
  // making a non-resident key.
  for (bool is_ctap_2_1 : {false, true}) {
    ResetVirtualDevice();

    size_t num_taps = 0;
    virtual_device_factory_->mutable_state()->simulate_press_callback =
        base::BindLambdaForTesting(
            [&num_taps](device::VirtualFidoDevice* device) {
              num_taps++;
              return true;
            });

    device::VirtualCtap2Device::Config config;
    if (is_ctap_2_1) {
      config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                               std::end(device::kCtap2Versions2_1)};
    }

    config.internal_uv_support = true;
    config.resident_key_support = true;
    config.resident_credential_storage = 0;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

    MakeCredentialResult result = AuthenticatorMakeCredential(
        make_credential_options(device::ResidentKeyRequirement::kPreferred));

    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(HasUV(result.response));
    ASSERT_EQ(1u,
              virtual_device_factory_->mutable_state()->registrations.size());
    const device::VirtualFidoDevice::RegistrationData& registration =
        virtual_device_factory_->mutable_state()->registrations.begin()->second;
    EXPECT_EQ(registration.is_resident, false);
    // In CTAP 2.0, the first request with rk=false fails due to exhausted
    // storage and then needs to be retried with rk=false, requiring a second
    // tap. In 2.1 remaining storage capacity can be checked up front such that
    // the request is sent with rk=false right away.
    EXPECT_EQ(num_taps, is_ctap_2_1 ? 1u : 2u);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest,
       MakeCredentialRkPreferredStorageFull_LargeBlob) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                           std::end(device::kCtap2Versions2_1)};
  config.internal_uv_support = true;
  config.resident_key_support = true;
  config.resident_credential_storage = 0;
  config.large_blob_support = true;
  config.pin_uv_auth_token_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  {
    PublicKeyCredentialCreationOptionsPtr options =
        make_credential_options(device::ResidentKeyRequirement::kPreferred);
    options->large_blob_enable = device::LargeBlobSupport::kRequired;
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
  }
  {
    PublicKeyCredentialCreationOptionsPtr options =
        make_credential_options(device::ResidentKeyRequirement::kPreferred);
    options->large_blob_enable = device::LargeBlobSupport::kPreferred;
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(result.response->echo_large_blob);
    EXPECT_FALSE(result.response->supports_large_blob);
    EXPECT_EQ(1u,
              virtual_device_factory_->mutable_state()->registrations.size());
    const device::VirtualFidoDevice::RegistrationData& registration =
        virtual_device_factory_->mutable_state()->registrations.begin()->second;
    EXPECT_FALSE(registration.is_resident);
    EXPECT_FALSE(registration.large_blob);
    EXPECT_FALSE(registration.large_blob_key);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialRkPreferredSetsPIN) {
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.internal_uv_support = false;
  config.resident_key_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->pin = "";

  MakeCredentialResult result = AuthenticatorMakeCredential(
      make_credential_options(device::ResidentKeyRequirement::kPreferred));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  ASSERT_EQ(1u, virtual_device_factory_->mutable_state()->registrations.size());
  const device::VirtualFidoDevice::RegistrationData& registration =
      virtual_device_factory_->mutable_state()->registrations.begin()->second;
  EXPECT_EQ(registration.is_resident, true);
  EXPECT_EQ(virtual_device_factory_->mutable_state()->pin, kTestPIN);
}

TEST_F(ResidentKeyAuthenticatorImplTest, StorageFull) {
  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  config.resident_credential_storage = 1;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

  // Add a resident key to fill the authenticator.
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 1, 1, 1}}, "test@example.com", "Test User"));

  test_client_.delegate_config.expected_failure_reason =
      AuthenticatorRequestClientDelegate::InterestingFailureReason::
          kStorageFull;
  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kStorageFull,
                                 AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(ResidentKeyAuthenticatorImplTest,
       MakeCredentialEmptyFields_SecurityKey) {
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.resident_key_support = true;
  config.reject_empty_display_name = true;
  virtual_device_factory_->SetCtap2Config(std::move(config));
  virtual_device_factory_->SetTransport(
      device::FidoTransportProtocol::kUsbHumanInterfaceDevice);

  PublicKeyCredentialCreationOptionsPtr options = make_credential_options();

  // This value is perfectly legal, but our VirtualCtap2Device simulates
  // some security keys in rejecting empty values. CBOR serialisation should
  // omit these values rather than send empty ones.
  options->user.display_name = "";

  EXPECT_EQ(AuthenticatorStatus::SUCCESS,
            AuthenticatorMakeCredential(std::move(options)).status);
}

// Regression test for crbug.com/346835891.
TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialEmptyFields_Phone) {
  // iPhones reject a request with a missing display name.
  VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.resident_key_support = true;
  config.reject_missing_display_name = true;
  virtual_device_factory_->SetCtap2Config(std::move(config));
  virtual_device_factory_->SetTransport(device::FidoTransportProtocol::kHybrid);

  PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
  options->user.display_name = "";

  EXPECT_EQ(AuthenticatorStatus::SUCCESS,
            AuthenticatorMakeCredential(std::move(options)).status);
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionSingleNoPII) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, std::nullopt, std::nullopt));

  // |SelectAccount| should not be called when there's only a single response
  // with no identifying user info because the UI is bad in that case: we can
  // only display the single choice of "Unknown user".
  test_client_.delegate_config.expected_accounts = "<invalid>";
  GetAssertionResult result =
      AuthenticatorGetAssertion(get_credential_options());

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionUserSelected) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, "Test", "User"));

  for (const bool internal_account_chooser : {false, true}) {
    SCOPED_TRACE(internal_account_chooser);

    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.resident_key_support = true;
    config.internal_account_chooser = internal_account_chooser;
    virtual_device_factory_->SetCtap2Config(config);

    // |SelectAccount| should not be called when userSelected is set.
    if (internal_account_chooser) {
      test_client_.delegate_config.expected_accounts = "<invalid>";
    } else {
      test_client_.delegate_config.expected_accounts = "01020304:Test:User";
      test_client_.delegate_config.selected_user_id = {1, 2, 3, 4};
    }
    GetAssertionResult result =
        AuthenticatorGetAssertion(get_credential_options());

    EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(HasUV(result.response));
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionSingleWithPII) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, std::nullopt, "Test User"));

  // |SelectAccount| should be called when PII is available.
  test_client_.delegate_config.expected_accounts = "01020304::Test User";
  test_client_.delegate_config.selected_user_id = {1, 2, 3, 4};
  GetAssertionResult result =
      AuthenticatorGetAssertion(get_credential_options());
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionMulti) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, "test@example.com", "Test User"));
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 2}}, kTestRelyingPartyId,
      /*user_id=*/{{5, 6, 7, 8}}, "test2@example.com", "Test User 2"));

  test_client_.delegate_config.expected_accounts =
      "01020304:test@example.com:Test User/"
      "05060708:test2@example.com:Test User 2";
  test_client_.delegate_config.selected_user_id = {1, 2, 3, 4};

  GetAssertionResult result =
      AuthenticatorGetAssertion(get_credential_options());

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionUVDiscouraged) {
  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  config.u2f_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, std::nullopt, std::nullopt));

  // |SelectAccount| should not be called when there's only a single response
  // without identifying information.
  test_client_.delegate_config.expected_accounts = "<invalid>";
  PublicKeyCredentialRequestOptionsPtr options(get_credential_options());
  options->user_verification =
      device::UserVerificationRequirement::kDiscouraged;

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  // The UV=discouraged should have been ignored for a resident-credential
  // request.
  EXPECT_TRUE(HasUV(result.response));
}

static const char* BlobSupportDescription(device::LargeBlobSupport support) {
  switch (support) {
    case device::LargeBlobSupport::kNotRequested:
      return "Blob not requested";
    case device::LargeBlobSupport::kPreferred:
      return "Blob preferred";
    case device::LargeBlobSupport::kRequired:
      return "Blob required";
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialLargeBlob) {
  constexpr auto BlobRequired = device::LargeBlobSupport::kRequired;
  constexpr auto BlobPreferred = device::LargeBlobSupport::kPreferred;
  constexpr auto BlobNotRequested = device::LargeBlobSupport::kNotRequested;
  constexpr auto nullopt = std::nullopt;

  constexpr struct {
    bool large_blob_extension;
    std::optional<bool> large_blob_support;
    bool rk_required;
    device::LargeBlobSupport large_blob_enable;
    bool request_success;
    bool did_create_large_blob;
  } kLargeBlobTestCases[] = {
      // clang-format off
      // ext,  support,  rk,    enabled,          success, did create
      { false, true,     true,  BlobRequired,     true,    true},
      { false, true,     true,  BlobPreferred,    true,    true},
      { false, true,     true,  BlobNotRequested, true,    false},
      { false, true,     false, BlobRequired,     false,   false},
      { false, true,     false, BlobPreferred,    true,    false},
      { false, true,     true,  BlobNotRequested, true,    false},
      { false, false,    true,  BlobRequired,     false,   false},
      { false, false,    true,  BlobPreferred,    true,    false},
      { false, true,     true,  BlobNotRequested, true,    false},

      { true,  true,     true,  BlobRequired,     true,    true},
      { true,  true,     true,  BlobPreferred,    true,    true},
      { true,  true,     true,  BlobNotRequested, true,    false},
      { true,  true,     false, BlobRequired,     false,   false},
      { true,  true,     false, BlobPreferred,    true,    false},
      { true,  true,     true,  BlobNotRequested, true,    false},
      { true,  nullopt,  true,  BlobRequired,     false,   false},
      { true,  nullopt,  true,  BlobPreferred,    true,    false},
      { true,  true,     true,  BlobNotRequested, true,    false},
      { true,  false,    true,  BlobPreferred,    true,    false},
      { true,  false,    true,  BlobRequired,     false,   false},
      // clang-format on
  };
  for (auto& test : kLargeBlobTestCases) {
    if (test.large_blob_support) {
      SCOPED_TRACE(::testing::Message()
                   << "support=" << *test.large_blob_support);
    } else {
      SCOPED_TRACE(::testing::Message() << "support={}");
    }
    SCOPED_TRACE(::testing::Message() << "rk_required=" << test.rk_required);
    SCOPED_TRACE(::testing::Message()
                 << "enabled="
                 << BlobSupportDescription(test.large_blob_enable));
    SCOPED_TRACE(::testing::Message() << "success=" << test.request_success);
    SCOPED_TRACE(::testing::Message()
                 << "did create=" << test.did_create_large_blob);
    SCOPED_TRACE(::testing::Message()
                 << "large_blob_extension=" << test.large_blob_extension);

    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.pin_uv_auth_token_support = true;
    config.resident_key_support = true;
    config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                             std::end(device::kCtap2Versions2_1)};
    if (test.large_blob_extension) {
      config.large_blob_extension_support = test.large_blob_support;
    } else {
      config.large_blob_support = *test.large_blob_support;
    }
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options(
        test.rk_required ? device::ResidentKeyRequirement::kRequired
                         : device::ResidentKeyRequirement::kDiscouraged);
    options->large_blob_enable = test.large_blob_enable;
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));

    if (test.request_success) {
      ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
      ASSERT_EQ(1u,
                virtual_device_factory_->mutable_state()->registrations.size());
      const device::VirtualFidoDevice::RegistrationData& registration =
          virtual_device_factory_->mutable_state()
              ->registrations.begin()
              ->second;
      EXPECT_EQ(test.did_create_large_blob && !test.large_blob_extension,
                registration.large_blob_key.has_value());
      EXPECT_EQ(test.large_blob_enable != BlobNotRequested,
                result.response->echo_large_blob);
      EXPECT_EQ(test.did_create_large_blob,
                result.response->supports_large_blob);
    } else {
      ASSERT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
      ASSERT_EQ(0u,
                virtual_device_factory_->mutable_state()->registrations.size());
    }
    virtual_device_factory_->mutable_state()->registrations.clear();
    virtual_device_factory_->mutable_state()->ClearLargeBlobs();
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionLargeBlobRead) {
  constexpr struct {
    bool large_blob_support;
    bool large_blob_set;
    bool large_blob_key_set;
    bool did_read_large_blob;
  } kLargeBlobTestCases[] = {
      // clang-format off
      // support,  set,   key_set, did_read
       { true,     true,  true,    true  },
       { true,     false, false,   false },
       { true,     false, true,    false },
       { false,    false, false,   false },
      // clang-format on
  };
  for (auto& test : kLargeBlobTestCases) {
    SCOPED_TRACE(::testing::Message() << "support=" << test.large_blob_support);
    SCOPED_TRACE(::testing::Message() << "set=" << test.large_blob_set);
    SCOPED_TRACE(::testing::Message() << "key_set=" << test.large_blob_key_set);
    SCOPED_TRACE(::testing::Message()
                 << "did_read=" << test.did_read_large_blob);

    const std::vector<uint8_t> large_blob = {'b', 'l', 'o', 'b'};
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.pin_uv_auth_token_support = true;
    config.resident_key_support = true;
    config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                             std::end(device::kCtap2Versions2_1)};
    config.large_blob_support = test.large_blob_support;
    virtual_device_factory_->SetCtap2Config(config);
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
        /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
        /*user_id=*/{{1, 2, 3, 4}}, std::nullopt, std::nullopt));

    if (test.large_blob_set) {
      virtual_device_factory_->mutable_state()->InjectLargeBlob(
          &virtual_device_factory_->mutable_state()
               ->registrations.begin()
               ->second,
          CompressLargeBlob(large_blob));
    } else if (test.large_blob_key_set) {
      virtual_device_factory_->mutable_state()
          ->registrations.begin()
          ->second.large_blob_key = {{0}};
    }

    PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
    options->extensions->large_blob_read = true;
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(result.response->extensions->echo_large_blob);
    EXPECT_FALSE(result.response->extensions->echo_large_blob_written);
    if (test.did_read_large_blob) {
      EXPECT_EQ(large_blob, *result.response->extensions->large_blob);
    } else {
      EXPECT_FALSE(result.response->extensions->large_blob.has_value());
    }
    virtual_device_factory_->mutable_state()->registrations.clear();
    virtual_device_factory_->mutable_state()->ClearLargeBlobs();
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionLargeBlobWrite) {
  constexpr struct {
    bool large_blob_support;
    bool large_blob_set;
    bool large_blob_key_set;
    bool did_write_large_blob;
  } kLargeBlobTestCases[] = {
      // clang-format off
      // support,  set,   key_set, did_write
       { true,     true,  true,    true  },
       { true,     false, false,   false },
       { true,     false, true,    true  },
       { false,    false, false,   false },
      // clang-format on
  };
  for (auto& test : kLargeBlobTestCases) {
    SCOPED_TRACE(::testing::Message() << "support=" << test.large_blob_support);
    SCOPED_TRACE(::testing::Message() << "set=" << test.large_blob_set);
    SCOPED_TRACE(::testing::Message() << "key_set=" << test.large_blob_key_set);
    SCOPED_TRACE(::testing::Message()
                 << "did_write=" << test.did_write_large_blob);

    const std::vector<uint8_t> large_blob = {'b', 'l', 'o', 'b'};
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.pin_uv_auth_token_support = true;
    config.resident_key_support = true;
    config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                             std::end(device::kCtap2Versions2_1)};
    config.large_blob_support = test.large_blob_support;
    virtual_device_factory_->SetCtap2Config(config);
    const std::vector<uint8_t> cred_id = {4, 3, 2, 1};
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
        cred_id, kTestRelyingPartyId,
        /*user_id=*/{{1, 2, 3, 4}}, std::nullopt, std::nullopt));

    if (test.large_blob_set) {
      virtual_device_factory_->mutable_state()->InjectLargeBlob(
          &virtual_device_factory_->mutable_state()
               ->registrations.begin()
               ->second,
          CompressLargeBlob(large_blob));
    } else if (test.large_blob_key_set) {
      virtual_device_factory_->mutable_state()
          ->registrations.begin()
          ->second.large_blob_key = {{0}};
    }

    PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
    options->allow_credentials = {device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, cred_id)};
    options->extensions->large_blob_write = large_blob;
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(result.response->extensions->echo_large_blob);
    EXPECT_FALSE(result.response->extensions->large_blob.has_value());
    EXPECT_TRUE(result.response->extensions->echo_large_blob_written);
    EXPECT_EQ(test.did_write_large_blob,
              result.response->extensions->large_blob_written);
    if (test.did_write_large_blob) {
      std::optional<device::LargeBlob> compressed_blob =
          virtual_device_factory_->mutable_state()->GetLargeBlob(
              virtual_device_factory_->mutable_state()
                  ->registrations.begin()
                  ->second);
      EXPECT_EQ(large_blob, UncompressLargeBlob(*compressed_blob));
    }
    virtual_device_factory_->mutable_state()->registrations.clear();
    virtual_device_factory_->mutable_state()->ClearLargeBlobs();
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest,
       GetAssertionLargeBlobExtensionNoSupport) {
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.pin_uv_auth_token_support = true;
  config.resident_key_support = true;
  config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                           std::end(device::kCtap2Versions2_1)};
  virtual_device_factory_->SetCtap2Config(config);

  const std::vector<uint8_t> cred_id = {4, 3, 2, 1};
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      cred_id, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, std::nullopt, std::nullopt));

  // Try to read a large blob that doesn't exist and couldn't exist because the
  // authenticator doesn't support large blobs.
  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  options->allow_credentials = {device::PublicKeyCredentialDescriptor(
      device::CredentialType::kPublicKey, cred_id)};
  options->extensions->large_blob_read = true;
  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
  ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(result.response->extensions->echo_large_blob);
  EXPECT_FALSE(result.response->extensions->echo_large_blob_written);
  ASSERT_FALSE(result.response->extensions->large_blob);
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionLargeBlobExtension) {
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.pin_uv_auth_token_support = true;
  config.resident_key_support = true;
  config.large_blob_extension_support = true;
  config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                           std::end(device::kCtap2Versions2_1)};
  virtual_device_factory_->SetCtap2Config(config);

  const std::vector<uint8_t> large_blob = {'b', 'l', 'o', 'b'};
  const std::vector<uint8_t> cred_id = {4, 3, 2, 1};
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      cred_id, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, std::nullopt, std::nullopt));

  {
    // Try to read a large blob that doesn't exist.
    PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
    options->allow_credentials = {device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, cred_id)};
    options->extensions->large_blob_read = true;
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(result.response->extensions->echo_large_blob);
    EXPECT_FALSE(result.response->extensions->echo_large_blob_written);
    ASSERT_FALSE(result.response->extensions->large_blob);
  }

  {
    // Write a large blob.
    PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
    options->allow_credentials = {device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, cred_id)};
    options->extensions->large_blob_write = large_blob;
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(result.response->extensions->echo_large_blob);
    EXPECT_TRUE(result.response->extensions->echo_large_blob_written);
    EXPECT_FALSE(result.response->extensions->large_blob);
  }

  {
    // Read it back.
    PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
    options->allow_credentials = {device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, cred_id)};
    options->extensions->large_blob_read = true;
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(result.response->extensions->echo_large_blob);
    EXPECT_FALSE(result.response->extensions->echo_large_blob_written);
    ASSERT_TRUE(result.response->extensions->large_blob);
    EXPECT_EQ(large_blob, *result.response->extensions->large_blob);
  }

  // Corrupt the large blob data and attempt to read it back. The invalid
  // large blob should be ignored.
  virtual_device_factory_->mutable_state()
      ->registrations.begin()
      ->second.large_blob->compressed_data = {1, 2, 3, 4};

  {
    PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
    options->allow_credentials = {device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, cred_id)};
    options->extensions->large_blob_read = true;
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(result.response->extensions->echo_large_blob);
    EXPECT_FALSE(result.response->extensions->echo_large_blob_written);
    ASSERT_FALSE(result.response->extensions->large_blob);
  }
}

static const char* ProtectionPolicyDescription(
    blink::mojom::ProtectionPolicy p) {
  switch (p) {
    case blink::mojom::ProtectionPolicy::UNSPECIFIED:
      return "UNSPECIFIED";
    case blink::mojom::ProtectionPolicy::NONE:
      return "NONE";
    case blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED:
      return "UV_OR_CRED_ID_REQUIRED";
    case blink::mojom::ProtectionPolicy::UV_REQUIRED:
      return "UV_REQUIRED";
  }
}

static const char* CredProtectDescription(device::CredProtect cred_protect) {
  switch (cred_protect) {
    case device::CredProtect::kUVOptional:
      return "UV optional";
    case device::CredProtect::kUVOrCredIDRequired:
      return "UV or cred ID required";
    case device::CredProtect::kUVRequired:
      return "UV required";
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, CredProtectRegistration) {
  const auto UNSPECIFIED = blink::mojom::ProtectionPolicy::UNSPECIFIED;
  const auto NONE = blink::mojom::ProtectionPolicy::NONE;
  const auto UV_OR_CRED =
      blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED;
  const auto UV_REQ = blink::mojom::ProtectionPolicy::UV_REQUIRED;
  const int kOk = 0;
  const int kNonsense = 1;
  const int kNotAllow = 2;
  const device::UserVerificationRequirement kUV =
      device::UserVerificationRequirement::kRequired;
  const device::UserVerificationRequirement kUP =
      device::UserVerificationRequirement::kDiscouraged;
  const device::UserVerificationRequirement kUVPref =
      device::UserVerificationRequirement::kPreferred;

  const struct {
    bool supported_by_authenticator;
    bool is_resident;
    blink::mojom::ProtectionPolicy protection;
    bool enforce;
    device::UserVerificationRequirement uv;
    int expected_outcome;
    blink::mojom::ProtectionPolicy resulting_policy;
  } kExpectations[] = {
      // clang-format off
    // Support | Resdnt | Level      | Enf  |  UV  || Result   | Prot level
    {  false,   false,   UNSPECIFIED, false, kUP,     kOk,       NONE},
    {  false,   false,   UNSPECIFIED, true,  kUP,     kNonsense, UNSPECIFIED},
    {  false,   false,   UNSPECIFIED, false, kUVPref, kOk,       NONE},
    {  false,   false,   NONE,        false, kUP,     kNonsense, UNSPECIFIED},
    {  false,   false,   NONE,        true,  kUP,     kNonsense, UNSPECIFIED},
    {  false,   false,   UV_OR_CRED,  false, kUP,     kOk,       NONE},
    {  false,   false,   UV_OR_CRED,  true,  kUP,     kNotAllow, UNSPECIFIED},
    {  false,   false,   UV_OR_CRED,  false, kUV,     kOk,       NONE},
    {  false,   false,   UV_OR_CRED,  true,  kUV,     kNotAllow, UNSPECIFIED},
    {  false,   false,   UV_REQ,      false, kUP,     kNonsense, UNSPECIFIED},
    {  false,   false,   UV_REQ,      false, kUV,     kOk,       NONE},
    {  false,   false,   UV_REQ,      true,  kUP,     kNonsense, UNSPECIFIED},
    {  false,   false,   UV_REQ,      true,  kUV,     kNotAllow, UNSPECIFIED},
    {  false,   true,    UNSPECIFIED, false, kUP,     kOk,       NONE},
    {  false,   true,    UNSPECIFIED, true,  kUP,     kNonsense, UNSPECIFIED},
    {  false,   true,    NONE,        false, kUP,     kOk,       NONE},
    {  false,   true,    NONE,        true,  kUP,     kNonsense, UNSPECIFIED},
    {  false,   true,    UV_OR_CRED,  false, kUP,     kOk,       NONE},
    {  false,   true,    UV_OR_CRED,  true,  kUP,     kNotAllow, UNSPECIFIED},
    {  false,   true,    UV_REQ,      false, kUP,     kNonsense, UNSPECIFIED},
    {  false,   true,    UV_REQ,      false, kUV,     kOk,       NONE},
    {  false,   true,    UV_REQ,      true,  kUP,     kNonsense, UNSPECIFIED},
    {  false,   true,    UV_REQ,      true,  kUV,     kNotAllow, UNSPECIFIED},

    // For the case where the authenticator supports credProtect we do not
    // repeat the cases above that are |kNonsense| on the assumption that
    // authenticator support is irrelevant. Therefore these are just the non-
    // kNonsense cases from the prior block.
    {  true,    false,   UNSPECIFIED, false, kUP,     kOk,       NONE},
    {  true,    false,   UV_OR_CRED,  false, kUP,     kOk,       UV_OR_CRED},
    {  true,    false,   UV_OR_CRED,  true,  kUP,     kOk,       UV_OR_CRED},
    {  true,    false,   UV_OR_CRED,  false, kUV,     kOk,       UV_OR_CRED},
    {  true,    false,   UV_OR_CRED,  true,  kUV,     kOk,       UV_OR_CRED},
    {  true,    false,   UV_REQ,      false, kUV,     kOk,       UV_REQ},
    {  true,    false,   UV_REQ,      true,  kUV,     kOk,       UV_REQ},
    {  true,    true,    UNSPECIFIED, false, kUP,     kOk,       UV_OR_CRED},
    {  true,    true,    UNSPECIFIED, false, kUVPref, kOk,       UV_REQ},
    {  true,    true,    NONE,        false, kUP,     kOk,       NONE},
    {  true,    true,    NONE,        false, kUVPref, kOk,       NONE},
    {  true,    true,    UV_OR_CRED,  false, kUP,     kOk,       UV_OR_CRED},
    {  true,    true,    UV_OR_CRED,  true,  kUP,     kOk,       UV_OR_CRED},
    {  true,    true,    UV_OR_CRED,  false, kUVPref, kOk,       UV_OR_CRED},
    {  true,    true,    UV_REQ,      false, kUV,     kOk,       UV_REQ},
    {  true,    true,    UV_REQ,      true,  kUV,     kOk,       UV_REQ},
      // clang-format on
  };

  for (const auto& test : kExpectations) {
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.resident_key_support = true;
    config.cred_protect_support = test.supported_by_authenticator;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->registrations.clear();

    SCOPED_TRACE(::testing::Message() << "uv=" << device::ToString(test.uv));
    SCOPED_TRACE(::testing::Message() << "enforce=" << test.enforce);
    SCOPED_TRACE(::testing::Message()
                 << "level=" << ProtectionPolicyDescription(test.protection));
    SCOPED_TRACE(::testing::Message() << "resident=" << test.is_resident);
    SCOPED_TRACE(::testing::Message()
                 << "support=" << test.supported_by_authenticator);

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
    options->authenticator_selection->resident_key =
        test.is_resident ? device::ResidentKeyRequirement::kRequired
                         : device::ResidentKeyRequirement::kDiscouraged;
    options->protection_policy = test.protection;
    options->enforce_protection_policy = test.enforce;
    options->authenticator_selection->user_verification_requirement = test.uv;

    AuthenticatorStatus status =
        AuthenticatorMakeCredential(std::move(options)).status;

    switch (test.expected_outcome) {
      case kOk: {
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, status);
        ASSERT_EQ(
            1u, virtual_device_factory_->mutable_state()->registrations.size());
        const device::CredProtect result =
            virtual_device_factory_->mutable_state()
                ->registrations.begin()
                ->second.protection;

        switch (test.resulting_policy) {
          case UNSPECIFIED:
            NOTREACHED();
          case NONE:
            EXPECT_EQ(device::CredProtect::kUVOptional, result);
            break;
          case UV_OR_CRED:
            EXPECT_EQ(device::CredProtect::kUVOrCredIDRequired, result);
            break;
          case UV_REQ:
            EXPECT_EQ(device::CredProtect::kUVRequired, result);
            break;
        }
        break;
      }
      case kNonsense:
        EXPECT_EQ(AuthenticatorStatus::PROTECTION_POLICY_INCONSISTENT, status);
        break;
      case kNotAllow:
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, status);
        break;
      default:
        NOTREACHED();
    }
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, AuthenticatorSetsCredProtect) {
  // Some authenticators are expected to set the credProtect extension ad
  // libitum. Therefore we should only require that the returned extension is at
  // least as restrictive as requested, but perhaps not exactly equal.
  constexpr std::array<blink::mojom::ProtectionPolicy, 3> kMojoLevels = {
      blink::mojom::ProtectionPolicy::NONE,
      blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED,
      blink::mojom::ProtectionPolicy::UV_REQUIRED,
  };
  constexpr std::array<device::CredProtect, 3> kDeviceLevels = {
      device::CredProtect::kUVOptional,
      device::CredProtect::kUVOrCredIDRequired,
      device::CredProtect::kUVRequired,
  };

  for (int requested_level = 0; requested_level < 3; requested_level++) {
    for (int forced_level = 1; forced_level < 3; forced_level++) {
      SCOPED_TRACE(::testing::Message() << "requested=" << requested_level);
      SCOPED_TRACE(::testing::Message() << "forced=" << forced_level);
      device::VirtualCtap2Device::Config config;
      config.pin_support = true;
      config.resident_key_support = true;
      config.cred_protect_support = true;
      config.force_cred_protect = kDeviceLevels[forced_level];
      virtual_device_factory_->SetCtap2Config(config);
      virtual_device_factory_->mutable_state()->registrations.clear();

      PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
      options->authenticator_selection->resident_key =
          device::ResidentKeyRequirement::kRequired;
      options->protection_policy = kMojoLevels[requested_level];
      options->authenticator_selection->user_verification_requirement =
          device::UserVerificationRequirement::kRequired;

      AuthenticatorStatus status =
          AuthenticatorMakeCredential(std::move(options)).status;

      if (requested_level <= forced_level) {
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, status);
        ASSERT_EQ(
            1u, virtual_device_factory_->mutable_state()->registrations.size());
        const std::optional<device::CredProtect> result =
            virtual_device_factory_->mutable_state()
                ->registrations.begin()
                ->second.protection;
        EXPECT_EQ(*result, config.force_cred_protect);
      } else {
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, status);
      }
    }
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, AuthenticatorDefaultCredProtect) {
  // Some authenticators may have a default credProtect level that isn't
  // kUVOptional. This has complex interactions that are tested here.
  constexpr struct {
    blink::mojom::ProtectionPolicy requested_level;
    device::CredProtect authenticator_default;
    device::CredProtect result;
  } kExpectations[] = {
      // Standard case: normal authenticator and nothing specified. Chrome sets
      // a default of kUVOrCredIDRequired for discoverable credentials.
      {
          blink::mojom::ProtectionPolicy::UNSPECIFIED,
          device::CredProtect::kUVOptional,
          device::CredProtect::kUVOrCredIDRequired,
      },
      // Chrome's default of |kUVOrCredIDRequired| should not prevent a site
      // from requesting |kUVRequired| from a normal authenticator.
      {
          blink::mojom::ProtectionPolicy::UV_REQUIRED,
          device::CredProtect::kUVOptional,
          device::CredProtect::kUVRequired,
      },
      // Authenticator has a non-standard default, which should work fine.
      {
          blink::mojom::ProtectionPolicy::UNSPECIFIED,
          device::CredProtect::kUVOrCredIDRequired,
          device::CredProtect::kUVOrCredIDRequired,
      },
      // Authenticators can have a default of kUVRequired, but Chrome has a
      // default of kUVOrCredIDRequired for discoverable credentials. We should
      // not get a lesser protection level because of that.
      {
          blink::mojom::ProtectionPolicy::UNSPECIFIED,
          device::CredProtect::kUVRequired,
          device::CredProtect::kUVRequired,
      },
      // Site should be able to explicitly set credProtect kUVOptional despite
      // an authenticator default.
      {
          blink::mojom::ProtectionPolicy::NONE,
          device::CredProtect::kUVOrCredIDRequired,
          device::CredProtect::kUVOptional,
      },
  };

  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.resident_key_support = true;
  config.cred_protect_support = true;

  for (const auto& test : kExpectations) {
    config.default_cred_protect = test.authenticator_default;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->registrations.clear();

    SCOPED_TRACE(::testing::Message()
                 << "result=" << CredProtectDescription(test.result));
    SCOPED_TRACE(::testing::Message()
                 << "default="
                 << CredProtectDescription(test.authenticator_default));
    SCOPED_TRACE(::testing::Message()
                 << "request="
                 << ProtectionPolicyDescription(test.requested_level));

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
    options->authenticator_selection->resident_key =
        device::ResidentKeyRequirement::kRequired;
    options->protection_policy = test.requested_level;
    options->authenticator_selection->user_verification_requirement =
        device::UserVerificationRequirement::kRequired;

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
    ASSERT_EQ(1u,
              virtual_device_factory_->mutable_state()->registrations.size());
    const device::CredProtect result = virtual_device_factory_->mutable_state()
                                           ->registrations.begin()
                                           ->second.protection;

    EXPECT_EQ(result, test.result) << CredProtectDescription(result);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, ProtectedNonResidentCreds) {
  // Until we have UVToken, there's a danger that we'll preflight UV-required
  // credential IDs such that the authenticator denies knowledge of all of them
  // for silent requests and then we fail the whole request.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.resident_key_support = true;
  config.cred_protect_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId));
  ASSERT_EQ(1u, virtual_device_factory_->mutable_state()->registrations.size());
  virtual_device_factory_->mutable_state()
      ->registrations.begin()
      ->second.protection = device::CredProtect::kUVRequired;

  // |SelectAccount| should not be called when there's only a single response.
  test_client_.delegate_config.expected_accounts = "<invalid>";

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  options->allow_credentials = GetTestCredentials(5);
  options->allow_credentials[0].id = {4, 3, 2, 1};

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(ResidentKeyAuthenticatorImplTest, WithAppIDExtension) {
  // Setting an AppID value for a resident-key request should be ignored.
  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.pin_support = true;
  config.resident_key_support = true;
  config.cred_protect_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, std::nullopt, std::nullopt));

  // |SelectAccount| should not be called when there's only a single response
  // without identifying information.
  test_client_.delegate_config.expected_accounts = "<invalid>";

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  options->extensions->appid = kTestOrigin1;

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

#if BUILDFLAG(IS_WIN)
// Requests with a credProtect extension that have |enforce_protection_policy|
// set should be rejected if the Windows WebAuthn API doesn't support
// credProtect.
TEST_F(ResidentKeyAuthenticatorImplTest, WinCredProtectApiVersion) {
  // The canned response returned by the Windows API fake is for acme.com.
  virtual_device_factory_->set_discover_win_webauthn_api_authenticator(true);
  fake_win_webauthn_api_.set_available(true);
  NavigateAndCommit(GURL("https://acme.com"));
  for (const bool supports_cred_protect : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "supports_cred_protect: " << supports_cred_protect);

    fake_win_webauthn_api_.set_version(supports_cred_protect
                                           ? WEBAUTHN_API_VERSION_2
                                           : WEBAUTHN_API_VERSION_1);

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
    options->relying_party = device::PublicKeyCredentialRpEntity();
    options->relying_party.id = device::test_data::kRelyingPartyId;
    options->relying_party.name = "";
    options->authenticator_selection->user_verification_requirement =
        device::UserVerificationRequirement::kRequired;
    options->authenticator_selection->resident_key =
        device::ResidentKeyRequirement::kRequired;
    options->protection_policy =
        blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED;
    options->enforce_protection_policy = true;

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              supports_cred_protect ? AuthenticatorStatus::SUCCESS
                                    : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

// Regression test for crbug.com/512385679.
// Tests that Chrome supports the hmac secret extension on create on Windows 10.
TEST_F(ResidentKeyAuthenticatorImplTest, WinCreateHmacSecret) {
  virtual_device_factory_->set_discover_win_webauthn_api_authenticator(true);
  fake_win_webauthn_api_.set_available(true);
  fake_win_webauthn_api_.set_version(2);
  NavigateAndCommit(GURL("https://acme.com"));
  PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
  options->relying_party = device::PublicKeyCredentialRpEntity();
  options->relying_party.id = device::test_data::kRelyingPartyId;
  options->relying_party.name = "";
  options->authenticator_selection->user_verification_requirement =
      device::UserVerificationRequirement::kRequired;
  options->authenticator_selection->resident_key =
      device::ResidentKeyRequirement::kRequired;
  options->hmac_create_secret = true;
  MakeCredentialResult result = AuthenticatorMakeCredential(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(result.response->echo_hmac_create_secret);
  EXPECT_TRUE(result.response->hmac_create_secret);
}

// Tests that the incognito flag is plumbed through conditional UI requests.
TEST_F(ResidentKeyAuthenticatorImplTest, ConditionalUI_Incognito) {
  virtual_device_factory_->set_discover_win_webauthn_api_authenticator(true);
  fake_win_webauthn_api_.set_available(true);
  fake_win_webauthn_api_.set_version(WEBAUTHN_API_VERSION_4);
  fake_win_webauthn_api_.set_supports_silent_discovery(true);
  device::PublicKeyCredentialRpEntity rp(kTestRelyingPartyId);
  device::PublicKeyCredentialUserEntity user({1, 2, 3, 4});
  fake_win_webauthn_api_.InjectDiscoverableCredential(
      /*credential_id=*/{{4, 3, 2, 1}}, std::move(rp), std::move(user),
      /*provider_name=*/std::nullopt);

  // |SelectAccount| should not be called for conditional UI requests.
  test_client_.delegate_config.expected_accounts = "<invalid>";
  test_client_.delegate_config.expect_conditional = true;

  for (bool is_off_the_record : {true, false}) {
    SCOPED_TRACE(is_off_the_record ? "off the record" : "on the record");
    static_cast<TestBrowserContext*>(GetBrowserContext())
        ->set_is_off_the_record(is_off_the_record);
    auto options = GetCredentialOptions::New();
    PublicKeyCredentialRequestOptionsPtr public_key(get_credential_options());
    options->mediation = blink::mojom::Mediation::CONDITIONAL;
    options->public_key = std::move(public_key);
    GetAssertionResult result = AuthenticatorGetCredential(std::move(options));
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    ASSERT_TRUE(fake_win_webauthn_api_.last_get_credentials_options());
    EXPECT_EQ(fake_win_webauthn_api_.last_get_credentials_options()
                  ->bBrowserInPrivateMode,
              is_off_the_record);
  }
}

// Tests that attempting to make a credential with large blob = required and
// attachment = platform on Windows fails and the request is not sent to the
// WebAuthn API.
// This is because largeBlob = required is ignored by the Windows platform
// authenticator at the time of writing (Feb 2023).
TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialLargeBlobWinPlatform) {
  virtual_device_factory_->set_discover_win_webauthn_api_authenticator(true);
  fake_win_webauthn_api_.set_available(true);
  fake_win_webauthn_api_.set_version(WEBAUTHN_API_VERSION_3);
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->large_blob_enable = device::LargeBlobSupport::kRequired;
  options->authenticator_selection->resident_key =
      device::ResidentKeyRequirement::kRequired;
  options->authenticator_selection->authenticator_attachment =
      device::AuthenticatorAttachment::kPlatform;
  MakeCredentialResult result = AuthenticatorMakeCredential(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_FALSE(fake_win_webauthn_api_.last_make_credential_options());
}

// Tests that attempting to make a credential with large blob = preferred does
// not fail the request on Windows.
// Regression test for crbug.com/325934997.
TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialLargeBlobWinPreferred) {
  virtual_device_factory_->set_discover_win_webauthn_api_authenticator(true);
  fake_win_webauthn_api_.set_available(true);
  fake_win_webauthn_api_.set_version(WEBAUTHN_API_VERSION_3);
  for (bool large_blob_supported : {false, true}) {
    fake_win_webauthn_api_.set_large_blob_supported(large_blob_supported);
    SCOPED_TRACE(large_blob_supported);
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->large_blob_enable = device::LargeBlobSupport::kPreferred;
    options->authenticator_selection->resident_key =
        device::ResidentKeyRequirement::kRequired;
    options->authenticator_selection->authenticator_attachment =
        device::AuthenticatorAttachment::kCrossPlatform;
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_TRUE(result.response->echo_large_blob);
    EXPECT_EQ(result.response->supports_large_blob, large_blob_supported);
  }
}

// Tests that the AAGUID is not zeroed out for Windows Hello on Windows versions
// where Chrome can learn the transport used.
// Regression test for crbug.com/446157740.
TEST_F(ResidentKeyAuthenticatorImplTest,
       WinPlatformAuthenticatorAttestationAAGUID) {
  enum Test {
    kWindowsReportsUsb,
    kWindowsReportsInternal,
    kWindowsDoesNotReportTransport
  };
  virtual_device_factory_->set_discover_win_webauthn_api_authenticator(true);
  fake_win_webauthn_api_.set_available(true);
  for (Test test : {kWindowsReportsUsb, kWindowsReportsInternal,
                    kWindowsDoesNotReportTransport}) {
    SCOPED_TRACE(test);
    fake_win_webauthn_api_.set_version(test == kWindowsDoesNotReportTransport
                                           ? WEBAUTHN_API_VERSION_5
                                           : WEBAUTHN_API_VERSION_6);
    fake_win_webauthn_api_.set_transport(test == kWindowsReportsInternal
                                             ? WEBAUTHN_CTAP_TRANSPORT_INTERNAL
                                             : WEBAUTHN_CTAP_TRANSPORT_USB);
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);

    const device::AuthenticatorData auth_data =
        AuthDataFromMakeCredentialResponse(result.response);

    std::optional<cbor::Value> attestation_value =
        cbor::Reader::Read(result.response->attestation_object);
    ASSERT_TRUE(attestation_value);
    ASSERT_TRUE(attestation_value->is_map());
    if (test == kWindowsReportsInternal) {
      EXPECT_EQ(auth_data.attested_data()->aaguid(),
                device::FakeWinWebAuthnApi::kTestWindowsAaguid);
    } else {
      EXPECT_TRUE(auth_data.attested_data()->IsAaguidZero());
    }
  }
}
#endif  // BUILDFLAG(IS_WIN)

// Tests that chrome does not attempt setting the PRF extension during a
// PinUvAuthToken GetAssertion request if it is not supported by the
// authenticator.
// Regression test for crbug.com/1408786.
TEST_F(ResidentKeyAuthenticatorImplTest, PRFNotSupportedWithPinUvAuthToken) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.u2f_support = true;
  config.pin_support = true;
  config.pin_uv_auth_token_support = true;
  config.hmac_secret_support = false;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->SetCtap2Config(config);

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->user_verification = device::UserVerificationRequirement::kRequired;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      options->allow_credentials[0].id, options->relying_party_id,
      std::vector<uint8_t>{1, 2, 3, 4}, std::nullopt, std::nullopt));

  auto prf_value = blink::mojom::PRFValues::New();
  prf_value->first = std::vector<uint8_t>(32, 1);
  std::vector<blink::mojom::PRFValuesPtr> inputs;
  inputs.emplace_back(std::move(prf_value));
  options->extensions->prf = true;
  options->extensions->prf_inputs = std::move(inputs);
  options->allow_credentials.clear();
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(ResidentKeyAuthenticatorImplTest, PRFExtension) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool use_prf_extension_instead : {false, true}) {
    for (const auto pin_protocol :
         {device::PINUVAuthProtocol::kV1, device::PINUVAuthProtocol::kV2}) {
      SCOPED_TRACE(use_prf_extension_instead);
      SCOPED_TRACE(static_cast<unsigned>(pin_protocol));

      std::optional<device::PublicKeyCredentialDescriptor> credential;
      for (bool authenticator_support : {false, true}) {
        // Setting the PRF extension on an authenticator that doesn't support it
        // should cause the extension to be echoed, but with enabled=false.
        // Otherwise, enabled should be true.
        device::VirtualCtap2Device::Config config;
        if (authenticator_support) {
          config.prf_support = use_prf_extension_instead;
          config.hmac_secret_support = !use_prf_extension_instead;
        }
        config.internal_account_chooser = config.prf_support;
        config.always_uv = config.prf_support;
        config.max_credential_count_in_list = 3;
        config.max_credential_id_length = 256;
        config.pin_support = true;
        config.pin_protocol = pin_protocol;
        config.resident_key_support = true;
        virtual_device_factory_->SetCtap2Config(config);

        PublicKeyCredentialCreationOptionsPtr options =
            GetTestPublicKeyCredentialCreationOptions();
        options->prf_enable = true;
        options->authenticator_selection->resident_key =
            authenticator_support
                ? device::ResidentKeyRequirement::kRequired
                : device::ResidentKeyRequirement::kDiscouraged;
        options->user.id = {1, 2, 3, 4};
        options->user.name = "name";
        options->user.display_name = "displayName";
        MakeCredentialResult result =
            AuthenticatorMakeCredential(std::move(options));
        EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);

        ASSERT_TRUE(result.response->echo_prf);
        ASSERT_EQ(result.response->prf, authenticator_support);

        if (authenticator_support) {
          device::AuthenticatorData auth_data =
              AuthDataFromMakeCredentialResponse(result.response);
          credential.emplace(device::CredentialType::kPublicKey,
                             auth_data.GetCredentialId());
        }
      }

      auto assertion = [&](std::vector<blink::mojom::PRFValuesPtr> inputs,
                           unsigned allow_list_size = 1,
                           device::UserVerificationRequirement uv =
                               device::UserVerificationRequirement::kPreferred)
          -> blink::mojom::PRFValuesPtr {
        PublicKeyCredentialRequestOptionsPtr options =
            GetTestPublicKeyCredentialRequestOptions();
        options->extensions->prf = true;
        options->extensions->prf_inputs = std::move(inputs);
        options->allow_credentials.clear();
        options->user_verification = uv;
        if (allow_list_size >= 1) {
          for (unsigned i = 0; i < allow_list_size - 1; i++) {
            std::vector<uint8_t> random_credential_id(32,
                                                      static_cast<uint8_t>(i));
            options->allow_credentials.emplace_back(
                device::CredentialType::kPublicKey,
                std::move(random_credential_id));
          }
          options->allow_credentials.push_back(*credential);
        }

        GetAssertionResult result =
            AuthenticatorGetAssertion(std::move(options));

        EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
        CHECK(result.response->extensions->prf_results);
        CHECK(!result.response->extensions->prf_results->id);
        return std::move(result.response->extensions->prf_results);
      };

      const std::vector<uint8_t> salt1(32, 1);
      const std::vector<uint8_t> salt2(32, 2);
      std::vector<uint8_t> salt1_eval;
      std::vector<uint8_t> salt2_eval;

      {
        auto prf_value = blink::mojom::PRFValues::New();
        prf_value->first = salt1;
        std::vector<blink::mojom::PRFValuesPtr> inputs;
        inputs.emplace_back(std::move(prf_value));
        auto result = assertion(std::move(inputs));
        salt1_eval = std::move(result->first);
      }

      // The result should be consistent
      {
        auto prf_value = blink::mojom::PRFValues::New();
        prf_value->first = salt1;
        std::vector<blink::mojom::PRFValuesPtr> inputs;
        inputs.emplace_back(std::move(prf_value));
        auto result = assertion(std::move(inputs));
        ASSERT_EQ(result->first, salt1_eval);
      }

      // Security keys will use a different PRF if UV isn't done. But the PRF
      // extension should always get the UV PRF so uv=discouraged shouldn't
      // change the output.
      {
        auto prf_value = blink::mojom::PRFValues::New();
        prf_value->first = salt1;
        std::vector<blink::mojom::PRFValuesPtr> inputs;
        inputs.emplace_back(std::move(prf_value));
        auto result =
            assertion(std::move(inputs), 1,
                      device::UserVerificationRequirement::kDiscouraged);
        ASSERT_EQ(result->first, salt1_eval);
      }

      // Should be able to evaluate two points at once.
      {
        auto prf_value = blink::mojom::PRFValues::New();
        prf_value->first = salt1;
        prf_value->second = salt2;
        std::vector<blink::mojom::PRFValuesPtr> inputs;
        inputs.emplace_back(std::move(prf_value));
        auto result = assertion(std::move(inputs));
        ASSERT_EQ(result->first, salt1_eval);
        ASSERT_TRUE(result->second);
        salt2_eval = std::move(*result->second);
        ASSERT_NE(salt1_eval, salt2_eval);
      }

      // Should be consistent if swapped.
      {
        auto prf_value = blink::mojom::PRFValues::New();
        prf_value->first = salt2;
        prf_value->second = salt1;
        std::vector<blink::mojom::PRFValuesPtr> inputs;
        inputs.emplace_back(std::move(prf_value));
        auto result = assertion(std::move(inputs));
        ASSERT_EQ(result->first, salt2_eval);
        ASSERT_TRUE(result->second);
        ASSERT_EQ(*result->second, salt1_eval);
      }

      // Should still trigger if the credential ID is specified
      {
        auto prf_value = blink::mojom::PRFValues::New();
        prf_value->id.emplace(credential->id);
        prf_value->first = salt1;
        prf_value->second = salt2;
        std::vector<blink::mojom::PRFValuesPtr> inputs;
        inputs.emplace_back(std::move(prf_value));
        auto result = assertion(std::move(inputs));
        ASSERT_EQ(result->first, salt1_eval);
        ASSERT_TRUE(result->second);
        ASSERT_EQ(*result->second, salt2_eval);
      }

      // And the specified credential ID should override any default inputs.
      {
        auto prf_value1 = blink::mojom::PRFValues::New();
        prf_value1->first = std::vector<uint8_t>(32, 3);
        auto prf_value2 = blink::mojom::PRFValues::New();
        prf_value2->id.emplace(credential->id);
        prf_value2->first = salt1;
        prf_value2->second = salt2;
        std::vector<blink::mojom::PRFValuesPtr> inputs;
        inputs.emplace_back(std::move(prf_value1));
        inputs.emplace_back(std::move(prf_value2));
        auto result = assertion(std::move(inputs));
        ASSERT_EQ(result->first, salt1_eval);
        ASSERT_TRUE(result->second);
        ASSERT_EQ(*result->second, salt2_eval);
      }

      // ... and that should still be true if there there are lots of dummy
      // entries in the allowlist. Note that the virtual authenticator was
      // configured such that this will cause multiple batches.
      {
        auto prf_value = blink::mojom::PRFValues::New();
        prf_value->id.emplace(credential->id);
        prf_value->first = salt1;
        prf_value->second = salt2;
        std::vector<blink::mojom::PRFValuesPtr> inputs;
        inputs.emplace_back(std::move(prf_value));
        auto result = assertion(std::move(inputs), /*allowlist_size=*/20);
        ASSERT_EQ(result->first, salt1_eval);
        ASSERT_TRUE(result->second);
        ASSERT_EQ(*result->second, salt2_eval);
      }

      // Default PRF values should be passed down when the allowlist is empty.
      {
        auto prf_value = blink::mojom::PRFValues::New();
        prf_value->first = salt1;
        prf_value->second = salt2;
        test_client_.delegate_config.expected_accounts =
            "01020304:name:displayName";
        test_client_.delegate_config.selected_user_id = {1, 2, 3, 4};
        std::vector<blink::mojom::PRFValuesPtr> inputs;
        inputs.emplace_back(std::move(prf_value));
        auto result = assertion(std::move(inputs), /*allowlist_size=*/0);
        ASSERT_EQ(result->first, salt1_eval);
        ASSERT_TRUE(result->second);
        ASSERT_EQ(*result->second, salt2_eval);
      }

      // And the default PRF values should be used if none of the specific
      // values match.
      {
        auto prf_value1 = blink::mojom::PRFValues::New();
        prf_value1->first = salt1;
        auto prf_value2 = blink::mojom::PRFValues::New();
        prf_value2->first = std::vector<uint8_t>(32, 3);
        prf_value2->id = std::vector<uint8_t>(32, 4);
        std::vector<blink::mojom::PRFValuesPtr> inputs;
        inputs.emplace_back(std::move(prf_value1));
        inputs.emplace_back(std::move(prf_value2));
        auto result = assertion(std::move(inputs), /*allowlist_size=*/20);
        ASSERT_EQ(result->first, salt1_eval);
        ASSERT_FALSE(result->second);
      }
    }
  }
}

// Tests that the PRF function is evaluated for all credentials in an empty
// allow-list request. Regression test for crbug.com/1520646.
TEST_F(ResidentKeyAuthenticatorImplTest, PRFEvaluationForMultipleCreds) {
  NavigateAndCommit(GURL(kTestOrigin1));
  device::PublicKeyCredentialDescriptor cred1;
  device::PublicKeyCredentialDescriptor cred2;
  device::VirtualCtap2Device::Config config;
  config.prf_support = false;
  config.hmac_secret_support = true;
  config.pin_support = true;
  config.resident_key_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->prf_enable = true;
    options->authenticator_selection->resident_key =
        device::ResidentKeyRequirement::kRequired;
    options->user.id = {1};
    options->user.name = "noah";
    options->user.display_name = "Noah";
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    ASSERT_TRUE(result.response->echo_prf);
    ASSERT_EQ(result.response->prf, true);
    device::AuthenticatorData auth_data =
        AuthDataFromMakeCredentialResponse(result.response);
    cred1 = device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, auth_data.GetCredentialId());
  }
  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->prf_enable = true;
    options->authenticator_selection->resident_key =
        device::ResidentKeyRequirement::kRequired;
    options->user.id = {2};
    options->user.name = "mio";
    options->user.display_name = "Mio";
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    ASSERT_TRUE(result.response->echo_prf);
    ASSERT_EQ(result.response->prf, true);
    device::AuthenticatorData auth_data =
        AuthDataFromMakeCredentialResponse(result.response);
    cred2 = device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, auth_data.GetCredentialId());
  }

  const std::vector<uint8_t> salt(32, 1);
  std::vector<uint8_t> salt1_eval;
  std::vector<uint8_t> salt2_eval;
  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->extensions->prf = true;
    auto prf_value = blink::mojom::PRFValues::New();
    prf_value->first = salt;
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value));
    options->extensions->prf_inputs = std::move(inputs);
    options->allow_credentials.clear();
    test_client_.delegate_config.expected_accounts = "01:noah:Noah/02:mio:Mio";
    test_client_.delegate_config.selected_user_id = {1};
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    ASSERT_TRUE(result.response->extensions->prf_results);
    ASSERT_FALSE(result.response->extensions->prf_results->id);
    salt1_eval = result.response->extensions->prf_results->first;
  }
  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->extensions->prf = true;
    auto prf_value = blink::mojom::PRFValues::New();
    prf_value->first = salt;
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value));
    options->extensions->prf_inputs = std::move(inputs);
    options->allow_credentials.clear();
    test_client_.delegate_config.expected_accounts = "01:noah:Noah/02:mio:Mio";
    test_client_.delegate_config.selected_user_id = {2};
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    ASSERT_TRUE(result.response->extensions->prf_results);
    ASSERT_FALSE(result.response->extensions->prf_results->id);
    salt2_eval = result.response->extensions->prf_results->first;
  }
  EXPECT_NE(salt1_eval, salt2_eval);
}

TEST_F(ResidentKeyAuthenticatorImplTest, PRFEvaluationDuringMakeCredential) {
  // The WebAuthn "prf" extension supports evaluating the PRF when making a
  // credential. The hmac-secret extension does not support this, but hybrid
  // devices (and our virtual authenticator) can support it using the
  // CTAP2-level "prf" extension.
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.prf_support = true;
  config.internal_account_chooser = true;
  config.always_uv = true;
  config.pin_support = true;
  config.resident_key_support = true;
  virtual_device_factory_->SetCtap2Config(config);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->prf_enable = true;
  options->authenticator_selection->resident_key =
      device::ResidentKeyRequirement::kRequired;
  options->user.id = {1, 2, 3, 4};
  options->user.name = "name";
  options->user.display_name = "displayName";
  options->prf_input = blink::mojom::PRFValues::New();
  const std::vector<uint8_t> salt1(32, 1);
  const std::vector<uint8_t> salt2(32, 2);
  options->prf_input->first = salt1;
  options->prf_input->second = salt2;

  MakeCredentialResult result = AuthenticatorMakeCredential(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);

  EXPECT_TRUE(result.response->echo_prf);
  EXPECT_TRUE(result.response->prf);
  ASSERT_TRUE(result.response->prf_results);
  EXPECT_EQ(result.response->prf_results->first.size(), 32u);
  EXPECT_EQ(result.response->prf_results->second->size(), 32u);
}

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialPRFExtension) {
  NavigateAndCommit(GURL(kTestOrigin1));
}

TEST_F(ResidentKeyAuthenticatorImplTest,
       PRFExtensionOnUnconfiguredAuthenticator) {
  // If a credential is on a UV-capable, but not UV-configured authenticator and
  // then an assertion with `prf` is requested there shouldn't be a result
  // because it would be from the wrong PRF. (This state should only happen when
  // the credential was created without the `prf` extension, which is an RP
  // issue.)
  device::VirtualCtap2Device::Config config;
  config.hmac_secret_support = true;
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.resident_key_support = true;
  virtual_device_factory_->SetCtap2Config(config);

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      options->allow_credentials[0].id, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, std::nullopt, std::nullopt));
  device::VirtualFidoDevice::RegistrationData& registration =
      virtual_device_factory_->mutable_state()->registrations.begin()->second;
  const std::array<uint8_t, 32> key1 = {1};
  const std::array<uint8_t, 32> key2 = {2};
  registration.hmac_key.emplace(key1, key2);

  auto prf_value = blink::mojom::PRFValues::New();
  const std::vector<uint8_t> salt1(32, 1);
  prf_value->first = salt1;
  std::vector<blink::mojom::PRFValuesPtr> inputs;
  inputs.emplace_back(std::move(prf_value));

  options->extensions->prf = true;
  options->extensions->prf_inputs = std::move(inputs);
  options->user_verification =
      device::UserVerificationRequirement::kDiscouraged;
  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_FALSE(result.response->extensions->prf_results);
}

TEST_F(ResidentKeyAuthenticatorImplTest, ConditionalUI) {
  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, std::nullopt, std::nullopt));

  // |SelectAccount| should not be called for conditional UI requests.
  test_client_.delegate_config.expected_accounts = "<invalid>";
  test_client_.delegate_config.expect_conditional = true;
  auto options = GetCredentialOptions::New();
  PublicKeyCredentialRequestOptionsPtr public_key(get_credential_options());
  options->mediation = blink::mojom::Mediation::CONDITIONAL;
  options->public_key = std::move(public_key);
  GetAssertionResult result = AuthenticatorGetCredential(std::move(options));
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kSuccess,
                               AuthenticationRequestMode::kConditional);
}

// Tests that the AuthenticatorRequestDelegate can choose a known platform
// authentictor credential as "preselected", which causes the request to be
// specialized to the chosen credential ID and post-request account selection UI
// to be skipped.
TEST_F(ResidentKeyAuthenticatorImplTest, PreselectDiscoverableCredential) {
  virtual_device_factory_->SetTransport(
      device::FidoTransportProtocol::kInternal);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  constexpr char kAuthenticatorId[] = "internal-authenticator";
  virtual_device_factory_->mutable_state()->device_id_override =
      kAuthenticatorId;
  std::vector<uint8_t> kFirstCredentialId{{1, 2, 3, 4}};
  std::vector<uint8_t> kSecondCredentialId{{10, 20, 30, 40}};
  std::vector<uint8_t> kFirstUserId{{2, 3, 4, 5}};
  std::vector<uint8_t> kSecondUserId{{20, 30, 40, 50}};

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      kFirstCredentialId, kTestRelyingPartyId, kFirstUserId, std::nullopt,
      std::nullopt));
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      kSecondCredentialId, kTestRelyingPartyId, kSecondUserId, std::nullopt,
      std::nullopt));
  for (bool has_pin_uv_auth_token : {false, true}) {
    SCOPED_TRACE(has_pin_uv_auth_token);
    device::VirtualCtap2Device::Config config;
    config.pin_uv_auth_token_support = has_pin_uv_auth_token;
    config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
    config.resident_key_support = true;
    config.internal_uv_support = true;
    virtual_device_factory_->SetCtap2Config(std::move(config));

    // |SelectAccount| should not be called if an account was chosen from
    // pre-select UI.
    test_client_.delegate_config.expected_accounts = "<invalid>";

    for (const auto& id : {kFirstCredentialId, kSecondCredentialId}) {
      test_client_.delegate_config.preselected_credential_id = id;
      test_client_.delegate_config.preselected_authenticator_id =
          kAuthenticatorId;
      PublicKeyCredentialRequestOptionsPtr options(get_credential_options());
      GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
      EXPECT_EQ(result.response->info->raw_id, id);
    }
  }
}

// Tests that preselecting a credential sets the response user entity to that of
// the credential metadata if it is not present in the response.
// Regression test for crbug.com/329412574.
TEST_F(ResidentKeyAuthenticatorImplTest, PreselectCredentialUserEntity) {
  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  config.omit_user_entity_on_allow_credentials_requests = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->SetTransport(
      device::FidoTransportProtocol::kInternal);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  constexpr char kAuthenticatorId[] = "internal-authenticator";
  virtual_device_factory_->mutable_state()->device_id_override =
      kAuthenticatorId;
  std::vector<uint8_t> kCredId{{1, 2, 3, 4}};
  std::vector<uint8_t> kUserId{{5, 6, 7, 8}};

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      kCredId, kTestRelyingPartyId, kUserId, std::nullopt, std::nullopt));

  // |SelectAccount| should not be called if an account was chosen from
  // pre-select UI.
  test_client_.delegate_config.expected_accounts = "<invalid>";

  test_client_.delegate_config.preselected_credential_id = kCredId;
  test_client_.delegate_config.preselected_authenticator_id = kAuthenticatorId;
  PublicKeyCredentialRequestOptionsPtr options(get_credential_options());
  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(result.response->info->raw_id, kCredId);
  EXPECT_EQ(result.response->user_handle, kUserId);
}

#if BUILDFLAG(IS_MAC)
class TouchIdAuthenticatorImplTest : public AuthenticatorImplTest {
 protected:
  using Credential = device::fido::mac::Credential;
  using CredentialMetadata = device::fido::mac::CredentialMetadata;

  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    test_client_.web_authentication_delegate.touch_id_authenticator_config =
        config_;
    test_client_.web_authentication_delegate.supports_resident_keys = true;
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

  void ResetVirtualDevice() override {}

  std::vector<Credential> GetCredentials(const std::string& rp_id) {
    return device::fido::mac::TouchIdCredentialStore::FindCredentialsForTesting(
        config_, rp_id);
  }

  TestAuthenticatorContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
  device::fido::mac::AuthenticatorConfig config_{
      .keychain_access_group = "test-keychain-access-group",
      .metadata_secret = "TestMetadataSecret"};
  device::fido::mac::ScopedTouchIdTestEnvironment touch_id_test_environment_{
      config_};
};

TEST_F(TouchIdAuthenticatorImplTest, IsUVPAA) {
  NavigateAndCommit(GURL(kTestOrigin1));
  for (const bool touch_id_available : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "touch_id_available=" << touch_id_available);
    touch_id_test_environment_.SetTouchIdAvailable(touch_id_available);
    EXPECT_EQ(AuthenticatorIsUvpaa(), touch_id_available);
  }
}

TEST_F(TouchIdAuthenticatorImplTest, MakeCredential) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  auto options = GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->authenticator_attachment =
      device::AuthenticatorAttachment::kPlatform;
  touch_id_test_environment_.SimulateTouchIdPromptSuccess();
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  auto credentials = GetCredentials(kTestRelyingPartyId);
  EXPECT_EQ(credentials.size(), 1u);
  const CredentialMetadata& metadata = credentials.at(0).metadata;
  // New credentials are always created discoverable.
  EXPECT_TRUE(metadata.is_resident);
  auto expected_user = GetTestPublicKeyCredentialUserEntity();
  EXPECT_EQ(metadata.ToPublicKeyCredentialUserEntity(), expected_user);
}

TEST_F(TouchIdAuthenticatorImplTest, MakeCredentialUnsupportedAlgorithm) {
  // crbug.com/362766319
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  auto options = GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->authenticator_attachment =
      device::AuthenticatorAttachment::kPlatform;
  options->public_key_parameters = GetTestPublicKeyCredentialParameters(
      static_cast<int32_t>(device::CoseAlgorithmIdentifier::kEdDSA));
  touch_id_test_environment_.SimulateTouchIdPromptSuccess();
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(TouchIdAuthenticatorImplTest, OptionalUv) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  // Disable biometrics to verify that requests without uv required do not
  // prompt the user for their macOS password.
  touch_id_test_environment_.keychain()->SetUVMethod(
      crypto::apple::ScopedFakeKeychainV2::UVMethod::kPasswordOnly);
  for (const auto uv : {device::UserVerificationRequirement::kDiscouraged,
                        device::UserVerificationRequirement::kPreferred,
                        device::UserVerificationRequirement::kRequired}) {
    SCOPED_TRACE(static_cast<int>(uv));
    auto options = GetTestPublicKeyCredentialCreationOptions();
    options->authenticator_selection->authenticator_attachment =
        device::AuthenticatorAttachment::kPlatform;
    // Set rk to required. On platform authenticators Chrome should not
    // universally require UV to make make a resident/discoverable credential,
    // like it would on a security key.
    options->authenticator_selection->resident_key =
        device::ResidentKeyRequirement::kRequired;
    options->authenticator_selection->user_verification_requirement = uv;
    bool requires_uv = uv == device::UserVerificationRequirement::kRequired;
    if (requires_uv) {
      touch_id_test_environment_.SimulateTouchIdPromptSuccess();
    } else {
      touch_id_test_environment_.DoNotResolveNextPrompt();
    }
    auto result = AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(HasUV(result.response), requires_uv);
    auto credentials = GetCredentials(kTestRelyingPartyId);
    EXPECT_EQ(credentials.size(), 1u);

    auto assertion_options = GetTestPublicKeyCredentialRequestOptions();
    assertion_options->user_verification = uv;
    assertion_options->allow_credentials =
        std::vector<device::PublicKeyCredentialDescriptor>(
            {{device::CredentialType::kPublicKey,
              credentials[0].credential_id}});
    if (requires_uv) {
      touch_id_test_environment_.SimulateTouchIdPromptSuccess();
    } else {
      touch_id_test_environment_.DoNotResolveNextPrompt();
    }
    auto assertion = AuthenticatorGetAssertion(std::move(assertion_options));
    EXPECT_EQ(assertion.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(HasUV(assertion.response), requires_uv);
  }
}

TEST_F(TouchIdAuthenticatorImplTest, MakeCredential_Resident) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  auto options = GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->authenticator_attachment =
      device::AuthenticatorAttachment::kPlatform;
  options->authenticator_selection->resident_key =
      device::ResidentKeyRequirement::kRequired;
  touch_id_test_environment_.SimulateTouchIdPromptSuccess();
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  auto credentials = GetCredentials(kTestRelyingPartyId);
  EXPECT_EQ(credentials.size(), 1u);
  EXPECT_TRUE(credentials.at(0).metadata.is_resident);
}

TEST_F(TouchIdAuthenticatorImplTest, MakeCredential_Eviction) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // A resident credential will overwrite the non-resident one.
  auto options = GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->authenticator_attachment =
      device::AuthenticatorAttachment::kPlatform;
  options->authenticator_selection->resident_key =
      device::ResidentKeyRequirement::kRequired;
  touch_id_test_environment_.SimulateTouchIdPromptSuccess();
  EXPECT_EQ(AuthenticatorMakeCredential(options->Clone()).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(GetCredentials(kTestRelyingPartyId).size(), 1u);

  // Another resident credential for the same user will evict the previous one.
  touch_id_test_environment_.SimulateTouchIdPromptSuccess();
  EXPECT_EQ(AuthenticatorMakeCredential(options->Clone()).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(GetCredentials(kTestRelyingPartyId).size(), 1u);

  // But a resident credential for a different user shouldn't.
  touch_id_test_environment_.SimulateTouchIdPromptSuccess();
  options->user.id = std::vector<uint8_t>({99});
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(GetCredentials(kTestRelyingPartyId).size(), 2u);

  // Neither should a credential for a different RP.
  touch_id_test_environment_.SimulateTouchIdPromptSuccess();
  options = GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->authenticator_attachment =
      device::AuthenticatorAttachment::kPlatform;
  options->relying_party.id = "a.google.com";
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(GetCredentials(kTestRelyingPartyId).size(), 2u);
}

class ICloudKeychainAuthenticatorImplTest : public AuthenticatorImplTest {
 protected:
  class InspectTAIAuthenticatorRequestDelegate
      : public DefaultAuthenticatorRequestClientDelegate {
   public:
    using Callback = base::RepeatingCallback<void(
        const device::FidoRequestHandlerBase::TransportAvailabilityInfo&,
        const std::optional<std::string>& icloud_keychain_id,
        device::FidoRequestHandlerBase::RequestCallback request_callback)>;
    explicit InspectTAIAuthenticatorRequestDelegate(Callback callback)
        : callback_(std::move(callback)) {}

    void RegisterActionCallbacks(
        base::OnceClosure cancel_callback,
        base::OnceClosure immediate_not_found_callback,
        base::RepeatingClosure start_over_callback,
        AccountPreselectedCallback account_preselected_callback,
        PasswordSelectedCallback password_selected_callback,
        device::FidoRequestHandlerBase::RequestCallback request_callback,
        base::OnceClosure cancel_ui_timeout_callback,
        base::RepeatingClosure bluetooth_adapter_power_on_callback,
        base::RepeatingCallback<
            void(device::FidoRequestHandlerBase::BlePermissionCallback)>
            request_ble_permission_callback) override {
      request_callback_ = std::move(request_callback);
    }

    void ConfigureDiscoveries(
        const url::Origin& origin,
        const std::string& rp_id,
        RequestSource request_source,
        device::FidoRequestType request_type,
        std::optional<device::ResidentKeyRequirement> resident_key_requirement,
        device::UserVerificationRequirement user_verification_requirement,
        bool cmtg_key_requested,
        std::optional<std::string_view> user_name,
        bool is_enclave_authenticator_available,
        device::FidoDiscoveryFactory* fido_discovery_factory) override {
      fido_discovery_factory->set_allow_no_nswindow_for_testing(true);
    }

    void OnTransportAvailabilityEnumerated(
        device::FidoRequestHandlerBase::TransportAvailabilityInfo tai)
        override {
      callback_.Run(tai, icloud_keychain_id_, request_callback_);
    }

    void FidoAuthenticatorAdded(
        const device::FidoAuthenticator& authenticator) override {
      if (authenticator.GetType() ==
          device::AuthenticatorType::kICloudKeychain) {
        CHECK(!icloud_keychain_id_);
        icloud_keychain_id_ = authenticator.GetId();
      }
    }

   private:
    Callback callback_;
    device::FidoRequestHandlerBase::RequestCallback request_callback_;
    std::optional<std::string> icloud_keychain_id_;
  };

  class InspectTAIContentBrowserClient : public ContentBrowserClient {
   public:
    explicit InspectTAIContentBrowserClient(
        InspectTAIAuthenticatorRequestDelegate::Callback callback)
        : callback_(std::move(callback)) {}

    std::unique_ptr<AuthenticatorRequestClientDelegate>
    GetWebAuthenticationRequestDelegate(
        RenderFrameHost* render_frame_host) override {
      return std::make_unique<InspectTAIAuthenticatorRequestDelegate>(
          callback_);
    }

   private:
    InspectTAIAuthenticatorRequestDelegate::Callback callback_;
  };

  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
    // This test uses the real discoveries and sets the transports on an
    // allowlist entry to limit it to kInternal.
    virtual_device_factory_ = nullptr;
    AuthenticatorEnvironment::GetInstance()->Reset();
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

  void OnTransportAvailabilityEnumerated(
      const device::FidoRequestHandlerBase::TransportAvailabilityInfo& tai,
      const std::optional<std::string>& icloud_keychain_id,
      device::FidoRequestHandlerBase::RequestCallback request_callback) {
    if (tai_callback_) {
      std::move(tai_callback_).Run(tai, icloud_keychain_id, request_callback);
    }
  }

  static std::vector<device::DiscoverableCredentialMetadata> GetCredentials() {
    device::DiscoverableCredentialMetadata metadata(
        device::AuthenticatorType::kICloudKeychain, kTestRelyingPartyId,
        {1, 2, 3, 4}, {{5, 6, 7, 8}, "name", "displayName"},
        /*provider_name=*/std::nullopt);
    return {std::move(metadata)};
  }

  InspectTAIContentBrowserClient test_client_{base::BindRepeating(
      &ICloudKeychainAuthenticatorImplTest::OnTransportAvailabilityEnumerated,
      base::Unretained(this))};
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
  InspectTAIAuthenticatorRequestDelegate::Callback tai_callback_;
};

TEST_F(ICloudKeychainAuthenticatorImplTest, Discovery) {
  if (__builtin_available(macOS 13.5, *)) {
    NavigateAndCommit(GURL(kTestOrigin1));
    device::fido::icloud_keychain::ScopedTestEnvironment test_environment(
        GetCredentials());
    bool tai_seen = false;
    tai_callback_ = base::BindLambdaForTesting(
        [&tai_seen](
            const device::FidoRequestHandlerBase::TransportAvailabilityInfo&
                tai,
            const std::optional<std::string>& icloud_keychain_id,
            device::FidoRequestHandlerBase::RequestCallback request_callback) {
          tai_seen = true;
          CHECK_EQ(tai.has_icloud_keychain, true);
          CHECK_EQ(tai.recognized_credentials.size(), 1u);
          CHECK_EQ(tai.has_icloud_keychain_credential,
                   device::FidoRequestHandlerBase::RecognizedCredential::
                       kHasRecognizedCredential);

          CHECK_EQ(tai.recognized_credentials[0].user.name.value(), "name");
        });

    auto options = GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials.clear();
    options->allow_credentials.push_back(device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, {1, 2, 3, 4},
        {device::FidoTransportProtocol::kInternal}));
    const auto result = AuthenticatorGetAssertion(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
    EXPECT_TRUE(tai_seen);
  } else {
    GTEST_SKIP() << "Need macOS 13.5 for this test";
  }
}

TEST_F(ICloudKeychainAuthenticatorImplTest, PRFOnCreate) {
  if (__builtin_available(macOS 15.0, *)) {
    NavigateAndCommit(GURL(kTestOrigin1));
    device::fido::icloud_keychain::ScopedTestEnvironment test_environment(
        GetCredentials());

    auto prf_value = blink::mojom::PRFValues::New();
    const std::vector<uint8_t> input1(8, 1);
    const std::vector<uint8_t> input2(8, 2);
    prf_value->first = input1;
    prf_value->second = input2;

    bool callback_was_called = false;
    test_environment.SetMakeCredentialCallback(base::BindLambdaForTesting(
        [&input1, &input2, &callback_was_called](
            const device::CtapMakeCredentialRequest& request) {
          CHECK(request.prf);
          CHECK(request.prf_input.has_value());
          CHECK(input1 == request.prf_input->input1);
          CHECK(input2 == request.prf_input->input2);
          callback_was_called = true;
        }));

    auto options = GetTestPublicKeyCredentialCreationOptions();
    options->prf_enable = true;
    options->prf_input = std::move(prf_value);

    const auto result = AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
    EXPECT_TRUE(callback_was_called);
  } else {
    GTEST_SKIP() << "Need macOS 15.0 for this test";
  }
}

TEST_F(ICloudKeychainAuthenticatorImplTest, PRFOnGet) {
  if (__builtin_available(macOS 15.0, *)) {
    NavigateAndCommit(GURL(kTestOrigin1));
    device::fido::icloud_keychain::ScopedTestEnvironment test_environment(
        GetCredentials());

    auto prf_value = blink::mojom::PRFValues::New();
    const std::vector<uint8_t> input1(8, 1);
    const std::vector<uint8_t> input2(8, 2);
    prf_value->first = input1;
    prf_value->second = input2;
    std::vector<blink::mojom::PRFValuesPtr> prf_inputs;
    prf_inputs.emplace_back(std::move(prf_value));

    bool callback_was_called = false;
    test_environment.SetGetAssertionCallback(base::BindLambdaForTesting(
        [&input1, &input2,
         &callback_was_called](const device::CtapGetAssertionRequest& request) {
          CHECK_EQ(request.prf_inputs.size(), 1u);
          CHECK(input1 == request.prf_inputs[0].input1);
          CHECK(input2 == request.prf_inputs[0].input2);
          callback_was_called = true;
        }));

    tai_callback_ = base::BindLambdaForTesting(
        [](const device::FidoRequestHandlerBase::TransportAvailabilityInfo& tai,
           const std::optional<std::string>& icloud_keychain_id,
           device::FidoRequestHandlerBase::RequestCallback request_callback) {
          CHECK_EQ(tai.has_icloud_keychain, true);
          CHECK(icloud_keychain_id.has_value());
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, base::BindOnce(request_callback, *icloud_keychain_id));
        });

    auto options = GetTestPublicKeyCredentialRequestOptions();
    options->extensions->prf = true;
    options->extensions->prf_inputs = std::move(prf_inputs);
    options->allow_credentials.clear();
    options->allow_credentials.push_back(device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, {1, 2, 3, 4},
        {device::FidoTransportProtocol::kInternal}));

    const auto result = AuthenticatorGetAssertion(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
    EXPECT_TRUE(callback_was_called);
  } else {
    GTEST_SKIP() << "Need macOS 15.0 for this test";
  }
}

#endif  // BUILDFLAG(IS_MAC)

TEST_F(ResidentKeyAuthenticatorImplTest,
       GetAssertionImmediateMediationTimeout_NoUI) {
  base::HistogramTester histogram_tester;
  constexpr base::TimeDelta kImmediateTimeout = base::Milliseconds(500);

  ReplaceDiscoveryFactory(std::make_unique<device::FidoDiscoveryFactory>());

  GetCredentialOptionsPtr options = GetTestGetCredentialOptions();
  options->mediation = blink::mojom::Mediation::IMMEDIATE;
  options->public_key->allow_credentials.clear();
  options->public_key->timeout = kTestTimeout;

  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetCredentialFuture future;
  authenticator->GetCredential(std::move(options), future.GetCallback());

  task_environment()->FastForwardBy(kImmediateTimeout);

  EXPECT_TRUE(future.Wait());
  ASSERT_TRUE(future.Get()->is_get_assertion_response());
  auto& get_assertion_response = future.Get()->get_get_assertion_response();
  EXPECT_EQ(get_assertion_response->status,
            AuthenticatorStatus::IMMEDIATE_NOT_FOUND);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.GetAssertion.Immediate.TimeoutWhileWaitingForUi", true,
      1);
}

TEST_F(ResidentKeyAuthenticatorImplTest,
       GetAssertionImmediateMediationTimeout_WithUiThenNoImmediateTimeout) {
  base::HistogramTester histogram_tester;
  test_client_.delegate_config.run_cancel_ui_timeout_callback = true;
  constexpr base::TimeDelta kImmediateTimeout = base::Milliseconds(500);

  ReplaceDiscoveryFactory(std::make_unique<device::FidoDiscoveryFactory>());

  auto options = GetTestGetCredentialOptions();
  options->mediation = blink::mojom::Mediation::IMMEDIATE;
  options->public_key->allow_credentials.clear();
  options->public_key->timeout = kTestTimeout;

  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetCredentialFuture future;

  authenticator->GetCredential(std::move(options), future.GetCallback());
  // Fast forward by the immediate mediation timeout.
  task_environment()->FastForwardBy(kImmediateTimeout + base::Milliseconds(1));

  // The request should NOT be complete yet because UI is displayed,
  // which bypasses the immediate timeout.
  EXPECT_FALSE(future.IsReady());
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.GetAssertion.Immediate.TimeoutWhileWaitingForUi",
      false, 1);
  test_client_.delegate_config.run_cancel_ui_timeout_callback = false;
}

}  // namespace
}  // namespace content
