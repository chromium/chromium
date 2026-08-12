// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl_unittest_test_base.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_common_impl.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/browser/webauth/authenticator_request_outcome_enums.h"
#include "content/browser/webauth/authenticator_test_base.h"
#include "content/browser/webauth/client_data_json.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_authentication_delegate.h"
#include "content/public/common/content_client.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/large_blob.h"
#include "device/fido/multiple_virtual_fido_device_factory.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/gzipper.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

using ::blink::mojom::AttestationConveyancePreference;
using ::blink::mojom::AuthenticatorStatus;
using ::blink::mojom::GetAssertionAuthenticatorResponsePtr;
using ::blink::mojom::GetCredentialOptionsPtr;
using ::blink::mojom::MakeCredentialAuthenticatorResponsePtr;
using ::blink::mojom::PublicKeyCredentialCreationOptionsPtr;
using ::blink::mojom::PublicKeyCredentialReportOptionsPtr;
using ::blink::mojom::PublicKeyCredentialRequestOptionsPtr;

const uint8_t AuthenticatorImplTest::kTestChallengeBytes[] = {
    0x68, 0x71, 0x34, 0x96, 0x82, 0x22, 0xEC, 0x17, 0x20, 0x2E, 0x42,
    0x50, 0x5F, 0x8E, 0xD2, 0xB1, 0x6A, 0xE2, 0x2F, 0x16, 0xBB, 0x05,
    0xB8, 0x8C, 0x25, 0xDB, 0x9E, 0x60, 0x26, 0x45, 0xF1, 0x41};

std::vector<uint8_t> AuthenticatorImplTest::GetTestChallengeBytes() {
  return std::vector<uint8_t>(std::begin(kTestChallengeBytes),
                              std::end(kTestChallengeBytes));
}

PublicKeyCredentialReportOptionsPtr
AuthenticatorImplTest::GetTestPublicKeyCredentialReportOptions() {
  auto options = blink::mojom::PublicKeyCredentialReportOptions::New();
  options->relying_party_id = std::string(kTestRelyingPartyId);
  return options;
}

device::AuthenticatorData
AuthenticatorImplTest::AuthDataFromMakeCredentialResponse(
    const MakeCredentialAuthenticatorResponsePtr& response) {
  std::optional<cbor::Value> attestation_value =
      cbor::Reader::Read(response->attestation_object);
  CHECK(attestation_value);
  const auto& attestation = attestation_value->GetMap();

  const auto auth_data_it = attestation.find(cbor::Value(device::kAuthDataKey));
  CHECK(auth_data_it != attestation.end());
  const std::vector<uint8_t>& auth_data = auth_data_it->second.GetBytestring();
  std::optional<device::AuthenticatorData> parsed_auth_data =
      device::AuthenticatorData::DecodeAuthenticatorData(auth_data);
  return std::move(parsed_auth_data.value());
}

bool AuthenticatorImplTest::HasUV(
    const MakeCredentialAuthenticatorResponsePtr& response) {
  return AuthDataFromMakeCredentialResponse(response)
      .obtained_user_verification();
}

bool AuthenticatorImplTest::HasUV(
    const GetAssertionAuthenticatorResponsePtr& response) {
  std::optional<device::AuthenticatorData> auth_data =
      device::AuthenticatorData::DecodeAuthenticatorData(
          response->info->authenticator_data);
  return auth_data->obtained_user_verification();
}

url::Origin AuthenticatorImplTest::GetTestOrigin() {
  const GURL test_relying_party_url(kTestOrigin1);
  CHECK(test_relying_party_url.is_valid());
  return url::Origin::Create(test_relying_party_url);
}

std::string AuthenticatorImplTest::GetTestClientDataJSON(
    webauthn::ClientDataRequestType type) {
  return BuildClientDataJson({std::move(type), GetTestOrigin(), GetTestOrigin(),
                              GetTestChallengeBytes(),
                              /*is_cross_origin_iframe=*/false});
}

device::LargeBlob AuthenticatorImplTest::CompressLargeBlob(
    base::span<const uint8_t> blob) {
  data_decoder::Gzipper gzipper;
  std::vector<uint8_t> compressed;
  base::RunLoop run_loop;
  gzipper.Deflate(blob, base::BindLambdaForTesting(
                            [&](std::optional<mojo_base::BigBuffer> result) {
                              compressed = base::ToVector(*result);
                              run_loop.Quit();
                            }));
  run_loop.Run();
  return device::LargeBlob(std::move(compressed), blob.size());
}

std::vector<uint8_t> AuthenticatorImplTest::UncompressLargeBlob(
    device::LargeBlob blob) {
  data_decoder::Gzipper gzipper;
  std::vector<uint8_t> uncompressed;
  base::RunLoop run_loop;
  gzipper.Inflate(
      {blob.compressed_data}, blob.original_size,
      base::BindLambdaForTesting(
          [&](std::optional<mojo_base::BigBuffer> result) {
            if (result) {
              uncompressed = base::ToVector(*result);
            } else {
              // Magic value to indicate failure.
              const char kErrorMsg[] = "decompress error";
              uncompressed.assign(
                  reinterpret_cast<const uint8_t*>(kErrorMsg),
                  reinterpret_cast<const uint8_t*>(std::end(kErrorMsg)));
            }
            run_loop.Quit();
          }));
  run_loop.Run();
  return uncompressed;
}

device::AttestationConveyancePreference
AuthenticatorImplTest::ConvertAttestationConveyancePreference(
    AttestationConveyancePreference in) {
  switch (in) {
    case AttestationConveyancePreference::NONE:
      return ::device::AttestationConveyancePreference::kNone;
    case AttestationConveyancePreference::INDIRECT:
      return ::device::AttestationConveyancePreference::kIndirect;
    case AttestationConveyancePreference::DIRECT:
      return ::device::AttestationConveyancePreference::kDirect;
    case AttestationConveyancePreference::ENTERPRISE:
      return ::device::AttestationConveyancePreference::
          kEnterpriseIfRPListedOnAuthenticator;
  }
}

AuthenticatorImplTest::AuthenticatorImplTest() {
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
}

AuthenticatorImplTest::~AuthenticatorImplTest() = default;

void AuthenticatorImplTest::SetUp() {
  AuthenticatorTestBase::SetUp();
  SetBluetoothLESupported(true);
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
}

void AuthenticatorImplTest::SetBluetoothLESupported(bool supported) {
  bluetooth_global_values_->SetLESupported(supported);
}

void AuthenticatorImplTest::NavigateAndCommit(const GURL& url) {
  RenderViewHostTestHarness::NavigateAndCommit(url);
}

mojo::Remote<blink::mojom::Authenticator>
AuthenticatorImplTest::ConnectToAuthenticator() {
  mojo::Remote<blink::mojom::Authenticator> authenticator;
  static_cast<RenderFrameHostImpl*>(main_rfh())
      ->GetWebAuthenticationService(authenticator.BindNewPipeAndPassReceiver());
  return authenticator;
}

bool AuthenticatorImplTest::AuthenticatorIsUvpaa() {
  TestIsUvpaaFuture future;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(
      future.GetCallback());
  EXPECT_TRUE(future.Wait());
  return future.Get();
}

AuthenticatorImplTest::ClientCapabilitiesList
AuthenticatorImplTest::AuthenticatorGetClientCapabilities() {
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetClientCapabilityFuture future;
  authenticator->GetClientCapabilities(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  return future.Take();
}

void AuthenticatorImplTest::ExpectCapability(
    const std::vector<blink::mojom::WebAuthnClientCapabilityPtr>& capabilities,
    std::string_view capability_name,
    std::optional<bool> supported) {
  auto capability_it =
      std::find_if(capabilities.begin(), capabilities.end(),
                   [&capability_name](const auto& capability) {
                     return capability->name == capability_name;
                   });

  if (supported.has_value()) {
    ASSERT_NE(capability_it, capabilities.end());
    EXPECT_EQ(supported, (*capability_it)->supported);
  } else {
    EXPECT_EQ(capability_it, capabilities.end());
  }
}

bool AuthenticatorImplTest::AuthenticatorIsConditionalMediationAvailable() {
  TestIsUvpaaFuture future;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  authenticator->IsConditionalMediationAvailable(future.GetCallback());
  EXPECT_TRUE(future.Wait());
  return future.Get();
}

AuthenticatorImplTest::MakeCredentialResult
AuthenticatorImplTest::AuthenticatorMakeCredential() {
  return AuthenticatorMakeCredential(
      GetTestPublicKeyCredentialCreationOptions());
}

AuthenticatorImplTest::MakeCredentialResult
AuthenticatorImplTest::AuthenticatorMakeCredential(
    PublicKeyCredentialCreationOptionsPtr options) {
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestMakeCredentialFuture future;
  authenticator->MakeCredential(std::move(options), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  auto [status, response, dom_exception] = future.Take();
  return {status, std::move(response)};
}

AuthenticatorImplTest::MakeCredentialResult
AuthenticatorImplTest::AuthenticatorMakeCredentialAndWaitForTimeout(
    PublicKeyCredentialCreationOptionsPtr options) {
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestMakeCredentialFuture future;
  authenticator->MakeCredential(std::move(options), future.GetCallback());
  task_environment()->FastForwardBy(kTestTimeout);
  EXPECT_TRUE(future.Wait());
  auto [status, response, dom_exception] = future.Take();
  return {status, std::move(response)};
}

AuthenticatorImplTest::GetAssertionResult
AuthenticatorImplTest::AuthenticatorGetAssertion() {
  return AuthenticatorGetAssertion(GetTestPublicKeyCredentialRequestOptions());
}

AuthenticatorImplTest::GetAssertionResult
AuthenticatorImplTest::AuthenticatorGetAssertion(
    PublicKeyCredentialRequestOptionsPtr options) {
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetCredentialFuture future;
  blink::mojom::GetCredentialOptionsPtr get_credential_options =
      blink::mojom::GetCredentialOptions::New();
  get_credential_options->public_key = std::move(options);
  authenticator->GetCredential(std::move(get_credential_options),
                               future.GetCallback());
  EXPECT_TRUE(future.Wait());
  auto get_assertion_response =
      std::move(future.Take()->get_get_assertion_response());
  return {get_assertion_response->status,
          std::move(get_assertion_response->credential)};
}

AuthenticatorImplTest::GetAssertionResult
AuthenticatorImplTest::AuthenticatorGetAssertionAndWaitForTimeout(
    PublicKeyCredentialRequestOptionsPtr options) {
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetCredentialFuture future;
  blink::mojom::GetCredentialOptionsPtr get_credential_options =
      blink::mojom::GetCredentialOptions::New();
  get_credential_options->public_key = std::move(options);
  authenticator->GetCredential(std::move(get_credential_options),
                               future.GetCallback());
  task_environment()->FastForwardBy(kTestTimeout);
  auto get_assertion_response =
      std::move(future.Take()->get_get_assertion_response());
  return {get_assertion_response->status,
          std::move(get_assertion_response->credential)};
}

AuthenticatorImplTest::GetAssertionResult
AuthenticatorImplTest::AuthenticatorGetCredential(
    GetCredentialOptionsPtr options) {
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetCredentialFuture future;
  authenticator->GetCredential(std::move(options), future.GetCallback());
  task_environment()->FastForwardBy(kTestTimeout);
  auto get_assertion_response =
      std::move(future.Take()->get_get_assertion_response());
  return {get_assertion_response->status,
          std::move(get_assertion_response->credential)};
}

AuthenticatorStatus AuthenticatorImplTest::AuthenticatorReport(
    PublicKeyCredentialReportOptionsPtr options) {
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestReportFuture future;
  authenticator->Report(std::move(options), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  auto [status, dom_exception] = future.Take();
  return status;
}

AuthenticatorStatus AuthenticatorImplTest::TryAuthenticationWithAppId(
    std::string_view origin,
    std::string_view appid) {
  const GURL origin_url(origin);
  NavigateAndCommit(origin_url);

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->relying_party_id = origin_url.GetHost();
  options->extensions->appid = appid;

  return AuthenticatorGetAssertion(std::move(options)).status;
}

AuthenticatorStatus AuthenticatorImplTest::TryRegistrationWithAppIdExclude(
    std::string_view origin,
    std::string_view appid_exclude) {
  const GURL origin_url(origin);
  NavigateAndCommit(origin_url);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = origin_url.GetHost();
  options->appid_exclude = appid_exclude;

  return AuthenticatorMakeCredential(std::move(options)).status;
}

ukm::TestUkmRecorder* AuthenticatorImplTest::GetTestUkmRecorder() {
  return &test_ukm_recorder_;
}

void AuthenticatorImplTest::VerifyGetAssertionOutcomeUkm(
    uint32_t index,
    GetAssertionOutcome outcome,
    AuthenticationRequestMode mode) {
  auto entries = GetTestUkmRecorder()->GetEntriesByName(
      ukm::builders::WebAuthn_SignCompletion::kEntryName);
  ASSERT_GT(entries.size(), index);
  GetTestUkmRecorder()->ExpectEntryMetric(
      entries[index], "SignCompletionResult", static_cast<int64_t>(outcome));
  GetTestUkmRecorder()->ExpectEntryMetric(entries[index], "RequestMode",
                                          static_cast<int64_t>(mode));
}

void AuthenticatorImplTest::VerifyMakeCredentialOutcomeUkm(
    uint32_t index,
    MakeCredentialOutcome outcome,
    AuthenticationRequestMode mode) {
  auto entries = GetTestUkmRecorder()->GetEntriesByName(
      ukm::builders::WebAuthn_RegisterCompletion::kEntryName);
  ASSERT_GT(entries.size(), index);
  GetTestUkmRecorder()->ExpectEntryMetric(entries[index],
                                          "RegisterCompletionResult",
                                          static_cast<int64_t>(outcome));
  GetTestUkmRecorder()->ExpectEntryMetric(entries[index], "RequestMode",
                                          static_cast<int64_t>(mode));
}

void AuthenticatorImplTest::InjectVirtualAuthenticatorForAllTransports() {
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillRepeatedly(::testing::Return(true));
  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  for (device::FidoTransportProtocol transport : {
           device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
           device::FidoTransportProtocol::kNearFieldCommunication,
           device::FidoTransportProtocol::kBluetoothLowEnergy,
           device::FidoTransportProtocol::kHybrid,
           device::FidoTransportProtocol::kInternal,
           device::FidoTransportProtocol::kSmartCard,
       }) {
    device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device;
    device.transport = transport;
    device.state->transport = transport;
    ASSERT_TRUE(device.state->InjectResidentKey(
        /*credential_id=*/{{1, 2, 3, 4}}, kTestRelyingPartyId,
        /*user_id=*/{{1, 1, 1, 1}}, "test@example.com", "Test User"));
    discovery->AddDevice(std::move(device));
  }
  ReplaceDiscoveryFactory(std::move(discovery));
}

UVTestAuthenticatorClientDelegate::UVTestAuthenticatorClientDelegate(
    bool* collected_pin,
    uint32_t* min_pin_length,
    bool* did_bio_enrollment,
    bool cancel_bio_enrollment,
    bool block_request_on_failure_once)
    : collected_pin_(collected_pin),
      min_pin_length_(min_pin_length),
      did_bio_enrollment_(did_bio_enrollment),
      cancel_bio_enrollment_(cancel_bio_enrollment),
      block_request_on_failure_once_(block_request_on_failure_once) {
  *collected_pin_ = false;
  *did_bio_enrollment_ = false;
}

UVTestAuthenticatorClientDelegate::~UVTestAuthenticatorClientDelegate() =
    default;

bool UVTestAuthenticatorClientDelegate::DoesBlockRequestOnFailure(
    InterestingFailureReason reason) {
  bool block = block_request_on_failure_once_;
  block_request_on_failure_once_ = false;
  return block;
}

bool UVTestAuthenticatorClientDelegate::SupportsPIN() const {
  return true;
}

void UVTestAuthenticatorClientDelegate::CollectPIN(
    CollectPINOptions options,
    base::OnceCallback<void(std::u16string)> provide_pin_cb) {
  *collected_pin_ = true;
  *min_pin_length_ = options.min_pin_length;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(provide_pin_cb),
                                AuthenticatorImplTest::kTestPIN16));
}

void UVTestAuthenticatorClientDelegate::StartBioEnrollment(
    base::OnceClosure next_callback) {
  *did_bio_enrollment_ = true;
  if (cancel_bio_enrollment_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(next_callback));
    return;
  }
  bio_callback_ = std::move(next_callback);
}

void UVTestAuthenticatorClientDelegate::OnSampleCollected(
    int remaining_samples) {
  if (remaining_samples <= 0) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(bio_callback_));
  }
}

UVTestAuthenticatorContentBrowserClient::
    UVTestAuthenticatorContentBrowserClient() = default;
UVTestAuthenticatorContentBrowserClient::
    ~UVTestAuthenticatorContentBrowserClient() = default;

WebAuthenticationDelegate*
UVTestAuthenticatorContentBrowserClient::GetWebAuthenticationDelegate() {
  return &web_authentication_delegate;
}

std::unique_ptr<AuthenticatorRequestClientDelegate>
UVTestAuthenticatorContentBrowserClient::GetWebAuthenticationRequestDelegate(
    RenderFrameHost* render_frame_host) {
  return std::make_unique<UVTestAuthenticatorClientDelegate>(
      &collected_pin, &min_pin_length, &did_bio_enrollment,
      cancel_bio_enrollment, block_request_on_failure_once);
}

TestWebAuthenticationDelegate*
UVTestAuthenticatorContentBrowserClient::GetTestWebAuthenticationDelegate() {
  return &web_authentication_delegate;
}

UVAuthenticatorImplTest::UVAuthenticatorImplTest() = default;
UVAuthenticatorImplTest::~UVAuthenticatorImplTest() = default;

void UVAuthenticatorImplTest::SetUp() {
  AuthenticatorImplTest::SetUp();
  old_client_ = SetBrowserClientForTesting(&test_client_);
}

void UVAuthenticatorImplTest::TearDown() {
  SetBrowserClientForTesting(old_client_);
  AuthenticatorImplTest::TearDown();
}

PublicKeyCredentialCreationOptionsPtr
UVAuthenticatorImplTest::make_credential_options(
    device::UserVerificationRequirement uv,
    bool exclude_credentials,
    bool appid_exclude) {
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  if (exclude_credentials) {
    options->exclude_credentials = GetTestCredentials(/*num_credentials=*/1);
  }
  if (appid_exclude) {
    CHECK(exclude_credentials);
    options->appid_exclude = kTestOrigin1;
  }
  options->authenticator_selection->user_verification_requirement = uv;
  return options;
}

PublicKeyCredentialRequestOptionsPtr
UVAuthenticatorImplTest::get_credential_options(
    device::UserVerificationRequirement uv) {
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->user_verification = uv;
  return options;
}

MockAuthenticatorRequestDelegateObserver::
    MockAuthenticatorRequestDelegateObserver(
        InterestingFailureReasonCallback failure_reasons_callback)
    : TestAuthenticatorRequestDelegate(
          nullptr /* render_frame_host */,
          base::DoNothing() /* did_start_request_callback */,
          /*started_over_callback=*/base::OnceClosure(),
          /*simulate_user_cancelled=*/false,
          /*enclave_discovered_callback=*/base::DoNothing(),
          /*transports_discovered_callback=*/base::DoNothing()),
      failure_reasons_callback_(std::move(failure_reasons_callback)) {}

MockAuthenticatorRequestDelegateObserver::
    ~MockAuthenticatorRequestDelegateObserver() = default;

bool MockAuthenticatorRequestDelegateObserver::DoesBlockRequestOnFailure(
    InterestingFailureReason reason) {
  CHECK(failure_reasons_callback_);
  std::move(failure_reasons_callback_).Run(reason);
  return false;
}

FakeAuthenticatorCommonImpl::FakeAuthenticatorCommonImpl(
    RenderFrameHost* render_frame_host,
    std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate)
    : AuthenticatorCommonImpl(render_frame_host,
                              ServingRequestsFor::kWebContents),
      mock_delegate_(std::move(mock_delegate)) {}

FakeAuthenticatorCommonImpl::~FakeAuthenticatorCommonImpl() = default;

std::unique_ptr<AuthenticatorRequestClientDelegate>
FakeAuthenticatorCommonImpl::MaybeCreateRequestDelegate() {
  DCHECK(mock_delegate_);
  return std::move(mock_delegate_);
}

AuthenticatorImplRequestDelegateTest::AuthenticatorImplRequestDelegateTest() =
    default;
AuthenticatorImplRequestDelegateTest::~AuthenticatorImplRequestDelegateTest() =
    default;

mojo::Remote<blink::mojom::Authenticator>
AuthenticatorImplRequestDelegateTest::ConnectToFakeAuthenticator(
    std::unique_ptr<MockAuthenticatorRequestDelegateObserver> delegate) {
  mojo::Remote<blink::mojom::Authenticator> authenticator;
  AuthenticatorImpl::CreateForTesting(
      *main_rfh(), authenticator.BindNewPipeAndPassReceiver(),
      std::make_unique<FakeAuthenticatorCommonImpl>(main_rfh(),
                                                    std::move(delegate)));
  return authenticator;
}

}  // namespace content
