// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_test_base.h"

#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/webauth_request_security_checker.h"
#include "content/public/browser/web_authentication_delegate.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/functions.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#endif

namespace content {

TestAuthenticatorRequestDelegate::TestAuthenticatorRequestDelegate(
    RenderFrameHost* render_frame_host,
    base::OnceClosure action_callbacks_registered_callback,
    base::OnceClosure started_over_callback,
    bool simulate_user_cancelled,
    base::RepeatingCallback<void(bool)> enclave_discovered_callback,
    base::RepeatingCallback<void(const base::flat_set<device::FidoTransportProtocol>&)>
        transports_discovered_callback)
    : action_callbacks_registered_callback_(
          std::move(action_callbacks_registered_callback)),
      started_over_callback_(std::move(started_over_callback)),
      does_block_request_on_failure_(!started_over_callback_.is_null()),
      simulate_user_cancelled_(simulate_user_cancelled),
      enclave_discovered_callback_(std::move(enclave_discovered_callback)),
      transports_discovered_callback_(
          std::move(transports_discovered_callback)) {}

TestAuthenticatorRequestDelegate::~TestAuthenticatorRequestDelegate() = default;

void TestAuthenticatorRequestDelegate::RegisterActionCallbacks(
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
        ble_status_callback) {
  ASSERT_TRUE(action_callbacks_registered_callback_)
      << "RegisterActionCallbacks called twice.";
  cancel_callback_ = std::move(cancel_callback);
  std::move(action_callbacks_registered_callback_).Run();
  if (started_over_callback_) {
    action_callbacks_registered_callback_ = std::move(started_over_callback_);
    start_over_callback_ = start_over_callback;
  }
}

void TestAuthenticatorRequestDelegate::OnTransportAvailabilityEnumerated(
    device::FidoRequestHandlerBase::TransportAvailabilityInfo transport_info) {
  if (transports_discovered_callback_) {
    transports_discovered_callback_.Run(transport_info.available_transports);
  }
  // Simulate the behaviour of Chrome's |AuthenticatorRequestDialogModel|
  // which shows a specific error when no transports are available and lets
  // the user cancel the request.
  if (transport_info.available_transports.empty() || simulate_user_cancelled_) {
    std::move(cancel_callback_).Run();
  }
}

bool TestAuthenticatorRequestDelegate::DoesBlockRequestOnFailure(
    InterestingFailureReason reason) {
  if (!does_block_request_on_failure_) {
    return false;
  }

  std::move(start_over_callback_).Run();
  does_block_request_on_failure_ = false;
  return true;
}

void TestAuthenticatorRequestDelegate::ConfigureDiscoveries(
    const url::Origin& origin,
    const std::string& rp_id,
    RequestSource request_source,
    device::FidoRequestType request_type,
    std::optional<device::ResidentKeyRequirement> resident_key_requirement,
    device::UserVerificationRequirement user_verification_requirement,
    std::optional<std::string_view> user_name,
    base::span<const device::CableDiscoveryData> pairings_from_extension,
    bool is_enclave_authenticator_available,
    device::FidoDiscoveryFactory* fido_discovery_factory) {
  if (enclave_discovered_callback_) {
    enclave_discovered_callback_.Run(is_enclave_authenticator_available);
  }
}

TestWebAuthenticationRequestProxy::Config::Config() = default;
TestWebAuthenticationRequestProxy::Config::~Config() = default;

TestWebAuthenticationRequestProxy::Observations::Observations() = default;
TestWebAuthenticationRequestProxy::Observations::~Observations() = default;

TestWebAuthenticationRequestProxy::TestWebAuthenticationRequestProxy() =
    default;

TestWebAuthenticationRequestProxy::~TestWebAuthenticationRequestProxy() {
  DCHECK(!HasPendingRequest());
}

bool TestWebAuthenticationRequestProxy::IsActive(
    const url::Origin& caller_origin) {
  return config_.is_active;
}

WebAuthenticationRequestProxy::RequestId
TestWebAuthenticationRequestProxy::SignalCreateRequest(
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options,
    CreateCallback callback) {
  DCHECK(!HasPendingRequest());

  current_request_id_++;
  observations_.create_requests.push_back(options->Clone());
  pending_create_callback_ = std::move(callback);
  if (config_.resolve_callbacks) {
    RunPendingCreateCallback();
    return current_request_id_;
  }
  return current_request_id_;
}

WebAuthenticationRequestProxy::RequestId
TestWebAuthenticationRequestProxy::SignalGetRequest(
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options,
    GetCallback callback) {
  current_request_id_++;
  observations_.get_requests.push_back(options->Clone());
  pending_get_callback_ = std::move(callback);
  if (config_.resolve_callbacks) {
    RunPendingGetCallback();
    return current_request_id_;
  }
  return current_request_id_;
}

WebAuthenticationRequestProxy::RequestId
TestWebAuthenticationRequestProxy::SignalIsUvpaaRequest(
    IsUvpaaCallback callback) {
  DCHECK(!HasPendingRequest());

  current_request_id_++;
  observations_.num_isuvpaa++;
  if (config_.resolve_callbacks) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), config_.is_uvpaa));
    return current_request_id_;
  }
  DCHECK(!pending_is_uvpaa_callback_);
  pending_is_uvpaa_callback_ = std::move(callback);
  return current_request_id_;
}

void TestWebAuthenticationRequestProxy::CancelRequest(RequestId request_id) {
  DCHECK_EQ(request_id, current_request_id_);
  observations_.num_cancel++;
  if (pending_create_callback_) {
    pending_create_callback_.Reset();
  }
  if (pending_get_callback_) {
    pending_get_callback_.Reset();
  }
}

void TestWebAuthenticationRequestProxy::RunPendingCreateCallback() {
  DCHECK(pending_create_callback_);
  auto callback =
      config_.request_success
          ? base::BindOnce(std::move(pending_create_callback_),
                           current_request_id_, nullptr,
                           config_.make_credential_response.Clone())
          : base::BindOnce(std::move(pending_create_callback_),
                           current_request_id_,
                           blink::mojom::WebAuthnDOMExceptionDetails::New(
                               config_.request_error_name, "message"),
                           nullptr);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

void TestWebAuthenticationRequestProxy::RunPendingGetCallback() {
  DCHECK(pending_get_callback_);
  auto callback =
      config_.request_success
          ? base::BindOnce(std::move(pending_get_callback_),
                           current_request_id_, nullptr,
                           config_.get_assertion_response.Clone())
          : base::BindOnce(std::move(pending_create_callback_),
                           current_request_id_,
                           blink::mojom::WebAuthnDOMExceptionDetails::New(
                               config_.request_error_name, "message"),
                           nullptr);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(callback));
}

void TestWebAuthenticationRequestProxy::RunPendingIsUvpaaCallback() {
  DCHECK(pending_is_uvpaa_callback_);
  std::move(pending_is_uvpaa_callback_).Run(config_.is_uvpaa);
}

bool TestWebAuthenticationRequestProxy::HasPendingRequest() {
  return pending_create_callback_ || pending_get_callback_ ||
         pending_is_uvpaa_callback_;
}

TestWebAuthenticationDelegate::TestWebAuthenticationDelegate() = default;
TestWebAuthenticationDelegate::~TestWebAuthenticationDelegate() = default;

void TestWebAuthenticationDelegate::
    IsUserVerifyingPlatformAuthenticatorAvailableOverride(
        RenderFrameHost*,
        base::OnceCallback<void(std::optional<bool>)> callback) {
  std::move(callback).Run(is_uvpaa_override);
}

bool TestWebAuthenticationDelegate::
    OverrideCallerOriginAndRelyingPartyIdValidation(
        content::BrowserContext* browser_context,
        const url::Origin& origin,
        const std::string& rp_id) {
  return permit_extensions && origin.scheme() == "chrome-extension" &&
         origin.host() == rp_id;
}

std::optional<std::string>
TestWebAuthenticationDelegate::MaybeGetRelyingPartyIdOverride(
    const std::string& claimed_rp_id,
    const url::Origin& caller_origin) {
  if (permit_extensions && caller_origin.scheme() == "chrome-extension") {
    return caller_origin.Serialize();
  }
  return std::nullopt;
}

bool TestWebAuthenticationDelegate::ShouldPermitIndividualAttestation(
    content::BrowserContext* browser_context,
    const url::Origin& caller_origin,
    const std::string& relying_party_id) {
  return permit_individual_attestation ||
         (permit_individual_attestation_for_rp_id.has_value() &&
          relying_party_id == *permit_individual_attestation_for_rp_id);
}

bool TestWebAuthenticationDelegate::SupportsResidentKeys(RenderFrameHost*) {
  return supports_resident_keys;
}

bool TestWebAuthenticationDelegate::IsFocused(WebContents* web_contents) {
  return is_focused;
}

#if BUILDFLAG(IS_MAC)
std::optional<WebAuthenticationDelegate::TouchIdAuthenticatorConfig>
TestWebAuthenticationDelegate::GetTouchIdAuthenticatorConfig(
    BrowserContext* browser_context) {
  return touch_id_authenticator_config;
}
#endif

WebAuthenticationRequestProxy*
TestWebAuthenticationDelegate::MaybeGetRequestProxy(
    content::BrowserContext* browser_context,
    const url::Origin& caller_origin) {
  return request_proxy && request_proxy->IsActive(caller_origin)
             ? request_proxy.get()
             : nullptr;
}

bool TestWebAuthenticationDelegate::OriginMayUseRemoteDesktopClientOverride(
    content::BrowserContext* browser_context,
    const url::Origin& caller_origin) {
  return caller_origin == remote_desktop_client_override_origin;
}

void TestWebAuthenticationDelegate::BrowserProvidedPasskeysAvailable(
    BrowserContext* browser_context,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(browser_provided_passkeys_available);
}

TestAuthenticatorContentBrowserClient::TestAuthenticatorContentBrowserClient() =
    default;
TestAuthenticatorContentBrowserClient::
    ~TestAuthenticatorContentBrowserClient() = default;

TestWebAuthenticationDelegate*
TestAuthenticatorContentBrowserClient::GetTestWebAuthenticationDelegate() {
  return &web_authentication_delegate;
}

WebAuthenticationDelegate*
TestAuthenticatorContentBrowserClient::GetWebAuthenticationDelegate() {
  return &web_authentication_delegate;
}

bool TestAuthenticatorContentBrowserClient::
    IsSecurityLevelAcceptableForWebAuthn(content::RenderFrameHost* rfh,
                                         const url::Origin& origin) {
  return is_webauthn_security_level_acceptable;
}

bool TestAuthenticatorContentBrowserClient::ShouldDisallowCredentialRequest(
    WebContents* web_contents) {
  return should_disallow_credential_request;
}

std::unique_ptr<AuthenticatorRequestClientDelegate>
TestAuthenticatorContentBrowserClient::GetWebAuthenticationRequestDelegate(
    RenderFrameHost* render_frame_host) {
  if (return_null_delegate) {
    return nullptr;
  }
  return std::make_unique<TestAuthenticatorRequestDelegate>(
      render_frame_host,
      action_callbacks_registered_callback
          ? std::move(action_callbacks_registered_callback)
          : base::DoNothing(),
      std::move(started_over_callback_),
      simulate_user_cancelled_,
      base::BindRepeating(
          [](base::WeakPtr<TestAuthenticatorContentBrowserClient> self,
             bool discovered) {
            if (self) {
              self->enclave_authenticator_should_be_discovered_ = discovered;
            }
          },
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          [](base::WeakPtr<TestAuthenticatorContentBrowserClient> self,
             const base::flat_set<device::FidoTransportProtocol>& transports) {
            if (self) {
              self->discovered_transports_ = transports;
            }
          },
          weak_factory_.GetWeakPtr()));
}

AuthenticatorTestBase::AuthenticatorTestBase()
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

AuthenticatorTestBase::~AuthenticatorTestBase() = default;

void AuthenticatorTestBase::SetUpTestSuite() {
#if BUILDFLAG(IS_MAC)
  // Load fido_strings, which can be required for exercising the Touch ID
  // authenticator.
  base::FilePath path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &path));
  base::FilePath fido_test_strings =
      path.Append(FILE_PATH_LITERAL("fido_test_strings.pak"));
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      fido_test_strings, ui::kScaleFactorNone);
#endif
}

void AuthenticatorTestBase::SetUp() {
  RenderViewHostTestHarness::SetUp();

  WebAuthRequestSecurityChecker::UseSystemSharedURLLoaderFactoryForTesting() =
      true;

  mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
      &AuthenticatorTestBase::OnMojoError, base::Unretained(this)));

#if BUILDFLAG(IS_CHROMEOS)
  chromeos::TpmManagerClient::InitializeFake();
  chromeos::U2FClient::InitializeFake();
#endif

#if BUILDFLAG(IS_WIN)
  // Disable the Windows WebAuthn API integration by default. Individual tests
  // can modify this.
  fake_win_webauthn_api_.set_available(false);

  // Prevent `FidoRequestHandlerBase` from doing a system API call, which can
  // cause tests to finish early since `RunUntilIdle` won't see it in the task
  // queue.
  biometrics_override_ =
      std::make_unique<device::fido::win::ScopedBiometricsOverride>(false);
#endif

  ResetVirtualDevice();
}

void AuthenticatorTestBase::TearDown() {
  RenderViewHostTestHarness::TearDown();
  WebAuthRequestSecurityChecker::UseSystemSharedURLLoaderFactoryForTesting() =
      false;

  mojo::SetDefaultProcessErrorHandler(base::NullCallback());

  virtual_device_factory_ = nullptr;
  AuthenticatorEnvironment::GetInstance()->Reset();
#if BUILDFLAG(IS_CHROMEOS)
  chromeos::U2FClient::Shutdown();
  chromeos::TpmManagerClient::Shutdown();
#endif
}

void AuthenticatorTestBase::ResetVirtualDevice() {
  auto virtual_device_factory =
      std::make_unique<device::test::VirtualFidoDeviceFactory>();
  virtual_device_factory_ = virtual_device_factory.get();
  AuthenticatorEnvironment::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::move(virtual_device_factory));
}

void AuthenticatorTestBase::ReplaceDiscoveryFactory(
    std::unique_ptr<device::FidoDiscoveryFactory> device_factory) {
  virtual_device_factory_ = nullptr;
  AuthenticatorEnvironment::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(std::move(device_factory));
}

void AuthenticatorTestBase::SetMojoErrorHandler(
    base::RepeatingCallback<void(const std::string&)> callback) {
  mojo_error_handler_ = callback;
}

void AuthenticatorTestBase::OnMojoError(const std::string& error) {
  if (mojo_error_handler_) {
    mojo_error_handler_.Run(error);
    return;
  }
  FAIL() << "Unhandled mojo error: " << error;
}

device::PublicKeyCredentialUserEntity GetTestPublicKeyCredentialUserEntity() {
  device::PublicKeyCredentialUserEntity entity;
  entity.display_name = "User A. Name";
  std::vector<uint8_t> id(32, 0x0A);
  entity.id = id;
  entity.name = "username@example.com";
  return entity;
}

device::AuthenticatorSelectionCriteria GetTestAuthenticatorSelectionCriteria() {
  return device::AuthenticatorSelectionCriteria(
      device::AuthenticatorAttachment::kAny,
      device::ResidentKeyRequirement::kDiscouraged,
      device::UserVerificationRequirement::kPreferred);
}

std::vector<device::PublicKeyCredentialDescriptor> GetTestCredentials(
    size_t num_credentials) {
  std::vector<device::PublicKeyCredentialDescriptor> descriptors;
  for (size_t i = 0; i < num_credentials; i++) {
    DCHECK(i <= std::numeric_limits<uint8_t>::max());
    std::vector<uint8_t> id(kTestCredentialIdLength, static_cast<uint8_t>(i));
    base::flat_set<device::FidoTransportProtocol> transports{
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
        device::FidoTransportProtocol::kBluetoothLowEnergy};
    descriptors.emplace_back(device::CredentialType::kPublicKey, std::move(id),
                             std::move(transports));
  }
  return descriptors;
}

std::vector<device::PublicKeyCredentialParams::CredentialInfo>
GetTestPublicKeyCredentialParameters(int32_t algorithm_identifier) {
  std::vector<device::PublicKeyCredentialParams::CredentialInfo> parameters;
  device::PublicKeyCredentialParams::CredentialInfo fake_parameter;
  fake_parameter.type = device::CredentialType::kPublicKey;
  fake_parameter.algorithm = algorithm_identifier;
  parameters.push_back(std::move(fake_parameter));
  return parameters;
}

device::PublicKeyCredentialRpEntity GetTestPublicKeyCredentialRPEntity() {
  device::PublicKeyCredentialRpEntity entity;
  entity.id = std::string(kTestRelyingPartyId);
  entity.name = "TestRP@example.com";
  return entity;
}

PublicKeyCredentialCreationOptionsPtr
GetTestPublicKeyCredentialCreationOptions() {
  auto options = PublicKeyCredentialCreationOptions::New();
  options->relying_party = GetTestPublicKeyCredentialRPEntity();
  options->user = GetTestPublicKeyCredentialUserEntity();
  options->public_key_parameters = GetTestPublicKeyCredentialParameters(
      static_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256));
  options->challenge.assign(32, 0x0A);
  options->timeout = base::Minutes(1);
  options->authenticator_selection = GetTestAuthenticatorSelectionCriteria();
  return options;
}

PublicKeyCredentialRequestOptionsPtr
GetTestPublicKeyCredentialRequestOptions() {
  auto options = PublicKeyCredentialRequestOptions::New();
  options->extensions = AuthenticationExtensionsClientInputs::New();
  options->relying_party_id = std::string(kTestRelyingPartyId);
  options->challenge = std::vector<uint8_t>(32, 0x0A);
  options->timeout = base::Minutes(1);
  options->user_verification = device::UserVerificationRequirement::kPreferred;
  options->allow_credentials = GetTestCredentials();
  return options;
}

GetCredentialOptionsPtr GetTestGetCredentialOptions() {
  auto options = GetCredentialOptions::New();
  options->public_key = GetTestPublicKeyCredentialRequestOptions();

  return options;
}

}  // namespace content
