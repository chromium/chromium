// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/webauthn/internal_authenticator_impl.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/browser/webauth/authenticator_common.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/test/test_render_frame_host.h"
#include "crypto/sha2.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/cable/v2_authenticator.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_discovery.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/v2_test_util.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/public_key.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "url/url_util.h"

#if defined(OS_MAC)
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#endif

#if defined(OS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#endif

namespace content {

using ::testing::_;

using blink::mojom::AttestationConveyancePreference;
using blink::mojom::AuthenticatorSelectionCriteria;
using blink::mojom::AuthenticatorSelectionCriteriaPtr;
using blink::mojom::AuthenticatorStatus;
using blink::mojom::AuthenticatorTransport;
using blink::mojom::CableAuthentication;
using blink::mojom::CableAuthenticationPtr;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
using blink::mojom::MakeCredentialAuthenticatorResponsePtr;
using blink::mojom::PublicKeyCredentialCreationOptions;
using blink::mojom::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::PublicKeyCredentialDescriptor;
using blink::mojom::PublicKeyCredentialDescriptorPtr;
using blink::mojom::PublicKeyCredentialParameters;
using blink::mojom::PublicKeyCredentialParametersPtr;
using blink::mojom::PublicKeyCredentialRequestOptions;
using blink::mojom::PublicKeyCredentialRequestOptionsPtr;
using blink::mojom::PublicKeyCredentialRpEntity;
using blink::mojom::PublicKeyCredentialRpEntityPtr;
using blink::mojom::PublicKeyCredentialType;
using blink::mojom::PublicKeyCredentialUserEntity;
using blink::mojom::PublicKeyCredentialUserEntityPtr;
using cbor::Reader;
using cbor::Value;

namespace {

using InterestingFailureReason =
    ::content::AuthenticatorRequestClientDelegate::InterestingFailureReason;
using FailureReasonCallbackReceiver =
    ::device::test::TestCallbackReceiver<InterestingFailureReason>;

constexpr base::TimeDelta kTestTimeout = base::TimeDelta::FromMinutes(1);

// The size of credential IDs returned by GetTestCredentials().
constexpr size_t kTestCredentialIdLength = 32u;

constexpr char kTestOrigin1[] = "https://a.google.com";
constexpr char kTestOrigin2[] = "https://acme.org";
constexpr char kTestRelyingPartyId[] = "google.com";
constexpr char kCryptotokenOrigin[] =
    "chrome-extension://kmendfapggjehodndflmmgagdbamhnfd";
constexpr char kTestExtensionOrigin[] =
    "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef";

constexpr uint8_t kTestChallengeBytes[] = {
    0x68, 0x71, 0x34, 0x96, 0x82, 0x22, 0xEC, 0x17, 0x20, 0x2E, 0x42,
    0x50, 0x5F, 0x8E, 0xD2, 0xB1, 0x6A, 0xE2, 0x2F, 0x16, 0xBB, 0x05,
    0xB8, 0x8C, 0x25, 0xDB, 0x9E, 0x60, 0x26, 0x45, 0xF1, 0x41};

constexpr char kTestRegisterClientDataJsonString[] =
    R"({"challenge":"aHE0loIi7BcgLkJQX47SsWriLxa7BbiMJdueYCZF8UE","origin":)"
    R"("https://a.google.com", "type":"webauthn.create"})";

constexpr char kTestSignClientDataJsonString[] =
    R"({"challenge":"aHE0loIi7BcgLkJQX47SsWriLxa7BbiMJdueYCZF8UE","origin":)"
    R"("https://a.google.com", "type":"webauthn.get"})";

typedef struct {
  const char* origin;
  // Either a relying party ID or a U2F AppID.
  const char* claimed_authority;
  AuthenticatorStatus expected_status;
} OriginClaimedAuthorityPair;

constexpr OriginClaimedAuthorityPair kValidRelyingPartyTestCases[] = {
    {"http://localhost", "localhost", AuthenticatorStatus::SUCCESS},
    {"https://myawesomedomain", "myawesomedomain",
     AuthenticatorStatus::SUCCESS},
    {"https://foo.bar.google.com", "foo.bar.google.com",
     AuthenticatorStatus::SUCCESS},
    {"https://foo.bar.google.com", "bar.google.com",
     AuthenticatorStatus::SUCCESS},
    {"https://foo.bar.google.com", "google.com", AuthenticatorStatus::SUCCESS},
    {"https://earth.login.awesomecompany", "login.awesomecompany",
     AuthenticatorStatus::SUCCESS},
    {"https://google.com:1337", "google.com", AuthenticatorStatus::SUCCESS},

    // Hosts with trailing dot valid for rpIds with or without trailing dot.
    // Hosts without trailing dots only matches rpIDs without trailing dot.
    // Two trailing dots only matches rpIDs with two trailing dots.
    {"https://google.com.", "google.com", AuthenticatorStatus::SUCCESS},
    {"https://google.com.", "google.com.", AuthenticatorStatus::SUCCESS},
    {"https://google.com..", "google.com..", AuthenticatorStatus::SUCCESS},

    // Leading dots are ignored in canonicalized hosts.
    {"https://.google.com", "google.com", AuthenticatorStatus::SUCCESS},
    {"https://..google.com", "google.com", AuthenticatorStatus::SUCCESS},
    {"https://.google.com", ".google.com", AuthenticatorStatus::SUCCESS},
    {"https://..google.com", ".google.com", AuthenticatorStatus::SUCCESS},
    {"https://accounts.google.com", ".google.com",
     AuthenticatorStatus::SUCCESS},
};

constexpr OriginClaimedAuthorityPair kInvalidRelyingPartyTestCases[] = {
    {"https://google.com", "com", AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"http://google.com", "google.com", AuthenticatorStatus::INVALID_DOMAIN},
    {"http://myawesomedomain", "myawesomedomain",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://google.com", "foo.bar.google.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"http://myawesomedomain", "randomdomain",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://myawesomedomain", "randomdomain",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://notgoogle.com", "google.com)",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://not-google.com", "google.com)",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://evil.appspot.com", "appspot.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://evil.co.uk", "co.uk", AuthenticatorStatus::BAD_RELYING_PARTY_ID},

    {"https://google.com", "google.com.",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://google.com", "google.com..",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://google.com", ".google.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://google.com..", "google.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://.com", "com.", AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://.co.uk", "co.uk.", AuthenticatorStatus::BAD_RELYING_PARTY_ID},

    {"https://1.2.3", "1.2.3", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://1.2.3", "2.3", AuthenticatorStatus::INVALID_DOMAIN},

    {"https://127.0.0.1", "127.0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", "27.0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", ".0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", "0.0.1", AuthenticatorStatus::INVALID_DOMAIN},

    {"https://[::127.0.0.1]", "127.0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::127.0.0.1]", "[127.0.0.1]",
     AuthenticatorStatus::INVALID_DOMAIN},

    {"https://[::1]", "1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::1]", "1]", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::1]", "::1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::1]", "[::1]", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[1::1]", "::1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[1::1]", "::1]", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[1::1]", "[::1]", AuthenticatorStatus::INVALID_DOMAIN},

    {"http://google.com:443", "google.com",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"data:google.com", "google.com", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"data:text/html,google.com", "google.com",
     AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ws://google.com", "google.com", AuthenticatorStatus::INVALID_DOMAIN},
    {"gopher://google.com", "google.com", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ftp://google.com", "google.com", AuthenticatorStatus::INVALID_DOMAIN},
    {"file:///google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},
    // Use of webauthn from a WSS origin may be technically valid, but we
    // prohibit use on non-HTTPS origins. (At least for now.)
    {"wss://google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},

    {"data:,", "", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"https://google.com", "", AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"ws:///google.com", "", AuthenticatorStatus::INVALID_DOMAIN},
    {"wss:///google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},
    {"gopher://google.com", "", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ftp://google.com", "", AuthenticatorStatus::INVALID_DOMAIN},
    {"file:///google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},

    // This case is acceptable according to spec, but both renderer
    // and browser handling currently do not permit it.
    {"https://login.awesomecompany", "awesomecompany",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},

    // These are AppID test cases, but should also be invalid relying party
    // examples too.
    {"https://example.com", "https://com/",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://example.com", "https://com/foo",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://example.com", "https://foo.com/",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://example.com", "http://example.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"http://example.com", "https://example.com",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", "https://127.0.0.1",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://www.notgoogle.com",
     "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json#x",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json2",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://www.google.com", "https://gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://ggoogle.com", "https://www.gstatic.com/securitykey/origi",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://com", "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
};

using TestIsUvpaaCallback = device::test::ValueCallbackReceiver<bool>;
using TestMakeCredentialCallback = device::test::StatusAndValueCallbackReceiver<
    AuthenticatorStatus,
    MakeCredentialAuthenticatorResponsePtr>;
using TestGetAssertionCallback = device::test::StatusAndValueCallbackReceiver<
    AuthenticatorStatus,
    GetAssertionAuthenticatorResponsePtr>;
using TestRequestStartedCallback = device::test::TestCallbackReceiver<>;

std::vector<uint8_t> GetTestChallengeBytes() {
  return std::vector<uint8_t>(std::begin(kTestChallengeBytes),
                              std::end(kTestChallengeBytes));
}

device::PublicKeyCredentialRpEntity GetTestPublicKeyCredentialRPEntity() {
  device::PublicKeyCredentialRpEntity entity;
  entity.id = std::string(kTestRelyingPartyId);
  entity.name = "TestRP@example.com";
  return entity;
}

device::PublicKeyCredentialUserEntity GetTestPublicKeyCredentialUserEntity() {
  device::PublicKeyCredentialUserEntity entity;
  entity.display_name = "User A. Name";
  std::vector<uint8_t> id(32, 0x0A);
  entity.id = id;
  entity.name = "username@example.com";
  entity.icon_url = GURL("https://gstatic.com/fakeurl2.png");
  return entity;
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

device::AuthenticatorSelectionCriteria GetTestAuthenticatorSelectionCriteria() {
  return device::AuthenticatorSelectionCriteria(
      device::AuthenticatorAttachment::kAny,
      device::ResidentKeyRequirement::kDiscouraged,
      device::UserVerificationRequirement::kPreferred);
}

std::vector<device::PublicKeyCredentialDescriptor> GetTestCredentials(
    size_t num_credentials = 1) {
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

PublicKeyCredentialCreationOptionsPtr
GetTestPublicKeyCredentialCreationOptions() {
  auto options = PublicKeyCredentialCreationOptions::New();
  options->relying_party = GetTestPublicKeyCredentialRPEntity();
  options->user = GetTestPublicKeyCredentialUserEntity();
  options->public_key_parameters = GetTestPublicKeyCredentialParameters(
      static_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256));
  options->challenge.assign(32, 0x0A);
  options->timeout = base::TimeDelta::FromMinutes(1);
  options->authenticator_selection = GetTestAuthenticatorSelectionCriteria();
  return options;
}

PublicKeyCredentialRequestOptionsPtr
GetTestPublicKeyCredentialRequestOptions() {
  auto options = PublicKeyCredentialRequestOptions::New();
  options->relying_party_id = std::string(kTestRelyingPartyId);
  options->challenge.assign(32, 0x0A);
  options->timeout = base::TimeDelta::FromMinutes(1);
  options->user_verification = device::UserVerificationRequirement::kPreferred;
  options->allow_credentials = GetTestCredentials();
  return options;
}

std::vector<device::CableDiscoveryData> GetTestCableExtension() {
  device::CableDiscoveryData cable;
  cable.version = device::CableDiscoveryData::Version::V1;
  cable.v1.emplace();
  cable.v1->client_eid.fill(0x01);
  cable.v1->authenticator_eid.fill(0x02);
  cable.v1->session_pre_key.fill(0x03);

  std::vector<device::CableDiscoveryData> ret;
  ret.emplace_back(std::move(cable));
  return ret;
}

device::AuthenticatorData AuthDataFromMakeCredentialResponse(
    const MakeCredentialAuthenticatorResponsePtr& response) {
  base::Optional<Value> attestation_value =
      Reader::Read(response->attestation_object);
  CHECK(attestation_value);
  const auto& attestation = attestation_value->GetMap();

  const auto auth_data_it = attestation.find(Value(device::kAuthDataKey));
  CHECK(auth_data_it != attestation.end());
  const std::vector<uint8_t>& auth_data = auth_data_it->second.GetBytestring();
  base::Optional<device::AuthenticatorData> parsed_auth_data =
      device::AuthenticatorData::DecodeAuthenticatorData(auth_data);
  return std::move(parsed_auth_data.value());
}

url::Origin GetTestOrigin() {
  const GURL test_relying_party_url(kTestOrigin1);
  CHECK(test_relying_party_url.is_valid());
  return url::Origin::Create(test_relying_party_url);
}

std::string GetTestClientDataJSON(std::string type) {
  return device::SerializeCollectedClientDataToJson(
      std::move(type), GetTestOrigin().Serialize(), GetTestChallengeBytes(),
      /*is_cross_origin=*/false);
}

}  // namespace

class AuthenticatorTestBase : public content::RenderViewHostTestHarness {
 protected:
  AuthenticatorTestBase()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AuthenticatorTestBase() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    ResetVirtualDevice();
  }

  void ResetVirtualDevice() {
    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory_ = virtual_device_factory.get();
    AuthenticatorEnvironmentImpl::GetInstance()
        ->ReplaceDefaultDiscoveryFactoryForTesting(
            std::move(virtual_device_factory));
  }

  device::test::VirtualFidoDeviceFactory* virtual_device_factory_;
};

class AuthenticatorImplTest : public AuthenticatorTestBase {
 protected:
  AuthenticatorImplTest() {
    url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  }
  ~AuthenticatorImplTest() override = default;

  void SetUp() override {
    AuthenticatorTestBase::SetUp();
    bluetooth_global_values_->SetLESupported(true);
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  void TearDown() override {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    authenticator_impl_.reset();
    AuthenticatorTestBase::TearDown();
  }

  void NavigateAndCommit(const GURL& url) {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    authenticator_impl_.reset();
    content::RenderViewHostTestHarness::NavigateAndCommit(url);
  }

  mojo::Remote<blink::mojom::Authenticator> ConnectToAuthenticator() {
    authenticator_impl_ = std::make_unique<AuthenticatorImpl>(main_rfh());
    mojo::Remote<blink::mojom::Authenticator> authenticator;
    authenticator_impl_->Bind(authenticator.BindNewPipeAndPassReceiver());
    return authenticator;
  }

  struct MakeCredentialResult {
    AuthenticatorStatus status;
    MakeCredentialAuthenticatorResponsePtr response;
  };

  MakeCredentialResult AuthenticatorMakeCredential() {
    return AuthenticatorMakeCredential(
        GetTestPublicKeyCredentialCreationOptions());
  }

  MakeCredentialResult AuthenticatorMakeCredential(
      PublicKeyCredentialCreationOptionsPtr options) {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    AuthenticatorStatus status;
    MakeCredentialAuthenticatorResponsePtr response;
    std::tie(status, response) = callback_receiver.TakeResult();
    return {status, std::move(response)};
  }

  MakeCredentialResult AuthenticatorMakeCredentialAndWaitForTimeout(
      PublicKeyCredentialCreationOptionsPtr options) {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    task_environment()->FastForwardBy(kTestTimeout);
    callback_receiver.WaitForCallback();
    AuthenticatorStatus status;
    MakeCredentialAuthenticatorResponsePtr response;
    std::tie(status, response) = callback_receiver.TakeResult();
    return {status, std::move(response)};
  }

  struct GetAssertionResult {
    AuthenticatorStatus status;
    GetAssertionAuthenticatorResponsePtr response;
  };

  GetAssertionResult AuthenticatorGetAssertion() {
    return AuthenticatorGetAssertion(
        GetTestPublicKeyCredentialRequestOptions());
  }

  GetAssertionResult AuthenticatorGetAssertion(
      PublicKeyCredentialRequestOptionsPtr options) {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    AuthenticatorStatus status;
    GetAssertionAuthenticatorResponsePtr response;
    std::tie(status, response) = callback_receiver.TakeResult();
    return {status, std::move(response)};
  }

  GetAssertionResult AuthenticatorGetAssertionAndWaitForTimeout(
      PublicKeyCredentialRequestOptionsPtr options) {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    task_environment()->FastForwardBy(kTestTimeout);
    AuthenticatorStatus status;
    GetAssertionAuthenticatorResponsePtr response;
    std::tie(status, response) = callback_receiver.TakeResult();
    return {status, std::move(response)};
  }

  AuthenticatorStatus TryAuthenticationWithAppId(const std::string& origin,
                                                 const std::string& appid) {
    const GURL origin_url(origin);
    NavigateAndCommit(origin_url);

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = origin_url.host();
    options->appid = appid;

    return AuthenticatorGetAssertion(std::move(options)).status;
  }

  AuthenticatorStatus TryRegistrationWithAppIdExclude(
      const std::string& origin,
      const std::string& appid_exclude) {
    const GURL origin_url(origin);
    NavigateAndCommit(origin_url);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = origin_url.host();
    options->appid_exclude = appid_exclude;

    return AuthenticatorMakeCredential(std::move(options)).status;
  }

  void EnableFeature(const base::Feature& feature) {
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitAndEnableFeature(feature);
  }

  void DisableFeature(const base::Feature& feature) {
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitAndDisableFeature(feature);
  }

 protected:
  std::unique_ptr<AuthenticatorImpl> authenticator_impl_;
  base::Optional<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalValuesForTesting>
      bluetooth_global_values_ =
          device::BluetoothAdapterFactory::Get()->InitGlobalValuesForTesting();
  scoped_refptr<::testing::NiceMock<device::MockBluetoothAdapter>>
      mock_adapter_ = base::MakeRefCounted<
          ::testing::NiceMock<device::MockBluetoothAdapter>>();

 private:
  url::ScopedSchemeRegistryForTests scoped_registry_;
};

TEST_F(AuthenticatorImplTest, ClientDataJSONSerialization) {
  // First test that the output is in the expected form. Some verifiers may be
  // depending on the exact JSON serialisation. Since the serialisation may add
  // extra elements, this can only test that the expected value is a prefix of
  // the returned value.
  std::vector<uint8_t> challenge_bytes = {1, 2, 3};
  EXPECT_TRUE(
      device::SerializeCollectedClientDataToJson("t\x05ype", "ori\"gin",
                                                 challenge_bytes, false)
          .find("{\"type\":\"t\\u0005ype\",\"challenge\":\"AQID\",\"origin\":"
                "\"ori\\\"gin\",\"crossOrigin\":false") == 0);

  // Second, check that a generic JSON parser correctly parses the result.
  static const struct {
    const char* type;
    const char* origin;
    std::vector<uint8_t> challenge;
    bool is_cross_origin;
  } kTestCases[] = {
      {
          "type",
          "origin",
          {1, 2, 3},
          false,
      },
      {
          "t\x01y\x02pe",
          "ori\"gin",
          {1, 2, 3, 4},
          true,
      },
      {
          "\\\\\"\\",
          "\x01\x02\x03\x04{}\x05c",
          {1, 2, 3, 4, 5},
          true,
      },
  };

  size_t num = 0;
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(num++);

    const std::string json = device::SerializeCollectedClientDataToJson(
        test.type, test.origin, test.challenge, test.is_cross_origin);

    const auto parsed = base::JSONReader::Read(json);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed->FindStringKey("type"), test.type);
    EXPECT_EQ(*parsed->FindStringKey("origin"), test.origin);
    std::string expected_challenge;
    base::Base64UrlEncode(
        base::StringPiece(reinterpret_cast<const char*>(test.challenge.data()),
                          test.challenge.size()),
        base::Base64UrlEncodePolicy::OMIT_PADDING, &expected_challenge);
    EXPECT_EQ(*parsed->FindStringKey("challenge"), expected_challenge);
    EXPECT_EQ(*parsed->FindBoolKey("crossOrigin"), test.is_cross_origin);
  }
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, MakeCredentialOriginAndRpIds) {
  std::vector<OriginClaimedAuthorityPair> tests(
      &kValidRelyingPartyTestCases[0],
      &kValidRelyingPartyTestCases[base::size(kValidRelyingPartyTestCases)]);
  tests.insert(tests.end(), &kInvalidRelyingPartyTestCases[0],
               &kInvalidRelyingPartyTestCases[base::size(
                   kInvalidRelyingPartyTestCases)]);

  for (const auto& test_case : tests) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              test_case.expected_status);
  }
}

// Test that MakeCredential returns INVALID_ICON_URL in the correct cases.
TEST_F(AuthenticatorImplTest, MakeCredentialURLs) {
  constexpr auto ok = AuthenticatorStatus::SUCCESS;
  constexpr auto bad = AuthenticatorStatus::INVALID_ICON_URL;
  const std::pair<GURL, AuthenticatorStatus> kTestCases[] = {
      {GURL(), ok},
      {GURL("https://secure-origin.com/kitten.png"), ok},
      {GURL("about:blank"), ok},
      {GURL("about:srcdoc"), ok},
      {GURL(
           "data:image/"
           "png;base64,"
           "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAACXBIWXMAAC4jAAAuIwF"
           "4pT92AAAAB3RJTUUH4wYUETEs5V5U8gAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdG"
           "ggR0lNUFeBDhcAAABGSURBVCjPY/z//"
           "z8DKYAJmcPYyICHi0UDyTYMDg2MFIUSnsAZAp5mbGT4X49DBcxLEAUsBMxrRCiFABb8"
           "gYNpLTXiAT8AAEeHFZvhj9g8AAAAAElFTkSuQmCC"),
       ok},

      {GURL("http://insecure-origin.com/kitten.png"), bad},
      {GURL("invalid:/url"), bad},
  };

  NavigateAndCommit(GURL(kTestOrigin1));

  for (const bool test_user_icon : {false, true}) {
    for (auto test_case : kTestCases) {
      SCOPED_TRACE(test_case.first.possibly_invalid_spec());
      SCOPED_TRACE(test_user_icon);

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      if (test_user_icon) {
        options->user.icon_url = test_case.first;
      } else {
        options->relying_party.icon_url = test_case.first;
      }

      EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
                test_case.second);
    }
  }
}

// Test that MakeCredential request times out with NOT_ALLOWED_ERROR if user
// verification is required for U2F devices.
TEST_F(AuthenticatorImplTest, MakeCredentialUserVerification) {
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->SetUserVerificationRequirementForTesting(
      device::UserVerificationRequirement::kRequired);

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, MakeCredentialResidentKeyUnsupported) {
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->SetResidentKeyForTesting(
      device::ResidentKeyRequirement::kRequired);

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
}

// Test that MakeCredential request times out with NOT_ALLOWED_ERROR if a
// platform authenticator is requested for U2F devices.
TEST_F(AuthenticatorImplTest, MakeCredentialPlatformAuthenticator) {
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->SetAuthenticatorAttachmentForTesting(
      device::AuthenticatorAttachment::kPlatform);

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// Parses its arguments as JSON and expects that all the keys in the first are
// also in the second, and with the same value.
void CheckJSONIsSubsetOfJSON(base::StringPiece subset_str,
                             base::StringPiece test_str) {
  std::unique_ptr<base::Value> subset(
      base::JSONReader::ReadDeprecated(subset_str));
  ASSERT_TRUE(subset);
  ASSERT_TRUE(subset->is_dict());
  std::unique_ptr<base::Value> test(base::JSONReader::ReadDeprecated(test_str));
  ASSERT_TRUE(test);
  ASSERT_TRUE(test->is_dict());

  for (const auto& item : subset->DictItems()) {
    base::Value* test_value = test->FindKey(item.first);
    if (test_value == nullptr) {
      ADD_FAILURE() << item.first << " does not exist in the test dictionary";
      continue;
    }

    if (!item.second.Equals(test_value)) {
      std::string want, got;
      ASSERT_TRUE(base::JSONWriter::Write(item.second, &want));
      ASSERT_TRUE(base::JSONWriter::Write(*test_value, &got));
      ADD_FAILURE() << "Value of " << item.first << " is unequal: want " << want
                    << " got " << got;
    }
  }
}

// Test that client data serializes to JSON properly.
TEST(ClientDataSerializationTest, Register) {
  CheckJSONIsSubsetOfJSON(kTestRegisterClientDataJsonString,
                          GetTestClientDataJSON(client_data::kCreateType));
}

TEST(ClientDataSerializationTest, Sign) {
  CheckJSONIsSubsetOfJSON(kTestSignClientDataJsonString,
                          GetTestClientDataJSON(client_data::kGetType));
}

TEST_F(AuthenticatorImplTest, TestMakeCredentialTimeout) {
  // The VirtualFidoAuthenticator simulates a tap immediately after it gets the
  // request. Replace by the real discovery that will wait until timeout.
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<device::FidoDiscoveryFactory>());
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, GetAssertionOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (const OriginClaimedAuthorityPair& test_case :
       kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              test_case.expected_status);
  }
}

constexpr OriginClaimedAuthorityPair kValidAppIdCases[] = {
    {"https://example.com", "https://example.com",
     AuthenticatorStatus::SUCCESS},
    {"https://www.example.com", "https://example.com",
     AuthenticatorStatus::SUCCESS},
    {"https://example.com", "https://www.example.com",
     AuthenticatorStatus::SUCCESS},
    {"https://example.com", "https://foo.bar.example.com",
     AuthenticatorStatus::SUCCESS},
    {"https://example.com", "https://foo.bar.example.com/foo/bar",
     AuthenticatorStatus::SUCCESS},
    {"https://google.com", "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::SUCCESS},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::SUCCESS},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/a/google.com/origins.json",
     AuthenticatorStatus::SUCCESS},
    {"https://accounts.google.com",
     "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::SUCCESS},
};

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, AppIdExtensionValues) {
  for (const auto& test_case : kValidAppIdCases) {
    SCOPED_TRACE(std::string(test_case.origin) + " " +
                 std::string(test_case.claimed_authority));

    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              TryAuthenticationWithAppId(test_case.origin,
                                         test_case.claimed_authority));

    EXPECT_EQ(AuthenticatorStatus::SUCCESS,
              TryRegistrationWithAppIdExclude(test_case.origin,
                                              test_case.claimed_authority));
  }

  // All the invalid relying party test cases should also be invalid as AppIDs.
  for (const auto& test_case : kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.origin) + " " +
                 std::string(test_case.claimed_authority));

    if (strlen(test_case.claimed_authority) == 0) {
      // In this case, no AppID is actually being tested.
      continue;
    }

    AuthenticatorStatus test_status = TryAuthenticationWithAppId(
        test_case.origin, test_case.claimed_authority);
    EXPECT_TRUE(test_status == AuthenticatorStatus::INVALID_DOMAIN ||
                test_status == test_case.expected_status);

    test_status = TryRegistrationWithAppIdExclude(test_case.origin,
                                                  test_case.claimed_authority);
    EXPECT_TRUE(test_status == AuthenticatorStatus::INVALID_DOMAIN ||
                test_status == test_case.expected_status);
  }
}

// Verify that a request coming from Cryptotoken bypasses origin checks.
TEST_F(AuthenticatorImplTest, CryptotokenBypass) {
  {
    NavigateAndCommit(GURL(kCryptotokenOrigin));
    // First, verify that the Cryptotoken request succeeds with the appid.
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = std::string(kTestOrigin1);

    // Inject a registration for the URL (which is a U2F AppID).
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestOrigin1));

    options->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    EXPECT_EQ(result.response->echo_appid_extension, true);
    EXPECT_EQ(result.response->appid_extension, true);
  }

  {
    ResetVirtualDevice();
    NavigateAndCommit(GURL(kTestExtensionOrigin));
    // Next, verify that other extensions cannot bypass the origin checks.
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = std::string(kTestOrigin1);

    // Inject a registration for the URL (which is a U2F AppID).
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestOrigin1));

    options->appid = kTestOrigin1;

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::INVALID_DOMAIN);
  }
}

// MakeCredential requests from cryptotoken to a U2F authenticator should
// succeed.
TEST_F(AuthenticatorImplTest, CryptoTokenMakeCredentialU2fDevice) {
  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);

  NavigateAndCommit(GURL(kCryptotokenOrigin));
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = kTestOrigin1;

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

// MakeCredential requests from cryptotoken to an authentictor that does not
// support U2F should fail.
TEST_F(AuthenticatorImplTest, CryptoTokenMakeCredentialCtap2Device) {
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  NavigateAndCommit(GURL(kCryptotokenOrigin));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = kTestOrigin1;

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// GetAssertion requests from cryptotoken to a U2F/CTAP authenticator should
// use the U2F interface.
TEST_F(AuthenticatorImplTest, CryptoTokenMakeCredentialDualProtocolDevice) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  virtual_device_factory_->SetCtap2Config(config);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = kTestOrigin1;

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  ASSERT_EQ(virtual_device_factory_->mutable_state()->registrations.size(), 1u);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()
                  ->registrations.begin()
                  ->second.is_u2f);
}

// GetAssertion requests from cryptotoken to a U2F authenticator should
// succeed.
TEST_F(AuthenticatorImplTest, CryptoTokenGetAssertionU2fDevice) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestOrigin1));
  options->appid = kTestOrigin1;

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

// GetAssertion requests from cryptotoken to an authentictor that does not
// support U2F should fail.
TEST_F(AuthenticatorImplTest, CryptoTokenGetAssertionCtap2Device) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestOrigin1));
  options->appid = kTestOrigin1;

  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// GetAssertion requests from cryptotoken should challenge credential on a
// U2F/CTAP authenticator via the U2F interface.
TEST_F(AuthenticatorImplTest, CryptoTokenGetAssertionDualProtocolDevice) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.ignore_u2f_credentials = true;
  virtual_device_factory_->SetCtap2Config(config);

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestOrigin1));
  options->appid = kTestOrigin1;

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

// Test that Cryptotoken requests should only be dispatched to USB
// authenticators.
TEST_F(AuthenticatorImplTest, CryptotokenUsbOnly) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));

  // caBLE and platform discoveries cannot be instantiated through
  // VirtualFidoDeviceFactory, so we don't test them here.
  for (const device::FidoTransportProtocol transport :
       {device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
        device::FidoTransportProtocol::kBluetoothLowEnergy,
        device::FidoTransportProtocol::kNearFieldCommunication}) {
    SCOPED_TRACE(::testing::Message()
                 << "transport=" << device::ToString(transport));

    ResetVirtualDevice();
    virtual_device_factory_->SetSupportedProtocol(
        device::ProtocolVersion::kU2f);
    virtual_device_factory_->SetTransport(transport);
    virtual_device_factory_->mutable_state()->transport = transport;

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();

    if (transport == device::FidoTransportProtocol::kUsbHumanInterfaceDevice) {
      EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
                AuthenticatorStatus::SUCCESS);
    } else {
      EXPECT_EQ(AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options))
                    .status,
                AuthenticatorStatus::NOT_ALLOWED_ERROR);
    }
  }
}

// Verify that a credential registered with U2F can be used via webauthn.
TEST_F(AuthenticatorImplTest, AppIdExtension) {
  NavigateAndCommit(GURL(kTestOrigin1));

  {
    // First, test that the appid extension isn't echoed at all when not
    // requested.
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestRelyingPartyId));

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->echo_appid_extension, false);
  }

  {
    // Second, test that the appid extension is echoed, but is false, when appid
    // is requested but not used.
    ResetVirtualDevice();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestRelyingPartyId));

    // This AppID won't be used because the RP ID will be tried (successfully)
    // first.
    options->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->echo_appid_extension, true);
    EXPECT_EQ(result.response->appid_extension, false);
  }

  {
    // Lastly, when used, the appid extension result should be "true".
    ResetVirtualDevice();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    // Inject a registration for the URL (which is a U2F AppID).
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestOrigin1));

    options->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->echo_appid_extension, true);
    EXPECT_EQ(result.response->appid_extension, true);
  }

  {
    // AppID should still work when the authenticator supports credProtect.
    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.u2f_support = true;
    config.pin_support = true;
    config.resident_key_support = true;
    config.cred_protect_support = true;

    virtual_device_factory_->SetCtap2Config(config);

    // Inject a registration for the URL (which is a U2F AppID).
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestOrigin1));

    options->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->echo_appid_extension, true);
    EXPECT_EQ(result.response->appid_extension, true);
  }
}

TEST_F(AuthenticatorImplTest, AppIdExcludeExtension) {
  NavigateAndCommit(GURL(kTestOrigin1));

  // Attempt to register a credential using the appidExclude extension. It
  // should fail when the registration already exists on the authenticator.
  for (bool credential_already_exists : {false, true}) {
    SCOPED_TRACE(credential_already_exists);

    for (bool is_ctap2 : {false, true}) {
      SCOPED_TRACE(is_ctap2);

      ResetVirtualDevice();
      virtual_device_factory_->SetSupportedProtocol(
          is_ctap2 ? device::ProtocolVersion::kCtap2
                   : device::ProtocolVersion::kU2f);

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->appid_exclude = kTestOrigin1;
      options->exclude_credentials = GetTestCredentials();

      if (credential_already_exists) {
        ASSERT_TRUE(
            virtual_device_factory_->mutable_state()->InjectRegistration(
                options->exclude_credentials[0].id(), kTestOrigin1));
      }

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));

      if (credential_already_exists) {
        ASSERT_EQ(result.status, AuthenticatorStatus::CREDENTIAL_EXCLUDED);
      } else {
        ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
      }
    }
  }

  {
    // Using appidExclude with an empty exclude list previously caused a crash.
    // See https://bugs.chromium.org/p/chromium/issues/detail?id=1054499.
    virtual_device_factory_->SetSupportedProtocol(
        device::ProtocolVersion::kCtap2);
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->appid_exclude = kTestOrigin1;
    options->exclude_credentials.clear();
    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
  }

  {
    // Also test the case where all credential IDs are eliminated because of
    // their size.
    device::VirtualCtap2Device::Config config;
    config.max_credential_count_in_list = 1;
    config.max_credential_id_length = 1;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->appid_exclude = kTestOrigin1;
    options->exclude_credentials = GetTestCredentials();

    for (const auto& cred : options->exclude_credentials) {
      ASSERT_GT(cred.id().size(), config.max_credential_id_length);
    }

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
  }
}

TEST_F(AuthenticatorImplTest, TestGetAssertionTimeout) {
  // The VirtualFidoAuthenticator simulates a tap immediately after it gets the
  // request. Replace by the real discovery that will wait until timeout.
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<device::FidoDiscoveryFactory>());
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();

  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, OversizedCredentialId) {
  // 255 is the maximum size of a U2F credential ID. We also test one greater
  // (256) to ensure that nothing untoward happens.
  const std::vector<size_t> kSizes = {255, 256};

  for (const size_t size : kSizes) {
    SCOPED_TRACE(size);

    NavigateAndCommit(GURL(kTestOrigin1));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    device::PublicKeyCredentialDescriptor credential;
    credential.SetCredentialTypeForTesting(device::CredentialType::kPublicKey);
    credential.GetIdForTesting().resize(size);
    credential.GetTransportsForTesting().emplace(
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice);

    const bool should_be_valid = size < 256;
    if (should_be_valid) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          credential.id(), kTestRelyingPartyId));
    }

    options->allow_credentials.emplace_back(credential);

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              should_be_valid ? AuthenticatorStatus::SUCCESS
                              : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, NoSilentAuthenticationForCable) {
  // https://crbug.com/954355
  EnableFeature(features::kWebAuthCable);

  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool is_cable_device : {false, true}) {
    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.reject_silent_authentication_requests = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = GetTestCredentials(/*num_credentials=*/2);
    options->cable_authentication_data = GetTestCableExtension();

    if (is_cable_device) {
      virtual_device_factory_->SetTransport(
          device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
      for (auto& cred : options->allow_credentials) {
        cred.GetTransportsForTesting().clear();
        cred.GetTransportsForTesting().emplace(
            device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
      }
    }

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestRelyingPartyId));

    // If a caBLE device is not simulated then silent requests should be used.
    // The virtual device will return an error because
    // |reject_silent_authentication_requests| is true and then it'll
    // immediately resolve the touch request.
    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              is_cable_device ? AuthenticatorStatus::SUCCESS
                              : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, TestGetAssertionU2fDeviceBackwardsCompatibility) {
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  // Inject credential ID to the virtual device so that successful sign in is
  // possible.
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorImplTest, GetAssertionWithEmptyAllowCredentials) {
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials.clear();

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
}

TEST_F(AuthenticatorImplTest, MakeCredentialAlreadyRegistered) {
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  // Exclude the one already registered credential.
  options->exclude_credentials = GetTestCredentials();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->exclude_credentials[0].id(), kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::CREDENTIAL_EXCLUDED);
}

TEST_F(AuthenticatorImplTest, MakeCredentialPendingRequest) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // Make first request.
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());

  // Make second request.
  // TODO(crbug.com/785955): Rework to ensure there are potential race
  // conditions once we have VirtualAuthenticatorEnvironment.
  PublicKeyCredentialCreationOptionsPtr options2 =
      GetTestPublicKeyCredentialCreationOptions();
  TestMakeCredentialCallback callback_receiver2;
  authenticator->MakeCredential(std::move(options2),
                                callback_receiver2.callback());
  callback_receiver2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver2.status());

  callback_receiver.WaitForCallback();
}

TEST_F(AuthenticatorImplTest, GetAssertionPendingRequest) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // Make first request.
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  // Make second request.
  // TODO(crbug.com/785955): Rework to ensure there are potential race
  // conditions once we have VirtualAuthenticatorEnvironment.
  PublicKeyCredentialRequestOptionsPtr options2 =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver2;
  authenticator->GetAssertion(std::move(options2),
                              callback_receiver2.callback());
  callback_receiver2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver2.status());

  callback_receiver.WaitForCallback();
}

TEST_F(AuthenticatorImplTest, NavigationDuringOperation) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  base::RunLoop run_loop;
  authenticator.set_disconnect_handler(run_loop.QuitClosure());

  // Make first request.
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  // Simulate a navigation while waiting for the user to press the token.
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindLambdaForTesting(
                           [&]() { NavigateAndCommit(GURL(kTestOrigin2)); }));
        return false;
      });

  run_loop.Run();
}

TEST_F(AuthenticatorImplTest, InvalidResponse) {
  virtual_device_factory_->mutable_state()->simulate_invalid_response = true;
  NavigateAndCommit(GURL(kTestOrigin1));

  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    EXPECT_EQ(
        AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
        AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }

  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    EXPECT_EQ(
        AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
        AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, Ctap2AssertionWithUnknownCredential) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool return_immediate_invalid_credential_error : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "return_immediate_invalid_credential_error="
                 << return_immediate_invalid_credential_error);

    device::VirtualCtap2Device::Config config;
    config.return_immediate_invalid_credential_error =
        return_immediate_invalid_credential_error;
    virtual_device_factory_->SetCtap2Config(config);

    bool pressed = false;
    virtual_device_factory_->mutable_state()->simulate_press_callback =
        base::BindRepeating(
            [](bool* flag, device::VirtualFidoDevice* device) {
              *flag = true;
              return true;
            },
            &pressed);

    EXPECT_EQ(
        AuthenticatorGetAssertion(GetTestPublicKeyCredentialRequestOptions())
            .status,
        AuthenticatorStatus::NOT_ALLOWED_ERROR);
    // The user must have pressed the authenticator for the operation to
    // resolve.
    EXPECT_TRUE(pressed);
  }
}

TEST_F(AuthenticatorImplTest, GetAssertionResponseWithAttestedCredentialData) {
  device::VirtualCtap2Device::Config config;
  config.return_attested_cred_data_in_get_assertion_response = true;
  virtual_device_factory_->SetCtap2Config(config);
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  NavigateAndCommit(GURL(kTestOrigin1));

  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

#if defined(OS_WIN)
TEST_F(AuthenticatorImplTest, IsUVPAA) {
  device::FakeWinWebAuthnApi win_webauthn_api;
  auto discovery_factory =
      std::make_unique<device::test::FakeFidoDiscoveryFactory>();
  discovery_factory->set_win_webauthn_api(&win_webauthn_api);
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(std::move(discovery_factory));

  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  for (const bool enable_win_webauthn_api : {false, true}) {
    SCOPED_TRACE(enable_win_webauthn_api ? "enable_win_webauthn_api"
                                         : "!enable_win_webauthn_api");
    for (const bool is_uvpaa : {false, true}) {
      SCOPED_TRACE(is_uvpaa ? "is_uvpaa" : "!is_uvpaa");

      win_webauthn_api.set_available(enable_win_webauthn_api);
      win_webauthn_api.set_is_uvpaa(is_uvpaa);

      TestIsUvpaaCallback cb;
      authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(
          cb.callback());
      cb.WaitForCallback();
      EXPECT_EQ(enable_win_webauthn_api && is_uvpaa, cb.value());
    }
  }
}
#endif  // defined(OS_WIN)

#if defined(OS_CHROMEOS)
TEST_F(AuthenticatorImplTest, IsUVPAA) {
  NavigateAndCommit(GURL(kTestOrigin1));
  for (const bool flag_enabled : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "flag_enabled=" << flag_enabled);
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        device::kWebAuthCrosPlatformAuthenticator, flag_enabled);
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestIsUvpaaCallback cb;
    authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(cb.callback());
    cb.WaitForCallback();
    EXPECT_EQ(flag_enabled, cb.value());
  }
}
#endif  // defined(OS_CHROMEOS)

class OverrideRPIDAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  OverrideRPIDAuthenticatorRequestDelegate() = default;
  ~OverrideRPIDAuthenticatorRequestDelegate() override = default;

  base::Optional<std::string> MaybeGetRelyingPartyIdOverride(
      const std::string& claimed_rp_id,
      const url::Origin& caller_origin) override {
    CHECK_EQ(caller_origin.scheme(), "chrome-extension");
    return caller_origin.Serialize();
  }

  bool SupportsResidentKeys() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverrideRPIDAuthenticatorRequestDelegate);
};

class OverrideRPIDAuthenticatorContentBrowserClient
    : public ContentBrowserClient {
 public:
  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<OverrideRPIDAuthenticatorRequestDelegate>();
  }
};

static constexpr char kExtensionId[] = "abcdefg";

class ExtensionAuthenticatorTest : public AuthenticatorImplTest {
 public:
  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);

    const std::string extension_origin =
        std::string("chrome-extension://") + kExtensionId;
    const std::string extension_page = extension_origin + "/test.html";
    NavigateAndCommit(GURL(extension_page));
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

 private:
  OverrideRPIDAuthenticatorContentBrowserClient test_client_;
  ContentBrowserClient* old_client_ = nullptr;
};

// Test that credentials can be created and used from an extension origin when
// permitted by the delegate.
TEST_F(ExtensionAuthenticatorTest, ChromeExtensions) {
  std::vector<uint8_t> credential_id;
  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = kExtensionId;

    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    credential_id = result.response->info->raw_id;
  }

  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = kExtensionId;
    options->allow_credentials[0] = device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, std::move(credential_id));

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
  }
}

// Tests that registering a resident credential on a capable authenticator also
// registers a large blob key when called from an extension.
TEST_F(ExtensionAuthenticatorTest, MakeCredentialLargeBlobKeyExtension) {
  base::Optional<device::PublicKeyCredentialDescriptor> credential;
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  config.resident_key_support = true;

  for (bool rk_enabled : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "rk=" << rk_enabled);
    for (bool large_blob_supported : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "largeBlob=" << large_blob_supported);
      config.large_blob_support = large_blob_supported;
      virtual_device_factory_->SetCtap2Config(config);
      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      if (rk_enabled) {
        options->authenticator_selection->SetResidentKeyForTesting(
            device::ResidentKeyRequirement::kRequired);
      }
      options->user.id = {1, 2, 3, 4};
      options->user.name = "name";
      options->user.display_name = "displayName";

      MakeCredentialResult make_credential_result =
          AuthenticatorMakeCredential(std::move(options));
      EXPECT_EQ(make_credential_result.status, AuthenticatorStatus::SUCCESS);

      auto& registration =
          *virtual_device_factory_->mutable_state()->registrations.begin();
      EXPECT_EQ(rk_enabled && large_blob_supported,
                registration.second.large_blob_key.has_value());
      virtual_device_factory_->mutable_state()->registrations.clear();
    }
  }
}

enum class EnterprisePolicy {
  LISTED,
  NOT_LISTED,
};

enum class AttestationConsent {
  GRANTED,
  DENIED,
  GRANTED_FOR_ENTERPRISE_ATTESTATION,
  DENIED_FOR_ENTERPRISE_ATTESTATION,
  NOT_USED,
};

enum class AttestationType {
  ANY,
  NONE,
  NONE_WITH_NONZERO_AAGUID,
  U2F,
  SELF,
  SELF_WITH_NONZERO_AAGUID,
  PACKED,
};

// Convert a blink::mojom::AttestationConveyancePreference to a
// device::AtttestationConveyancePreference.
device::AttestationConveyancePreference ConvertAttestationConveyancePreference(
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

class TestAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  TestAuthenticatorRequestDelegate(
      RenderFrameHost* render_frame_host,
      base::OnceClosure action_callbacks_registered_callback,
      EnterprisePolicy enterprise_policy,
      AttestationConsent attestation_consent,
      bool is_focused,
      bool is_uvpaa)
      : action_callbacks_registered_callback_(
            std::move(action_callbacks_registered_callback)),
        enterprise_policy_(enterprise_policy),
        attestation_consent_(attestation_consent),
        is_focused_(is_focused),
        is_uvpaa_(is_uvpaa) {}

  ~TestAuthenticatorRequestDelegate() override {
    CHECK(attestation_consent_queried_ ||
          attestation_consent_ == AttestationConsent::NOT_USED);
  }

  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback) override {
    ASSERT_TRUE(action_callbacks_registered_callback_)
        << "RegisterActionCallbacks called twice.";
    cancel_callback_.emplace(std::move(cancel_callback));
    std::move(action_callbacks_registered_callback_).Run();
  }

  bool ShouldPermitIndividualAttestation(
      const std::string& relying_party_id) override {
    return enterprise_policy_ == EnterprisePolicy::LISTED;
  }

  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      const device::FidoAuthenticator* authenticator,
      bool is_enterprise_attestation,
      base::OnceCallback<void(bool)> callback) override {
    bool result = false;
    switch (attestation_consent_) {
      case AttestationConsent::NOT_USED:
        CHECK(false);
        break;
      case AttestationConsent::DENIED:
        CHECK(!is_enterprise_attestation);
        break;
      case AttestationConsent::GRANTED:
        CHECK(!is_enterprise_attestation);
        result = true;
        break;
      case AttestationConsent::DENIED_FOR_ENTERPRISE_ATTESTATION:
        CHECK(is_enterprise_attestation);
        break;
      case AttestationConsent::GRANTED_FOR_ENTERPRISE_ATTESTATION:
        CHECK(is_enterprise_attestation);
        result = true;
        break;
    }

    attestation_consent_queried_ = true;
    std::move(callback).Run(result);
  }

  base::Optional<bool> IsUserVerifyingPlatformAuthenticatorAvailableOverride()
      override {
    return is_uvpaa_;
  }

  bool IsFocused() override { return is_focused_; }

  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo transport_info)
      override {
    // Simulate the behaviour of Chrome's |AuthenticatorRequestDialogModel|
    // which shows a specific error when no transports are available and lets
    // the user cancel the request.
    if (transport_info.available_transports.empty()) {
      std::move(*cancel_callback_).Run();
    }
  }

  base::OnceClosure action_callbacks_registered_callback_;
  base::Optional<base::OnceClosure> cancel_callback_;
  const EnterprisePolicy enterprise_policy_;
  const AttestationConsent attestation_consent_;
  const bool is_focused_;
  const bool is_uvpaa_;
  bool attestation_consent_queried_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAuthenticatorRequestDelegate);
};

class TestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    if (return_null_delegate)
      return nullptr;
    return std::make_unique<TestAuthenticatorRequestDelegate>(
        render_frame_host,
        action_callbacks_registered_callback
            ? std::move(action_callbacks_registered_callback)
            : base::DoNothing(),
        enterprise_policy, attestation_consent, is_focused, is_uvpaa);
  }

  // If set, this closure will be called when the subsequently constructed
  // delegate is informed that the request has started.
  base::OnceClosure action_callbacks_registered_callback;

  EnterprisePolicy enterprise_policy = EnterprisePolicy::NOT_LISTED;
  AttestationConsent attestation_consent = AttestationConsent::NOT_USED;
  bool is_focused = true;

  bool is_uvpaa = false;

  // This emulates scenarios where a nullptr RequestClientDelegate is returned
  // because a request is already in progress.
  bool return_null_delegate = false;
};

// A test class that installs and removes an
// |AuthenticatorTestContentBrowserClient| automatically and can run tests
// against simulated attestation results.
class AuthenticatorContentBrowserClientTest : public AuthenticatorImplTest {
 public:
  AuthenticatorContentBrowserClientTest() = default;

  struct TestCase {
    AttestationConveyancePreference attestation_requested;
    EnterprisePolicy enterprise_policy;
    AttestationConsent attestation_consent;
    AuthenticatorStatus expected_status;
    AttestationType expected_attestation;
    const char* expected_certificate_substring;
  };

  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

  void RunTestCases(const std::vector<TestCase>& tests) {
    for (size_t i = 0; i < tests.size(); i++) {
      const auto& test = tests[i];
      if (test.attestation_consent != AttestationConsent::NOT_USED) {
        SCOPED_TRACE(test.attestation_consent == AttestationConsent::GRANTED
                         ? "consent granted"
                         : "consent denied");
      }
      SCOPED_TRACE(test.enterprise_policy == EnterprisePolicy::LISTED
                       ? "individual attestation"
                       : "no individual attestation");
      SCOPED_TRACE(
          AttestationConveyancePreferenceToString(test.attestation_requested));
      SCOPED_TRACE(i);

      test_client_.enterprise_policy = test.enterprise_policy;
      test_client_.attestation_consent = test.attestation_consent;

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->relying_party.id = "example.com";
      options->timeout = base::TimeDelta::FromSeconds(1);
      options->attestation =
          ConvertAttestationConveyancePreference(test.attestation_requested);

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));
      EXPECT_EQ(result.status, test.expected_status);

      if (test.expected_status != AuthenticatorStatus::SUCCESS) {
        ASSERT_EQ(AttestationType::ANY, test.expected_attestation);
        continue;
      }

      const device::AuthenticatorData auth_data =
          AuthDataFromMakeCredentialResponse(result.response);

      base::Optional<Value> attestation_value =
          Reader::Read(result.response->attestation_object);
      ASSERT_TRUE(attestation_value);
      ASSERT_TRUE(attestation_value->is_map());
      const auto& attestation = attestation_value->GetMap();

      switch (test.expected_attestation) {
        case AttestationType::ANY:
          ASSERT_STREQ("", test.expected_certificate_substring);
          break;

        case AttestationType::NONE:
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "none");
          EXPECT_TRUE(auth_data.attested_data()->IsAaguidZero());
          break;

        case AttestationType::NONE_WITH_NONZERO_AAGUID:
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "none");
          EXPECT_FALSE(auth_data.attested_data()->IsAaguidZero());
          break;

        case AttestationType::U2F:
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "fido-u2f");
          if (strlen(test.expected_certificate_substring) > 0) {
            ExpectCertificateContainingSubstring(
                attestation, test.expected_certificate_substring);
          }
          break;

        case AttestationType::PACKED:
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "packed");
          if (strlen(test.expected_certificate_substring) > 0) {
            ExpectCertificateContainingSubstring(
                attestation, test.expected_certificate_substring);
          }
          break;

        case AttestationType::SELF: {
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "packed");

          // A self-attestation should not include an X.509 chain nor ECDAA key.
          const auto attestation_statement_it =
              attestation.find(Value("attStmt"));
          ASSERT_TRUE(attestation_statement_it != attestation.end());
          ASSERT_TRUE(attestation_statement_it->second.is_map());
          const auto& attestation_statement =
              attestation_statement_it->second.GetMap();

          ASSERT_TRUE(attestation_statement.find(Value("x5c")) ==
                      attestation_statement.end());
          ASSERT_TRUE(attestation_statement.find(Value("ecdaaKeyId")) ==
                      attestation_statement.end());
          EXPECT_TRUE(auth_data.attested_data()->IsAaguidZero());
          break;
        }
        case AttestationType::SELF_WITH_NONZERO_AAGUID: {
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "packed");

          // A self-attestation should not include an X.509 chain nor ECDAA key.
          const auto attestation_statement_it =
              attestation.find(Value("attStmt"));
          ASSERT_TRUE(attestation_statement_it != attestation.end());
          ASSERT_TRUE(attestation_statement_it->second.is_map());
          const auto& attestation_statement =
              attestation_statement_it->second.GetMap();

          ASSERT_TRUE(attestation_statement.find(Value("x5c")) ==
                      attestation_statement.end());
          ASSERT_TRUE(attestation_statement.find(Value("ecdaaKeyId")) ==
                      attestation_statement.end());
          EXPECT_FALSE(auth_data.attested_data()->IsAaguidZero());
          break;
        }
      }
    }
  }

 protected:
  TestAuthenticatorContentBrowserClient test_client_;

 private:
  static const char* AttestationConveyancePreferenceToString(
      AttestationConveyancePreference v) {
    switch (v) {
      case AttestationConveyancePreference::NONE:
        return "none";
      case AttestationConveyancePreference::INDIRECT:
        return "indirect";
      case AttestationConveyancePreference::DIRECT:
        return "direct";
      case AttestationConveyancePreference::ENTERPRISE:
        return "enterprise";
      default:
        NOTREACHED();
        return "";
    }
  }

  // Expects that |map| contains the given key with a string-value equal to
  // |expected|.
  static void ExpectMapHasKeyWithStringValue(const Value::MapValue& map,
                                             const char* key,
                                             const char* expected) {
    const auto it = map.find(Value(key));
    ASSERT_TRUE(it != map.end()) << "No such key '" << key << "'";
    const auto& value = it->second;
    EXPECT_TRUE(value.is_string())
        << "Value of '" << key << "' has type "
        << static_cast<int>(value.type()) << ", but expected to find a string";
    EXPECT_EQ(std::string(expected), value.GetString())
        << "Value of '" << key << "' is '" << value.GetString()
        << "', but expected to find '" << expected << "'";
  }

  // Asserts that the webauthn attestation CBOR map in |attestation| contains a
  // single X.509 certificate containing |substring|.
  static void ExpectCertificateContainingSubstring(
      const Value::MapValue& attestation,
      const std::string& substring) {
    const auto& attestation_statement_it = attestation.find(Value("attStmt"));
    ASSERT_TRUE(attestation_statement_it != attestation.end());
    ASSERT_TRUE(attestation_statement_it->second.is_map());
    const auto& attestation_statement =
        attestation_statement_it->second.GetMap();
    const auto& x5c_it = attestation_statement.find(Value("x5c"));
    ASSERT_TRUE(x5c_it != attestation_statement.end());
    ASSERT_TRUE(x5c_it->second.is_array());
    const auto& x5c = x5c_it->second.GetArray();
    ASSERT_EQ(1u, x5c.size());
    ASSERT_TRUE(x5c[0].is_bytestring());
    base::StringPiece cert = x5c[0].GetBytestringAsString();
    EXPECT_TRUE(cert.find(substring) != cert.npos);
  }

  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorContentBrowserClientTest);
};

TEST_F(AuthenticatorContentBrowserClientTest, AttestationBehaviour) {
  const char kStandardCommonName[] = "U2F Attestation";
  const char kIndividualCommonName[] = "Individual Cert";

  const std::vector<TestCase> kTests = {
      {
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          EnterprisePolicy::LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::INDIRECT,
          EnterprisePolicy::LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          // Requesting enterprise attestation and not being approved results in
          // no attestation.
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kIndividualCommonName,
      },
  };

  virtual_device_factory_->mutable_state()->attestation_cert_common_name =
      kStandardCommonName;
  virtual_device_factory_->mutable_state()
      ->individual_attestation_cert_common_name = kIndividualCommonName;
  NavigateAndCommit(GURL("https://example.com"));

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest, Ctap2EnterpriseAttestation) {
  const char kStandardCommonName[] = "U2F Attestation";
  const char kIndividualCommonName[] = "Individual Cert";
  virtual_device_factory_->mutable_state()->attestation_cert_common_name =
      kStandardCommonName;
  virtual_device_factory_->mutable_state()
      ->individual_attestation_cert_common_name = kIndividualCommonName;
  NavigateAndCommit(GURL("https://example.com"));

  {
    SCOPED_TRACE("Without RP listed");

    device::VirtualCtap2Device::Config config;
    config.support_enterprise_attestation = true;
    virtual_device_factory_->SetCtap2Config(config);

    const std::vector<TestCase> kTests = {
        {
            AttestationConveyancePreference::ENTERPRISE,
            EnterprisePolicy::LISTED,
            AttestationConsent::NOT_USED,
            AuthenticatorStatus::SUCCESS,
            AttestationType::PACKED,
            kIndividualCommonName,
        },
        {
            // Requesting enterprise attestation and not being approved results
            // in no attestation.
            AttestationConveyancePreference::ENTERPRISE,
            EnterprisePolicy::NOT_LISTED,
            AttestationConsent::NOT_USED,
            AuthenticatorStatus::SUCCESS,
            AttestationType::NONE,
            "",
        },
    };

    RunTestCases(kTests);
  }

  {
    SCOPED_TRACE("With RP listed");

    device::VirtualCtap2Device::Config config;
    config.support_enterprise_attestation = true;
    config.enterprise_attestation_rps = {"example.com"};
    virtual_device_factory_->SetCtap2Config(config);

    const std::vector<TestCase> kTests = {
        {
            // Despite not being listed in enterprise policy, since the
            // authenticator recognises the RP ID, attestation should still be
            // returned.
            AttestationConveyancePreference::ENTERPRISE,
            EnterprisePolicy::NOT_LISTED,
            AttestationConsent::GRANTED_FOR_ENTERPRISE_ATTESTATION,
            AuthenticatorStatus::SUCCESS,
            AttestationType::PACKED,
            kIndividualCommonName,
        },
        {
            // Consent is required in the case that an enterprise attestation is
            // approved by an authenticator, however.
            AttestationConveyancePreference::ENTERPRISE,
            EnterprisePolicy::NOT_LISTED,
            AttestationConsent::DENIED_FOR_ENTERPRISE_ATTESTATION,
            AuthenticatorStatus::SUCCESS,
            AttestationType::NONE,
            "",
        },
    };

    RunTestCases(kTests);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       Ctap2EnterpriseAttestationUnsolicited) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.support_enterprise_attestation = true;
  virtual_device_factory_->SetCtap2Config(config);

  {
    EXPECT_EQ(
        AuthenticatorMakeCredential(GetTestPublicKeyCredentialCreationOptions())
            .status,
        AuthenticatorStatus::SUCCESS);
  }

  config.always_return_enterprise_attestation = true;
  virtual_device_factory_->SetCtap2Config(config);

  {
    EXPECT_EQ(
        AuthenticatorMakeCredential(GetTestPublicKeyCredentialCreationOptions())
            .status,
        AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       InappropriatelyIdentifyingAttestation) {
  // This common name is used by several devices that have inappropriately
  // identifying attestation certificates.
  const char kCommonName[] = "FT FIDO 0100";

  const std::vector<TestCase> kTests = {
      {
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          // If individual attestation was not requested then the attestation
          // certificate will be removed, even if consent is given, because the
          // consent isn't to be tracked.
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kCommonName,
      },
  };

  virtual_device_factory_->mutable_state()->attestation_cert_common_name =
      kCommonName;
  virtual_device_factory_->mutable_state()
      ->individual_attestation_cert_common_name = kCommonName;
  NavigateAndCommit(GURL("https://example.com"));

  RunTestCases(kTests);
}

// Test attestation erasure for an authenticator that uses self-attestation
// (which requires a zero AAGUID), but has a non-zero AAGUID. This mirrors the
// behavior of the Touch ID platform authenticator.
TEST_F(AuthenticatorContentBrowserClientTest,
       PlatformAuthenticatorAttestation) {
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  virtual_device_factory_->SetTransport(
      device::FidoTransportProtocol::kInternal);
  virtual_device_factory_->mutable_state()->self_attestation = true;
  virtual_device_factory_->mutable_state()
      ->non_zero_aaguid_with_self_attestation = true;
  NavigateAndCommit(GURL("https://example.com"));

  const std::vector<TestCase> kTests = {
      {
          // Self-attestation is defined as having a zero AAGUID, but
          // |non_zero_aaguid_with_self_attestation| is set above. Thus, if no
          // attestation is requested, the self-attestation will be removed but,
          // because the transport is kInternal, the AAGUID will be preserved.
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE_WITH_NONZERO_AAGUID,
          "",
      },
      {
          // If attestation is requested, but denied, we'll return none
          // attestation. But because the transport is kInternal, the AAGUID
          // will be preserved.
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE_WITH_NONZERO_AAGUID,
          "",
      },
      {
          // If attestation is requested and granted, the self attestation
          // will be returned.
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::SELF_WITH_NONZERO_AAGUID,
          "",
      },
  };

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest, Ctap2SelfAttestation) {
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  virtual_device_factory_->mutable_state()->self_attestation = true;
  NavigateAndCommit(GURL("https://example.com"));

  const std::vector<TestCase> kTests = {
      {
          // If no attestation is requested, we'll return the self attestation
          // rather than erasing it.
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::SELF,
          "",
      },
      {
          // If attestation is requested, but denied, we'll return none
          // attestation.
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          // If attestation is requested and granted, the self attestation will
          // be returned.
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::SELF,
          "",
      },
  };

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       Ctap2SelfAttestationNonZeroAaguid) {
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  virtual_device_factory_->mutable_state()->self_attestation = true;
  virtual_device_factory_->mutable_state()
      ->non_zero_aaguid_with_self_attestation = true;
  NavigateAndCommit(GURL("https://example.com"));

  const std::vector<TestCase> kTests = {
      {
          // Since the virtual device is configured to set a non-zero AAGUID the
          // self-attestation should still be replaced with a "none"
          // attestation.
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
  };

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest, BlockedAttestation) {
  NavigateAndCommit(GURL("https://foo.example.com"));

  static constexpr struct {
    const char* domains;
    AttestationConveyancePreference attestation;
    EnterprisePolicy enterprise_policy;
    AttestationType result;
  } kTests[] = {
      // Empty or nonsense parameter doesn't block anything.
      {
          "",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::U2F,
      },
      {
          " ,,   ,, ",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::U2F,
      },
      // Direct listing of domain blocks...
      {
          "foo.example.com",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::NONE,
      },
      // ... unless attestation is permitted by policy.
      {
          "foo.example.com",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AttestationType::U2F,
      },
      // Additional stuff in the string doesn't break the blocking.
      {
          "other,foo.example.com,,nonsenseXYZ123",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::NONE,
      },
      // The whole domain can be blocked.
      {
          "(*.)example.com",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::NONE,
      },
      // Policy again overrides
      {
          "(*.)example.com",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AttestationType::U2F,
      },
      // Trying to block everything doesn't work.
      {
          "(*.)",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::U2F,
      },
  };

  int test_num = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_num++);
    SCOPED_TRACE(test.domains);

    std::map<std::string, std::string> params;
    params.emplace("domains", test.domains);

    base::test::ScopedFeatureList scoped_feature_list_;
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        device::kWebAuthAttestationBlockList, params);

    const std::vector<TestCase> kTestCase = {
        {
            test.attestation,
            test.enterprise_policy,
            test.result == AttestationType::U2F ? AttestationConsent::GRANTED
                                                : AttestationConsent::NOT_USED,
            AuthenticatorStatus::SUCCESS,
            test.result,
            "",
        },
    };

    RunTestCases(kTestCase);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       MakeCredentialRequestStartedCallback) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  TestRequestStartedCallback request_started;
  test_client_.action_callbacks_registered_callback =
      request_started.callback();
  authenticator->MakeCredential(std::move(options), base::DoNothing());
  request_started.WaitForCallback();
}

TEST_F(AuthenticatorContentBrowserClientTest,
       GetAssertionRequestStartedCallback) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();

  TestRequestStartedCallback request_started;
  test_client_.action_callbacks_registered_callback =
      request_started.callback();
  authenticator->GetAssertion(std::move(options), base::DoNothing());
  request_started.WaitForCallback();
}

TEST_F(AuthenticatorContentBrowserClientTest, Unfocused) {
  // When the |ContentBrowserClient| considers the tab to be unfocused,
  // registration requests should fail with a |NOT_FOCUSED| error, but getting
  // assertions should still work.
  test_client_.is_focused = false;

  NavigateAndCommit(GURL(kTestOrigin1));

  {
    TestRequestStartedCallback request_started;
    test_client_.action_callbacks_registered_callback =
        request_started.callback();

    EXPECT_EQ(
        AuthenticatorMakeCredential(GetTestPublicKeyCredentialCreationOptions())
            .status,
        AuthenticatorStatus::NOT_FOCUSED);
    EXPECT_FALSE(request_started.was_called());
  }

  {
    device::PublicKeyCredentialDescriptor credential;
    credential.SetCredentialTypeForTesting(device::CredentialType::kPublicKey);
    credential.GetIdForTesting().resize(16);
    credential.GetTransportsForTesting() = {
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice};

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        credential.id(), kTestRelyingPartyId));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials.emplace_back(credential);

    TestRequestStartedCallback request_started;
    test_client_.action_callbacks_registered_callback =
        request_started.callback();

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
    EXPECT_TRUE(request_started.was_called());
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       NullDelegate_RejectsWithPendingRequest) {
  test_client_.return_null_delegate = true;
  NavigateAndCommit(GURL(kTestOrigin1));

  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::PENDING_REQUEST);
  }

  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::PENDING_REQUEST);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, IsUVPAAOverride) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  for (const bool is_uvpaa : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "is_uvpaa=" << is_uvpaa);
    test_client_.is_uvpaa = is_uvpaa;

    TestIsUvpaaCallback cb;
    authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(cb.callback());
    cb.WaitForCallback();
    EXPECT_EQ(is_uvpaa, cb.value());
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       CryptotokenBypassesAttestationConsentPrompt) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));

  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  // Despite the direct attestation conveyance preference, the request delegate
  // is not asked for attestation consent. Hence the request will succeed,
  // despite the handler denying all attestation consent prompts.
  options->attestation = device::AttestationConveyancePreference::kDirect;
  test_client_.attestation_consent = AttestationConsent::NOT_USED;

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       CableCredentialWithoutCableExtension) {
  // Exercise the case where a credential is marked as "cable" but no caBLE
  // extension is provided. The AuthenticatorRequestClientDelegate should see no
  // transports, which triggers it to cancel the request. (Outside of a testing
  // environment, Chrome's AuthenticatorRequestClientDelegate will show an
  // informative error and wait for the user to cancel the request.)
  EnableFeature(features::kWebAuthCable);
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  std::vector<uint8_t> id(32u, 1u);
  base::flat_set<device::FidoTransportProtocol> transports{
      device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};
  options->allow_credentials.clear();
  options->allow_credentials.emplace_back(device::CredentialType::kPublicKey,
                                          std::move(id), std::move(transports));

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

class MockAuthenticatorRequestDelegateObserver
    : public TestAuthenticatorRequestDelegate {
 public:
  using InterestingFailureReasonCallback =
      base::OnceCallback<void(InterestingFailureReason)>;

  explicit MockAuthenticatorRequestDelegateObserver(
      InterestingFailureReasonCallback failure_reasons_callback =
          base::DoNothing())
      : TestAuthenticatorRequestDelegate(
            nullptr /* render_frame_host */,
            base::DoNothing() /* did_start_request_callback */,
            EnterprisePolicy::NOT_LISTED,
            AttestationConsent::NOT_USED,
            true /* is_focused */,
            /*is_uvpaa=*/false),
        failure_reasons_callback_(std::move(failure_reasons_callback)) {}
  ~MockAuthenticatorRequestDelegateObserver() override = default;

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override {
    CHECK(failure_reasons_callback_);
    std::move(failure_reasons_callback_).Run(reason);
    return false;
  }

  MOCK_METHOD1(
      OnTransportAvailabilityEnumerated,
      void(device::FidoRequestHandlerBase::TransportAvailabilityInfo data));
  MOCK_METHOD1(EmbedderControlsAuthenticatorDispatch,
               bool(const device::FidoAuthenticator&));
  MOCK_METHOD1(FidoAuthenticatorAdded, void(const device::FidoAuthenticator&));
  MOCK_METHOD1(FidoAuthenticatorRemoved, void(base::StringPiece));

 private:
  InterestingFailureReasonCallback failure_reasons_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockAuthenticatorRequestDelegateObserver);
};

// Fake test construct that shares all other behavior with AuthenticatorCommon
// except that:
//  - FakeAuthenticatorCommon does not trigger UI activity.
//  - MockAuthenticatorRequestDelegateObserver is injected to
//  |request_delegate_|
//    instead of ChromeAuthenticatorRequestDelegate.
class FakeAuthenticatorCommon : public AuthenticatorCommon {
 public:
  explicit FakeAuthenticatorCommon(
      RenderFrameHost* render_frame_host,
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate)
      : AuthenticatorCommon(render_frame_host),
        mock_delegate_(std::move(mock_delegate)) {}
  ~FakeAuthenticatorCommon() override = default;

  std::unique_ptr<AuthenticatorRequestClientDelegate> CreateRequestDelegate()
      override {
    DCHECK(mock_delegate_);
    return std::move(mock_delegate_);
  }

 private:
  friend class AuthenticatorImplRequestDelegateTest;

  std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate_;
};

class AuthenticatorImplRequestDelegateTest : public AuthenticatorImplTest {
 public:
  AuthenticatorImplRequestDelegateTest() = default;
  ~AuthenticatorImplRequestDelegateTest() override = default;

  void TearDown() override {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    authenticator_impl_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  mojo::Remote<blink::mojom::Authenticator> ConnectToFakeAuthenticator(
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> delegate) {
    authenticator_impl_ = std::make_unique<AuthenticatorImpl>(
        main_rfh(), std::make_unique<FakeAuthenticatorCommon>(
                        main_rfh(), std::move(delegate)));
    mojo::Remote<blink::mojom::Authenticator> authenticator;
    authenticator_impl_->Bind(authenticator.BindNewPipeAndPassReceiver());
    return authenticator;
  }
};

TEST_F(AuthenticatorImplRequestDelegateTest,
       TestRequestDelegateObservesFidoRequestHandler) {
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillRepeatedly(::testing::Return(true));

  auto discovery_factory =
      std::make_unique<device::test::FakeFidoDiscoveryFactory>();
  auto* fake_hid_discovery = discovery_factory->ForgeNextHidDiscovery();
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(std::move(discovery_factory));

  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;

  auto mock_delegate =
      std::make_unique<MockAuthenticatorRequestDelegateObserver>();
  auto* const mock_delegate_ptr = mock_delegate.get();
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  auto mock_usb_device = device::MockFidoDevice::MakeCtap();
  mock_usb_device->StubGetId();
  mock_usb_device->SetDeviceTransport(
      device::FidoTransportProtocol::kUsbHumanInterfaceDevice);
  const auto device_id = mock_usb_device->GetId();

  EXPECT_CALL(*mock_delegate_ptr, OnTransportAvailabilityEnumerated(_));
  EXPECT_CALL(*mock_delegate_ptr, EmbedderControlsAuthenticatorDispatch(_))
      .WillOnce(testing::Return(true));

  base::RunLoop usb_device_found_done;
  EXPECT_CALL(*mock_delegate_ptr, FidoAuthenticatorAdded(_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&usb_device_found_done]() { usb_device_found_done.Quit(); }));

  base::RunLoop usb_device_lost_done;
  EXPECT_CALL(*mock_delegate_ptr, FidoAuthenticatorRemoved(_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&usb_device_lost_done]() { usb_device_lost_done.Quit(); }));

  authenticator->GetAssertion(std::move(options), callback_receiver.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
  fake_hid_discovery->AddDevice(std::move(mock_usb_device));
  usb_device_found_done.Run();

  fake_hid_discovery->RemoveDevice(device_id);
  usb_device_lost_done.Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AuthenticatorImplRequestDelegateTest, FailureReasonForTimeout) {
  // The VirtualFidoAuthenticator simulates a tap immediately after it gets the
  // request. Replace by the real discovery that will wait until timeout.
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<device::FidoDiscoveryFactory>());
  NavigateAndCommit(GURL(kTestOrigin1));

  FailureReasonCallbackReceiver failure_reason_receiver;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_receiver.callback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(GetTestPublicKeyCredentialRequestOptions(),
                              callback_receiver.callback());

  task_environment()->FastForwardBy(kTestTimeout);

  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());

  ASSERT_TRUE(failure_reason_receiver.was_called());
  EXPECT_EQ(content::AuthenticatorRequestClientDelegate::
                InterestingFailureReason::kTimeout,
            std::get<0>(*failure_reason_receiver.result()));
}

TEST_F(AuthenticatorImplRequestDelegateTest,
       FailureReasonForDuplicateRegistration) {
  NavigateAndCommit(GURL(kTestOrigin1));

  FailureReasonCallbackReceiver failure_reason_receiver;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_receiver.callback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = GetTestCredentials();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->exclude_credentials[0].id(), kTestRelyingPartyId));

  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());

  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_EXCLUDED,
            callback_receiver.status());

  ASSERT_TRUE(failure_reason_receiver.was_called());
  EXPECT_EQ(content::AuthenticatorRequestClientDelegate::
                InterestingFailureReason::kKeyAlreadyRegistered,
            std::get<0>(*failure_reason_receiver.result()));
}

TEST_F(AuthenticatorImplRequestDelegateTest,
       FailureReasonForMissingRegistration) {
  NavigateAndCommit(GURL(kTestOrigin1));

  FailureReasonCallbackReceiver failure_reason_receiver;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_receiver.callback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(GetTestPublicKeyCredentialRequestOptions(),
                              callback_receiver.callback());

  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());

  ASSERT_TRUE(failure_reason_receiver.was_called());
  EXPECT_EQ(content::AuthenticatorRequestClientDelegate::
                InterestingFailureReason::kKeyNotRegistered,
            std::get<0>(*failure_reason_receiver.result()));
}

TEST_F(AuthenticatorImplTest, Transports) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (auto protocol :
       {device::ProtocolVersion::kU2f, device::ProtocolVersion::kCtap2}) {
    SCOPED_TRACE(static_cast<int>(protocol));
    virtual_device_factory_->SetSupportedProtocol(protocol);

    for (const auto transport : std::map<device::FidoTransportProtocol,
                                         blink::mojom::AuthenticatorTransport>(
             {{device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
               blink::mojom::AuthenticatorTransport::USB},
              {device::FidoTransportProtocol::kBluetoothLowEnergy,
               blink::mojom::AuthenticatorTransport::BLE},
              {device::FidoTransportProtocol::kNearFieldCommunication,
               blink::mojom::AuthenticatorTransport::NFC}})) {
      virtual_device_factory_->SetTransport(transport.first);

      MakeCredentialResult result = AuthenticatorMakeCredential();
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);

      const std::vector<device::FidoTransportProtocol>& transports(
          result.response->transports);
      ASSERT_EQ(1u, transports.size());
      EXPECT_EQ(transport.first, transports[0]);
    }
  }
}

TEST_F(AuthenticatorImplTest, ExtensionHMACSecret) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (const bool include_extension : {false, true}) {
    for (const bool authenticator_support : {false, true}) {
      SCOPED_TRACE(include_extension);
      SCOPED_TRACE(authenticator_support);

      device::VirtualCtap2Device::Config config;
      config.hmac_secret_support = authenticator_support;
      virtual_device_factory_->SetCtap2Config(config);

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->hmac_create_secret = include_extension;
      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);

      device::AuthenticatorData parsed_auth_data =
          AuthDataFromMakeCredentialResponse(result.response);

      // The virtual CTAP2 device always echos the hmac-secret extension on
      // registrations. Therefore, if |hmac_secret| was set above it should be
      // serialised in the CBOR and correctly passed all the way back around to
      // the reply's authenticator data.
      bool has_hmac_secret = false;
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
      }

      EXPECT_EQ(include_extension && authenticator_support, has_hmac_secret);
    }
  }
}

// Tests that for an authenticator that does not support batching, credential
// lists get probed silently to work around authenticators rejecting exclude
// lists exceeding a certain size.
TEST_F(AuthenticatorImplTest, MakeCredentialWithLargeExcludeList) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool has_excluded_credential : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "has_excluded_credential=" << has_excluded_credential);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.reject_large_allow_and_exclude_lists = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->exclude_credentials = GetTestCredentials(/*num_credentials=*/10);
    if (has_excluded_credential) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          options->exclude_credentials.back().id(), kTestRelyingPartyId));
    }

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              has_excluded_credential ? AuthenticatorStatus::CREDENTIAL_EXCLUDED
                                      : AuthenticatorStatus::SUCCESS);
  }
}

// Tests that for an authenticator that does not support batching, credential
// lists get probed silently to work around authenticators rejecting allow lists
// exceeding a certain size.
TEST_F(AuthenticatorImplTest, GetAssertionWithLargeAllowList) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool has_allowed_credential : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "has_allowed_credential=" << has_allowed_credential);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.reject_large_allow_and_exclude_lists = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = GetTestCredentials(/*num_credentials=*/10);
    if (has_allowed_credential) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          options->allow_credentials.back().id(), kTestRelyingPartyId));
    }

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              has_allowed_credential ? AuthenticatorStatus::SUCCESS
                                     : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

// Tests that, regardless of batching support, GetAssertion requests with a
// single allowed credential ID don't result in a silent probing request.
TEST_F(AuthenticatorImplTest, GetAssertionSingleElementAllowListDoesNotProbe) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool supports_batching : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "supports_batching=" << supports_batching);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    if (supports_batching) {
      config.max_credential_id_length = kTestCredentialIdLength;
      config.max_credential_count_in_list = 10;
    }
    config.reject_silent_authentication_requests = true;
    virtual_device_factory_->SetCtap2Config(config);

    auto test_credentials = GetTestCredentials(/*num_credentials=*/1);
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        test_credentials.front().id(), kTestRelyingPartyId));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = std::move(test_credentials);

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
  }
}

// Tests that an allow list that fits into a single batch does not result in a
// silent probing request.
TEST_F(AuthenticatorImplTest, GetAssertionSingleBatchListDoesNotProbe) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool allow_list_fits_single_batch : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "allow_list_fits_single_batch="
                                      << allow_list_fits_single_batch);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.max_credential_id_length = kTestCredentialIdLength;
    constexpr size_t kBatchSize = 10;
    config.max_credential_count_in_list = kBatchSize;
    config.reject_silent_authentication_requests = true;
    virtual_device_factory_->SetCtap2Config(config);

    auto test_credentials = GetTestCredentials(
        /*num_credentials=*/kBatchSize +
        (allow_list_fits_single_batch ? 0 : 1));
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        test_credentials.back().id(), kTestRelyingPartyId));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = std::move(test_credentials);

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              allow_list_fits_single_batch
                  ? AuthenticatorStatus::SUCCESS
                  : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, OptionalCredentialInAssertionResponse) {
  // This test exercises the unfortunate optionality in the CTAP2 spec r.e.
  // whether an authenticator returns credential information when the allowlist
  // only has a single entry.
  NavigateAndCommit(GURL(kTestOrigin1));

  for (const auto behavior :
       {device::VirtualCtap2Device::Config::IncludeCredential::ONLY_IF_NEEDED,
        device::VirtualCtap2Device::Config::IncludeCredential::ALWAYS,
        device::VirtualCtap2Device::Config::IncludeCredential::NEVER}) {
    SCOPED_TRACE(static_cast<int>(behavior));

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.include_credential_in_assertion_response = behavior;
    config.max_credential_count_in_list = 10;
    config.max_credential_id_length = 256;
    virtual_device_factory_->SetCtap2Config(config);

    size_t num_credentials;
    bool should_timeout = false;
    switch (behavior) {
      case device::VirtualCtap2Device::Config::IncludeCredential::
          ONLY_IF_NEEDED:
        // The behaviour to test for |ONLY_IF_NEEDED| is that an omitted
        // credential in the response is handled correctly.
        num_credentials = 1;
        break;
      case device::VirtualCtap2Device::Config::IncludeCredential::ALWAYS:
        // Also test that a technically-superfluous credential in the response
        // is handled.
        num_credentials = 1;
        break;
      case device::VirtualCtap2Device::Config::IncludeCredential::NEVER:
        // Test that omitting a credential in an ambiguous context causes a
        // failure.
        num_credentials = 2;
        should_timeout = true;
        break;
    }

    auto test_credentials = GetTestCredentials(num_credentials);
    for (const auto& cred : test_credentials) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          cred.id(), kTestRelyingPartyId));
    }

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = std::move(test_credentials);

    if (should_timeout) {
      EXPECT_EQ(
          AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
          AuthenticatorStatus::NOT_ALLOWED_ERROR);
    } else {
      EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
                AuthenticatorStatus::SUCCESS);
    }
  }
}

// Tests that an allowList with only credential IDs of a length exceeding the
// maxCredentialIdLength parameter is not mistakenly interpreted as an empty
// allow list.
TEST_F(AuthenticatorImplTest, AllowListWithOnlyOversizedCredentialIds) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.max_credential_id_length = kTestCredentialIdLength;
  config.max_credential_count_in_list = 10;
  virtual_device_factory_->SetCtap2Config(config);

  const std::vector<uint8_t> cred_id(kTestCredentialIdLength + 1, 0);
  // Inject registration so that the test will fail (because of a successful
  // response) if the oversized credential ID is sent.
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      cred_id, kTestRelyingPartyId));

  for (const bool has_app_id : {false, true}) {
    SCOPED_TRACE(has_app_id);
    virtual_device_factory_->mutable_state()->allow_list_sizes.clear();

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    if (has_app_id) {
      options->appid = kTestOrigin1;
    }
    options->allow_credentials = {device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, cred_id)};

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::NOT_ALLOWED_ERROR);
    const auto& allow_list_sizes =
        virtual_device_factory_->mutable_state()->allow_list_sizes;
    // No empty allow-list requests should have been made.
    EXPECT_TRUE(std::none_of(allow_list_sizes.cbegin(), allow_list_sizes.cend(),
                             [](size_t size) { return size == 0; }));
  }
}

TEST_F(AuthenticatorImplTest, NoUnexpectedAuthenticatorExtensions) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.add_extra_extension = true;
  virtual_device_factory_->SetCtap2Config(config);

  // Check that extra authenticator extensions are rejected when creating a
  // credential.
  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);

  // Extensions should also be rejected when getting an assertion.
  PublicKeyCredentialRequestOptionsPtr assertion_options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      assertion_options->allow_credentials.back().id(), kTestRelyingPartyId));
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(assertion_options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, NoUnexpectedClientExtensions) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.reject_all_extensions = true;
  virtual_device_factory_->SetCtap2Config(config);

  // Check that no unexpected client extensions are sent to the authenticator.
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);

  // No extensions should be sent when getting an assertion either.
  PublicKeyCredentialRequestOptionsPtr assertion_options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      assertion_options->allow_credentials.back().id(), kTestRelyingPartyId));
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(assertion_options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorImplTest, AndroidClientDataExtension) {
  EnableFeature(device::kWebAuthPhoneSupport);
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.support_android_client_data_extension = true;
  virtual_device_factory_->SetCtap2Config(config);

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);

  PublicKeyCredentialRequestOptionsPtr assertion_options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      assertion_options->allow_credentials.back().id(), kTestRelyingPartyId));
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(assertion_options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorImplTest, UnsolicitedAndroidClientDataExtensionReponse) {
  EnableFeature(device::kWebAuthPhoneSupport);
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.send_unsolicited_android_client_data_extension = true;
  virtual_device_factory_->SetCtap2Config(config);

  // An unsolicited androidClientData extension response results in an error.
  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);

  // The same goes for getAssertion.
  PublicKeyCredentialRequestOptionsPtr assertion_options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      assertion_options->allow_credentials.back().id(), kTestRelyingPartyId));
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(assertion_options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// Tests that on an authenticator that supports batching, exclude lists that fit
// into a single batch are sent without probing.
TEST_F(AuthenticatorImplTest, ExcludeListBatching) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool authenticator_has_excluded_credential : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "authenticator_has_excluded_credential="
                 << authenticator_has_excluded_credential);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.max_credential_id_length = kTestCredentialIdLength;
    constexpr size_t kBatchSize = 10;
    config.max_credential_count_in_list = kBatchSize;
    // Reject silent authentication requests to ensure we are not probing
    // credentials silently, since the exclude list should fit into a single
    // batch.
    config.reject_silent_authentication_requests = true;
    virtual_device_factory_->SetCtap2Config(config);

    auto test_credentials = GetTestCredentials(kBatchSize);
    test_credentials.insert(
        test_credentials.end() - 1,
        {device::CredentialType::kPublicKey,
         std::vector<uint8_t>(kTestCredentialIdLength + 1, 1)});
    if (authenticator_has_excluded_credential) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          test_credentials.back().id(), kTestRelyingPartyId));
    }

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->exclude_credentials = std::move(test_credentials);
    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              authenticator_has_excluded_credential
                  ? AuthenticatorStatus::CREDENTIAL_EXCLUDED
                  : AuthenticatorStatus::SUCCESS);
  }
}

TEST_F(AuthenticatorImplTest, GetPublicKey) {
  device::VirtualCtap2Device::Config config;
  config.support_invalid_for_testing_algorithm = true;
  virtual_device_factory_->SetCtap2Config(config);
  NavigateAndCommit(GURL(kTestOrigin1));

  static constexpr struct {
    device::CoseAlgorithmIdentifier algo;
    base::Optional<int> evp_id;
  } kTests[] = {
      {device::CoseAlgorithmIdentifier::kEs256, EVP_PKEY_EC},
      {device::CoseAlgorithmIdentifier::kRs256, EVP_PKEY_RSA},
      {device::CoseAlgorithmIdentifier::kEdDSA, EVP_PKEY_ED25519},
      {device::CoseAlgorithmIdentifier::kInvalidForTesting, base::nullopt},
  };

  for (const auto& test : kTests) {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->public_key_parameters =
        GetTestPublicKeyCredentialParameters(static_cast<int32_t>(test.algo));

    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    const auto& response = result.response;
    EXPECT_EQ(response->public_key_algo, static_cast<int32_t>(test.algo));
    EXPECT_FALSE(response->info->authenticator_data.empty());

    ASSERT_EQ(test.evp_id.has_value(), response->public_key_der.has_value());
    if (!test.evp_id) {
      continue;
    }

    const std::vector<uint8_t>& public_key_der =
        response->public_key_der.value();

    CBS cbs;
    CBS_init(&cbs, public_key_der.data(), public_key_der.size());
    bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
    EXPECT_EQ(0u, CBS_len(&cbs));
    ASSERT_TRUE(pkey.get());

    EXPECT_EQ(test.evp_id.value(), EVP_PKEY_id(pkey.get()));
  }
}

TEST_F(AuthenticatorImplTest, VirtualAuthenticatorPublicKeyAlgos) {
  // Exercise all the public key types in the virtual authenticator for create()
  // and get().
  device::VirtualCtap2Device::Config config;
  virtual_device_factory_->SetCtap2Config(config);
  NavigateAndCommit(GURL(kTestOrigin1));

  static const struct {
    device::CoseAlgorithmIdentifier algo;
    const EVP_MD* digest;
  } kTests[] = {
      {device::CoseAlgorithmIdentifier::kEs256, EVP_sha256()},
      {device::CoseAlgorithmIdentifier::kRs256, EVP_sha256()},
      {device::CoseAlgorithmIdentifier::kEdDSA, nullptr},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(static_cast<int>(test.algo));

    PublicKeyCredentialCreationOptionsPtr create_options =
        GetTestPublicKeyCredentialCreationOptions();
    create_options->public_key_parameters =
        GetTestPublicKeyCredentialParameters(static_cast<int32_t>(test.algo));

    MakeCredentialResult create_result =
        AuthenticatorMakeCredential(std::move(create_options));
    ASSERT_EQ(create_result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(create_result.response->public_key_algo,
              static_cast<int32_t>(test.algo));

    const std::vector<uint8_t>& public_key_der =
        create_result.response->public_key_der.value();
    CBS cbs;
    CBS_init(&cbs, public_key_der.data(), public_key_der.size());
    bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
    EXPECT_EQ(0u, CBS_len(&cbs));
    ASSERT_TRUE(pkey.get());

    PublicKeyCredentialRequestOptionsPtr get_options =
        GetTestPublicKeyCredentialRequestOptions();
    device::PublicKeyCredentialDescriptor public_key(
        device::CredentialType::kPublicKey,
        create_result.response->info->raw_id,
        {device::FidoTransportProtocol::kUsbHumanInterfaceDevice});
    get_options->allow_credentials = {std::move(public_key)};
    GetAssertionResult get_result =
        AuthenticatorGetAssertion(std::move(get_options));
    ASSERT_EQ(get_result.status, AuthenticatorStatus::SUCCESS);
    base::span<const uint8_t> signature(get_result.response->signature);
    std::vector<uint8_t> signed_data(
        get_result.response->info->authenticator_data);
    const std::array<uint8_t, crypto::kSHA256Length> client_data_json_hash(
        crypto::SHA256Hash(get_result.response->info->client_data_json));
    signed_data.insert(signed_data.end(), client_data_json_hash.begin(),
                       client_data_json_hash.end());

    bssl::ScopedEVP_MD_CTX md_ctx;
    ASSERT_EQ(EVP_DigestVerifyInit(md_ctx.get(), /*pctx=*/nullptr, test.digest,
                                   /*engine=*/nullptr, pkey.get()),
              1);
    EXPECT_EQ(EVP_DigestVerify(md_ctx.get(), signature.data(), signature.size(),
                               signed_data.data(), signed_data.size()),
              1);
  }
}

TEST_F(AuthenticatorImplTest, ResetDiscoveryFactoryOverride) {
  // This is a regression test for crbug.com/1087158.
  NavigateAndCommit(GURL(kTestOrigin1));

  // Make the entire discovery factory disappear mid-request.
  bool was_called = false;
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        was_called = true;
        ResetVirtualDevice();
        return false;
      });

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

static constexpr char kTestPIN[] = "1234";

class UVTestAuthenticatorClientDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  explicit UVTestAuthenticatorClientDelegate(bool* collected_pin,
                                             bool* did_bio_enrollment,
                                             bool cancel_bio_enrollment)
      : collected_pin_(collected_pin),
        did_bio_enrollment_(did_bio_enrollment),
        cancel_bio_enrollment_(cancel_bio_enrollment) {
    *collected_pin_ = false;
    *did_bio_enrollment_ = false;
  }

  bool SupportsPIN() const override { return true; }

  void CollectPIN(
      base::Optional<int> attempts,
      base::OnceCallback<void(std::string)> provide_pin_cb) override {
    *collected_pin_ = true;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(provide_pin_cb), kTestPIN));
  }

  void StartBioEnrollment(base::OnceClosure next_callback) override {
    *did_bio_enrollment_ = true;
    if (cancel_bio_enrollment_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(next_callback));
      return;
    }
    bio_callback_ = std::move(next_callback);
  }

  void OnSampleCollected(int remaining_samples) override {
    if (remaining_samples <= 0) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(bio_callback_));
    }
  }

  void FinishCollectToken() override {}

 private:
  bool* collected_pin_;
  base::OnceClosure bio_callback_;
  bool* did_bio_enrollment_;
  bool cancel_bio_enrollment_;
};

class UVTestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<UVTestAuthenticatorClientDelegate>(
        &collected_pin_, &did_bio_enrollment_, cancel_bio_enrollment_);
  }

  bool collected_pin() { return collected_pin_; }

  bool did_bio_enrollment() { return did_bio_enrollment_; }

  void set_cancel_bio_enrollment(bool cancel_bio_enrollment) {
    cancel_bio_enrollment_ = cancel_bio_enrollment;
  }

 private:
  bool collected_pin_;
  bool did_bio_enrollment_;
  bool cancel_bio_enrollment_ = false;
};

class UVAuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  UVAuthenticatorImplTest() = default;

  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

 protected:
  static PublicKeyCredentialCreationOptionsPtr make_credential_options(
      device::UserVerificationRequirement uv =
          device::UserVerificationRequirement::kRequired) {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        uv);
    return options;
  }

  static PublicKeyCredentialRequestOptionsPtr get_credential_options(
      device::UserVerificationRequirement uv =
          device::UserVerificationRequirement::kRequired) {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->user_verification = uv;
    return options;
  }

  static const char* UVToString(device::UserVerificationRequirement uv) {
    switch (uv) {
      case device::UserVerificationRequirement::kDiscouraged:
        return "discouraged";
      case device::UserVerificationRequirement::kPreferred:
        return "preferred";
      case device::UserVerificationRequirement::kRequired:
        return "required";
    }
  }

  static bool HasUV(const MakeCredentialAuthenticatorResponsePtr& response) {
    return AuthDataFromMakeCredentialResponse(response)
        .obtained_user_verification();
  }

  static bool HasUV(const GetAssertionAuthenticatorResponsePtr& response) {
    base::Optional<device::AuthenticatorData> auth_data =
        device::AuthenticatorData::DecodeAuthenticatorData(
            response->info->authenticator_data);
    return auth_data->obtained_user_verification();
  }

  UVTestAuthenticatorContentBrowserClient test_client_;

 private:
  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UVAuthenticatorImplTest);
};

// PINList is a list of expected |attempts| values and the PIN to answer with.
using PINList = std::list<std::pair<base::Optional<int>, std::string>>;

class PINTestAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  explicit PINTestAuthenticatorRequestDelegate(
      bool supports_pin,
      const PINList& pins,
      base::Optional<InterestingFailureReason>* failure_reason)
      : supports_pin_(supports_pin),
        expected_(pins),
        failure_reason_(failure_reason) {}
  ~PINTestAuthenticatorRequestDelegate() override { DCHECK(expected_.empty()); }

  bool SupportsPIN() const override { return supports_pin_; }

  void CollectPIN(
      base::Optional<int> attempts,
      base::OnceCallback<void(std::string)> provide_pin_cb) override {
    DCHECK(supports_pin_);
    DCHECK(!expected_.empty());
    DCHECK(attempts == expected_.front().first)
        << "got: " << attempts.value_or(-1)
        << " expected: " << expected_.front().first.value_or(-1);
    std::string pin = std::move(expected_.front().second);
    expected_.pop_front();

    base::SequencedTaskRunnerHandle::Get()->PostTask(
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
  PINList expected_;
  base::Optional<InterestingFailureReason>* const failure_reason_;
  DISALLOW_COPY_AND_ASSIGN(PINTestAuthenticatorRequestDelegate);
};

class PINTestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<PINTestAuthenticatorRequestDelegate>(
        supports_pin, expected, &failure_reason);
  }

  bool supports_pin = true;
  PINList expected;
  base::Optional<InterestingFailureReason> failure_reason;
};

class PINAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  PINAuthenticatorImplTest() = default;

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

  void ConfigureVirtualDevice(bool pin_uv_auth_token, int support_level) {
    device::VirtualCtap2Device::Config config;
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
  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PINAuthenticatorImplTest);
};

static constexpr device::UserVerificationRequirement kUVLevel[3] = {
    device::UserVerificationRequirement::kDiscouraged,
    device::UserVerificationRequirement::kPreferred,
    device::UserVerificationRequirement::kRequired,
};

static const char* kUVDescription[3] = {"discouraged", "preferred", "required"};

static const char* kPINSupportDescription[3] = {"no PIN support", "PIN not set",
                                                "PIN set"};

TEST_F(PINAuthenticatorImplTest, MakeCredential) {
  typedef int Expectations[3][3];
  // kExpectedWithUISupport enumerates the expected behaviour when the embedder
  // supports prompting the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithUISupport = {
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kSetPIN  },
    /* PIN set */     {    kUsePIN,     kUsePIN,    kUsePIN  },
    //                        ^
    //                        |
    //            VirtualCtap2Device cannot fall back to U2F.
  };
  // clang-format on

  // kExpectedWithoutUISupport enumerates the expected behaviour when the
  // embedder cannot prompt the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithoutUISupport = {
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN set */     {    kFailure,    kFailure,   kFailure },
    //                        ^            ^
    //                        |            |
    //            VirtualCtap2Device cannot fall back to U2F and
    //            a PIN is required to create credentials once set
    //            in CTAP 2.0.
  };
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
        SCOPED_TRACE(kPINSupportDescription[support_level]);
        ConfigureVirtualDevice(pin_uv_auth_token, support_level);

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
              test_client_.expected = {{base::nullopt, kTestPIN}};
              break;

            case kUsePIN:
              // A single PIN prompt to get the PIN is expected.
              test_client_.expected = {{8, kTestPIN}};
              break;

            default:
              NOTREACHED();
          }

          MakeCredentialResult result = AuthenticatorMakeCredential(
              make_credential_options(kUVLevel[uv_level]));

          switch (expected[support_level][uv_level]) {
            case kFailure:
              EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
              break;

            case kNoPIN:
              EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
              EXPECT_EQ("", virtual_device_factory_->mutable_state()->pin);
              EXPECT_FALSE(HasUV(result.response));
              break;

            case kSetPIN:
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

TEST_F(PINAuthenticatorImplTest, MakeCredentialSoftLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  test_client_.expected = {{8, "wrong"}, {7, "wrong"}, {6, "wrong"}};
  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->pin_retries);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->soft_locked);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kSoftPINBlock,
            *test_client_.failure_reason);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialHardLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries = 1;

  test_client_.expected = {{1, "wrong"}};
  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(0, virtual_device_factory_->mutable_state()->pin_retries);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kHardPINBlock,
            *test_client_.failure_reason);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialWrongPINFirst) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  // Test that we can successfully get a PIN token after a failure.
  test_client_.expected = {{8, "wrong"}, {7, kTestPIN}};
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(static_cast<int>(device::kMaxPinRetries),
            virtual_device_factory_->mutable_state()->pin_retries);
}

TEST_F(PINAuthenticatorImplTest, GetAssertion) {
  typedef int Expectations[3][3];
  // kExpectedWithUISupport enumerates the expected behaviour when the embedder
  // supports prompting the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithUISupport = {
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN set */     {    kNoPIN,      kUsePIN,    kUsePIN  },
  };
  // clang-format on

  // kExpectedWithoutUISupport enumerates the expected behaviour when the
  // embedder cannot prompt the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithoutUISupport = {
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN set */     {    kNoPIN,      kNoPIN,     kFailure },
  };
  // clang-format on

  PublicKeyCredentialRequestOptionsPtr dummy_options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      dummy_options->allow_credentials[0].id(), kTestRelyingPartyId));

  for (bool pin_uv_auth_token : {false, true}) {
    for (bool ui_support : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "ui_support=" << ui_support);
      const Expectations& expected =
          ui_support ? kExpectedWithUISupport : kExpectedWithoutUISupport;
      test_client_.supports_pin = ui_support;

      for (int support_level = 0; support_level <= 2; support_level++) {
        SCOPED_TRACE(kPINSupportDescription[support_level]);
        ConfigureVirtualDevice(pin_uv_auth_token, support_level);

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
              test_client_.expected = {{8, kTestPIN}};
              break;

            default:
              NOTREACHED();
          }

          GetAssertionResult result = AuthenticatorGetAssertion(
              get_credential_options(kUVLevel[uv_level]));

          switch (expected[support_level][uv_level]) {
            case kFailure:
              EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
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

TEST_F(PINAuthenticatorImplTest, GetAssertionSoftLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  test_client_.expected = {{8, "wrong"}, {7, "wrong"}, {6, "wrong"}};
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->pin_retries);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->soft_locked);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kSoftPINBlock,
            *test_client_.failure_reason);
}

TEST_F(PINAuthenticatorImplTest, GetAssertionHardLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries = 1;

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  test_client_.expected = {{1, "wrong"}};
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(0, virtual_device_factory_->mutable_state()->pin_retries);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kHardPINBlock,
            *test_client_.failure_reason);
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
      config.support_invalid_for_testing_algorithm = false;
      virtual_device_factory_->SetCtap2Config(config);
    } else if (i == 1) {
      device::VirtualCtap2Device::Config config;
      // The second config is a device with a PIN set that _does_ support the
      // algorithm. Since the PIN is set, we might convert the makeCredential
      // request to U2F, but shouldn't because the algorithm cannot be
      // represented in U2F.
      config.support_invalid_for_testing_algorithm = true;
      config.u2f_support = true;
      config.pin_support = true;
      virtual_device_factory_->mutable_state()->pin = kTestPIN;
      virtual_device_factory_->mutable_state()->pin_retries =
          device::kMaxPinRetries;
      virtual_device_factory_->SetCtap2Config(config);
      test_client_.expected = {{device::kMaxPinRetries, kTestPIN}};
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
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        device::UserVerificationRequirement::kDiscouraged);
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
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    // Set uv=discouraged so that U2F fallback is possible.
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        device::UserVerificationRequirement::kDiscouraged);

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
      test_client_.expected = {{device::kMaxPinRetries, kTestPIN}};
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

class InternalUVAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  struct TestCase {
    const bool fingerprints_enrolled;
    const bool supports_pin;
    const device::UserVerificationRequirement uv;
  };

  InternalUVAuthenticatorImplTest() = default;

  void SetUp() override {
    UVAuthenticatorImplTest::SetUp();
    NavigateAndCommit(GURL(kTestOrigin1));
  }

  std::vector<TestCase> GetTestCases() {
    std::vector<TestCase> test_cases;
    for (const bool fingerprints_enrolled : {true, false}) {
      for (const bool supports_pin : {true, false}) {
        // Avoid just testing for PIN.
        if (!fingerprints_enrolled && supports_pin)
          continue;
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
    SCOPED_TRACE(UVToString(test_case.uv));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalUVAuthenticatorImplTest);
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
      options->timeout = base::TimeDelta::FromMilliseconds(100);
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
  EXPECT_TRUE(test_client_.collected_pin());
}

TEST_F(InternalUVAuthenticatorImplTest, MakeCredentialCryptotoken) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));

  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;
    EXPECT_EQ(AuthenticatorMakeCredential(
                  make_credential_options(
                      device::UserVerificationRequirement::kPreferred))
                  .status,
              AuthenticatorStatus::SUCCESS);
    // The credential should have been created over U2F.
    for (const auto& registration :
         virtual_device_factory_->mutable_state()->registrations) {
      EXPECT_TRUE(registration.second.is_u2f);
    }
  }
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
  EXPECT_TRUE(test_client_.collected_pin());
  EXPECT_TRUE(test_client_.did_bio_enrollment());
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->fingerprints_enrolled);
}

// Test making a credential skipping biometric enrollment during credential
// creation.
TEST_F(InternalUVAuthenticatorImplTest, MakeCredentialSkipInlineBioEnrollment) {
  test_client_.set_cancel_bio_enrollment(true);

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
  EXPECT_TRUE(test_client_.collected_pin());
  EXPECT_TRUE(test_client_.did_bio_enrollment());
  EXPECT_FALSE(virtual_device_factory_->mutable_state()->fingerprints_enrolled);
}

TEST_F(InternalUVAuthenticatorImplTest, GetAssertion) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

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
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  GetAssertionResult result = AuthenticatorGetAssertion(
      get_credential_options(device::UserVerificationRequirement::kRequired));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  EXPECT_TRUE(test_client_.collected_pin());
}

TEST_F(InternalUVAuthenticatorImplTest, GetAssertionCryptotoken) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;
    EXPECT_EQ(AuthenticatorGetAssertion(
                  get_credential_options(
                      device::UserVerificationRequirement::kPreferred))
                  .status,
              AuthenticatorStatus::SUCCESS);
  }
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
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;

    for (auto uv : {device::UserVerificationRequirement::kDiscouraged,
                    device::UserVerificationRequirement::kPreferred,
                    device::UserVerificationRequirement::kRequired}) {
      SCOPED_TRACE(UVToString(uv));

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
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

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
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  int expected_retries = 5;
  virtual_device_factory_->mutable_state()->uv_retries = expected_retries;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        EXPECT_EQ(--expected_retries,
                  virtual_device_factory_->mutable_state()->uv_retries);
        return true;
      });

  EXPECT_EQ(AuthenticatorGetAssertion(get_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(0, expected_retries);
  EXPECT_TRUE(test_client_.collected_pin());
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
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorGetAssertion(get_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(test_client_.collected_pin());
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
      SCOPED_TRACE(UVToString(uv));

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
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

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
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  int expected_retries = 5;
  virtual_device_factory_->mutable_state()->uv_retries = expected_retries;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        EXPECT_EQ(--expected_retries,
                  virtual_device_factory_->mutable_state()->uv_retries);
        return true;
      });

  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(0, expected_retries);
  EXPECT_TRUE(test_client_.collected_pin());
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
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(test_client_.collected_pin());
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->uv_retries);
}

// ResidentKeyTestAuthenticatorRequestDelegate is a delegate that:
//   a) always returns |kTestPIN| when asked for a PIN.
//   b) sorts potential resident-key accounts by user ID, maps them to a string
//      form ("<hex user ID>:<user name>:<display name>"), joins the strings
//      with "/", and compares the result against |expected_accounts|.
//   c) auto-selects the account with the user ID matching |selected_user_id|.
class ResidentKeyTestAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  ResidentKeyTestAuthenticatorRequestDelegate(
      std::string expected_accounts,
      std::vector<uint8_t> selected_user_id,
      bool* might_create_resident_credential,
      base::Optional<InterestingFailureReason>* failure_reason)
      : expected_accounts_(expected_accounts),
        selected_user_id_(selected_user_id),
        might_create_resident_credential_(might_create_resident_credential),
        failure_reason_(failure_reason) {}

  bool SupportsPIN() const override { return true; }

  void CollectPIN(
      base::Optional<int> attempts,
      base::OnceCallback<void(std::string)> provide_pin_cb) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(provide_pin_cb), kTestPIN));
  }

  void FinishCollectToken() override {}

  bool SupportsResidentKeys() override { return true; }

  void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback) override {
    std::sort(responses.begin(), responses.end(),
              [](const device::AuthenticatorGetAssertionResponse& a,
                 const device::AuthenticatorGetAssertionResponse& b) {
                return a.user_entity()->id < b.user_entity()->id;
              });

    std::vector<std::string> string_reps;
    std::transform(
        responses.begin(), responses.end(), std::back_inserter(string_reps),
        [](const device::AuthenticatorGetAssertionResponse& response) {
          const device::PublicKeyCredentialUserEntity& user =
              response.user_entity().value();
          return base::HexEncode(user.id.data(), user.id.size()) + ":" +
                 user.name.value_or("") + ":" + user.display_name.value_or("");
        });

    EXPECT_EQ(expected_accounts_, base::JoinString(string_reps, "/"));

    const auto selected = std::find_if(
        responses.begin(), responses.end(),
        [this](const device::AuthenticatorGetAssertionResponse& response) {
          return response.user_entity()->id == selected_user_id_;
        });
    ASSERT_TRUE(selected != responses.end());

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(*selected)));
  }

  void SetMightCreateResidentCredential(bool v) override {
    *might_create_resident_credential_ = v;
  }

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override {
    *failure_reason_ = reason;
    return AuthenticatorRequestClientDelegate::DoesBlockRequestOnFailure(
        reason);
  }

 private:
  const std::string expected_accounts_;
  const std::vector<uint8_t> selected_user_id_;
  bool* const might_create_resident_credential_;
  base::Optional<InterestingFailureReason>* const failure_reason_;
  DISALLOW_COPY_AND_ASSIGN(ResidentKeyTestAuthenticatorRequestDelegate);
};

class ResidentKeyTestAuthenticatorContentBrowserClient
    : public ContentBrowserClient {
 public:
  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<ResidentKeyTestAuthenticatorRequestDelegate>(
        expected_accounts, selected_user_id, &might_create_resident_credential,
        &failure_reason);
  }

  std::string expected_accounts;
  std::vector<uint8_t> selected_user_id;
  bool might_create_resident_credential = false;
  base::Optional<AuthenticatorRequestClientDelegate::InterestingFailureReason>
      failure_reason;
};

class ResidentKeyAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
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

 protected:
  ResidentKeyTestAuthenticatorContentBrowserClient test_client_;

  static PublicKeyCredentialCreationOptionsPtr make_credential_options(
      device::ResidentKeyRequirement resident_key =
          device::ResidentKeyRequirement::kRequired) {
    PublicKeyCredentialCreationOptionsPtr options =
        UVAuthenticatorImplTest::make_credential_options();
    options->authenticator_selection->SetResidentKeyForTesting(resident_key);
    options->user.id = {1, 2, 3, 4};
    return options;
  }

  static PublicKeyCredentialRequestOptionsPtr get_credential_options() {
    PublicKeyCredentialRequestOptionsPtr options =
        UVAuthenticatorImplTest::get_credential_options();
    options->allow_credentials.clear();
    return options;
  }

 private:
  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ResidentKeyAuthenticatorImplTest);
};

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialRkRequired) {
  for (const bool internal_uv : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "internal_uv=" << internal_uv);
    test_client_.might_create_resident_credential = false;

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
    EXPECT_TRUE(test_client_.might_create_resident_credential);
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
    EXPECT_EQ(options->user.icon_url, registration.user->icon_url);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialRkPreferred) {
  for (const bool supports_rk : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "supports_rk=" << supports_rk);
    ResetVirtualDevice();
    test_client_.might_create_resident_credential = false;

    device::VirtualCtap2Device::Config config;
    config.internal_uv_support = true;
    config.resident_key_support = supports_rk;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

    MakeCredentialResult result = AuthenticatorMakeCredential(
        make_credential_options(device::ResidentKeyRequirement::kPreferred));

    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(test_client_.might_create_resident_credential);
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
  ResetVirtualDevice();
  test_client_.might_create_resident_credential = false;

  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.resident_key_support = true;
  config.resident_credential_storage = 0;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

  MakeCredentialResult result = AuthenticatorMakeCredential(
      make_credential_options(device::ResidentKeyRequirement::kPreferred));

  ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(test_client_.might_create_resident_credential);
  EXPECT_TRUE(HasUV(result.response));
  ASSERT_EQ(1u, virtual_device_factory_->mutable_state()->registrations.size());
  const device::VirtualFidoDevice::RegistrationData& registration =
      virtual_device_factory_->mutable_state()->registrations.begin()->second;
  EXPECT_EQ(registration.is_resident, false);
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
  EXPECT_TRUE(test_client_.might_create_resident_credential);
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

  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(AuthenticatorRequestClientDelegate::InterestingFailureReason::
                kStorageFull,
            test_client_.failure_reason);
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionSingleNoPII) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, base::nullopt, base::nullopt));

  // |SelectAccount| should not be called when there's only a single response
  // with no identifying user info because the UI is bad in that case: we can
  // only display the single choice of "Unknown user".
  test_client_.expected_accounts = "<invalid>";
  GetAssertionResult result =
      AuthenticatorGetAssertion(get_credential_options());

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionSingleWithPII) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, base::nullopt, "Test User"));

  // |SelectAccount| should be called when PII is available.
  test_client_.expected_accounts = "01020304::Test User";
  test_client_.selected_user_id = {1, 2, 3, 4};
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

  test_client_.expected_accounts =
      "01020304:test@example.com:Test User/"
      "05060708:test2@example.com:Test User 2";
  test_client_.selected_user_id = {1, 2, 3, 4};

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
      /*user_id=*/{{1, 2, 3, 4}}, base::nullopt, base::nullopt));

  // |SelectAccount| should not be called when there's only a single response
  // without identifying information.
  test_client_.expected_accounts = "<invalid>";
  PublicKeyCredentialRequestOptionsPtr options(get_credential_options());
  options->user_verification =
      device::UserVerificationRequirement::kDiscouraged;

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  // The UV=discouraged should have been ignored for a resident-credential
  // request.
  EXPECT_TRUE(HasUV(result.response));
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

  const struct {
    bool supported_by_authenticator;
    bool is_resident;
    blink::mojom::ProtectionPolicy protection;
    bool enforce;
    bool uv;
    int expected_outcome;
    blink::mojom::ProtectionPolicy resulting_policy;
  } kExpectations[] = {
      // clang-format off
    // Support | Resdnt | Level      | Enf  |  UV  || Result   | Prot level
    {  false,   false,   UNSPECIFIED, false, false,   kOk,       NONE},
    {  false,   false,   UNSPECIFIED, true,  false,   kNonsense, UNSPECIFIED},
    {  false,   false,   NONE,        false, false,   kNonsense, UNSPECIFIED},
    {  false,   false,   NONE,        true,  false,   kNonsense, UNSPECIFIED},
    {  false,   false,   UV_OR_CRED,  false, false,   kOk,       NONE},
    {  false,   false,   UV_OR_CRED,  true,  false,   kNotAllow, UNSPECIFIED},
    {  false,   false,   UV_OR_CRED,  false, true,    kOk,       NONE},
    {  false,   false,   UV_OR_CRED,  true,  true,    kNotAllow, UNSPECIFIED},
    {  false,   false,   UV_REQ,      false, false,   kNonsense, UNSPECIFIED},
    {  false,   false,   UV_REQ,      false, true,    kOk,       NONE},
    {  false,   false,   UV_REQ,      true,  false,   kNonsense, UNSPECIFIED},
    {  false,   false,   UV_REQ,      true,  true,    kNotAllow, UNSPECIFIED},
    {  false,   true,    UNSPECIFIED, false, false,   kOk,       NONE},
    {  false,   true,    UNSPECIFIED, true,  false,   kNonsense, UNSPECIFIED},
    {  false,   true,    NONE,        false, false,   kOk,       NONE},
    {  false,   true,    NONE,        true,  false,   kNonsense, UNSPECIFIED},
    {  false,   true,    UV_OR_CRED,  false, false,   kOk,       NONE},
    {  false,   true,    UV_OR_CRED,  true,  false,   kNotAllow, UNSPECIFIED},
    {  false,   true,    UV_REQ,      false, false,   kNonsense, UNSPECIFIED},
    {  false,   true,    UV_REQ,      false, true,    kOk,       NONE},
    {  false,   true,    UV_REQ,      true,  false,   kNonsense, UNSPECIFIED},
    {  false,   true,    UV_REQ,      true,  true,    kNotAllow, UNSPECIFIED},

    // For the case where the authenticator supports credProtect we do not
    // repeat the cases above that are |kNonsense| on the assumption that
    // authenticator support is irrelevant. Therefore these are just the non-
    // kNonsense cases from the prior block.
    {  true,    false,   UNSPECIFIED, false, false,   kOk,       NONE},
    {  true,    false,   UV_OR_CRED,  false, false,   kOk,       UV_OR_CRED},
    {  true,    false,   UV_OR_CRED,  true,  false,   kOk,       UV_OR_CRED},
    {  true,    false,   UV_OR_CRED,  false, true,    kOk,       UV_OR_CRED},
    {  true,    false,   UV_OR_CRED,  true,  true,    kOk,       UV_OR_CRED},
    {  true,    false,   UV_REQ,      false, true,    kOk,       UV_REQ},
    {  true,    false,   UV_REQ,      true,  true,    kOk,       UV_REQ},
    {  true,    true,    UNSPECIFIED, false, false,   kOk,       UV_OR_CRED},
    {  true,    true,    NONE,        false, false,   kOk,       NONE},
    {  true,    true,    UV_OR_CRED,  false, false,   kOk,       UV_OR_CRED},
    {  true,    true,    UV_OR_CRED,  true,  false,   kOk,       UV_OR_CRED},
    {  true,    true,    UV_REQ,      false, true,    kOk,       UV_REQ},
    {  true,    true,    UV_REQ,      true,  true,    kOk,       UV_REQ},
      // clang-format on
  };

  for (const auto& test : kExpectations) {
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.resident_key_support = true;
    config.cred_protect_support = test.supported_by_authenticator;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->registrations.clear();

    SCOPED_TRACE(::testing::Message() << "uv=" << test.uv);
    SCOPED_TRACE(::testing::Message() << "enforce=" << test.enforce);
    SCOPED_TRACE(::testing::Message()
                 << "level=" << ProtectionPolicyDescription(test.protection));
    SCOPED_TRACE(::testing::Message() << "resident=" << test.is_resident);
    SCOPED_TRACE(::testing::Message()
                 << "support=" << test.supported_by_authenticator);

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
    options->authenticator_selection->SetResidentKeyForTesting(
        test.is_resident ? device::ResidentKeyRequirement::kRequired
                         : device::ResidentKeyRequirement::kDiscouraged);
    options->protection_policy = test.protection;
    options->enforce_protection_policy = test.enforce;
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        test.uv ? device::UserVerificationRequirement::kRequired
                : device::UserVerificationRequirement::kDiscouraged);

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
            break;
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
  constexpr blink::mojom::ProtectionPolicy kMojoLevels[] = {
      blink::mojom::ProtectionPolicy::NONE,
      blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED,
      blink::mojom::ProtectionPolicy::UV_REQUIRED,
  };
  constexpr device::CredProtect kDeviceLevels[] = {
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
      options->authenticator_selection->SetResidentKeyForTesting(
          device::ResidentKeyRequirement::kRequired);
      options->protection_policy = kMojoLevels[requested_level];
      options->authenticator_selection
          ->SetUserVerificationRequirementForTesting(
              device::UserVerificationRequirement::kRequired);

      AuthenticatorStatus status =
          AuthenticatorMakeCredential(std::move(options)).status;

      if (requested_level <= forced_level) {
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, status);
        ASSERT_EQ(
            1u, virtual_device_factory_->mutable_state()->registrations.size());
        const base::Optional<device::CredProtect> result =
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
    options->authenticator_selection->SetResidentKeyForTesting(
        device::ResidentKeyRequirement::kRequired);
    options->protection_policy = test.requested_level;
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        device::UserVerificationRequirement::kRequired);

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
  test_client_.expected_accounts = "<invalid>";

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  options->allow_credentials = GetTestCredentials(5);
  options->allow_credentials[0].GetIdForTesting() = {4, 3, 2, 1};

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
      /*user_id=*/{{1, 2, 3, 4}}, base::nullopt, base::nullopt));

  // |SelectAccount| should not be called when there's only a single response
  // without identifying information.
  test_client_.expected_accounts = "<invalid>";

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  options->appid = kTestOrigin1;

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

#if defined(OS_WIN)
// Requests with a credProtect extension that have |enforce_protection_policy|
// set should be rejected if the Windows WebAuthn API doesn't support
// credProtect.
TEST_F(ResidentKeyAuthenticatorImplTest, WinCredProtectApiVersion) {
  // The canned response returned by the Windows API fake is for acme.com.
  NavigateAndCommit(GURL("https://acme.com"));
  for (const bool supports_cred_protect : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "supports_cred_protect: " << supports_cred_protect);

    ::device::FakeWinWebAuthnApi api;
    virtual_device_factory_->set_win_webauthn_api(&api);
    api.set_version(supports_cred_protect ? WEBAUTHN_API_VERSION_2
                                          : WEBAUTHN_API_VERSION_1);

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
    options->relying_party = device::PublicKeyCredentialRpEntity();
    options->relying_party.id = device::test_data::kRelyingPartyId;
    options->relying_party.name = "";
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        device::UserVerificationRequirement::kRequired);
    options->authenticator_selection->SetResidentKeyForTesting(
        device::ResidentKeyRequirement::kRequired);
    options->protection_policy =
        blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED;
    options->enforce_protection_policy = true;

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              supports_cred_protect ? AuthenticatorStatus::SUCCESS
                                    : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}
#endif  // defined(OS_WIN)

TEST_F(ResidentKeyAuthenticatorImplTest, PRFExtension) {
  NavigateAndCommit(GURL(kTestOrigin1));

  base::Optional<device::PublicKeyCredentialDescriptor> credential;
  for (bool hmac_secret_supported : {false, true}) {
    // Setting the PRF extension on an authenticator that doesn't support it
    // should cause the extension to be echoed, but with enabled=false.
    // Otherwise, enabled should be true.
    device::VirtualCtap2Device::Config config;
    config.hmac_secret_support = hmac_secret_supported;
    config.max_credential_count_in_list = 3;
    config.max_credential_id_length = 256;
    config.pin_support = true;
    config.resident_key_support = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->prf_enable = true;
    options->authenticator_selection->SetResidentKeyForTesting(
        hmac_secret_supported ? device::ResidentKeyRequirement::kRequired
                              : device::ResidentKeyRequirement::kDiscouraged);
    options->user.id = {1, 2, 3, 4};
    options->user.name = "name";
    options->user.display_name = "displayName";
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);

    ASSERT_TRUE(result.response->echo_prf);
    ASSERT_EQ(result.response->prf, hmac_secret_supported);

    if (hmac_secret_supported) {
      device::AuthenticatorData auth_data =
          AuthDataFromMakeCredentialResponse(result.response);
      credential.emplace(device::CredentialType::kPublicKey,
                         auth_data.GetCredentialId());
    }
  }

  auto assertion = [&](std::vector<blink::mojom::PRFValuesPtr> inputs,
                       unsigned allow_list_size =
                           1) -> blink::mojom::PRFValuesPtr {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->prf = true;
    options->prf_inputs = std::move(inputs);
    options->allow_credentials.clear();
    if (allow_list_size >= 1) {
      for (unsigned i = 0; i < allow_list_size - 1; i++) {
        std::vector<uint8_t> random_credential_id(32, static_cast<uint8_t>(i));
        options->allow_credentials.emplace_back(
            device::CredentialType::kPublicKey,
            std::move(random_credential_id));
      }
      options->allow_credentials.push_back(*credential);
    }

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    CHECK(result.response->prf_results);
    CHECK(!result.response->prf_results->id);
    return std::move(result.response->prf_results);
  };

  const std::vector<uint8_t> salt1(32, 1);
  const std::vector<uint8_t> salt2(32, 2);

  auto prf_value = blink::mojom::PRFValues::New();
  prf_value->first = salt1;
  std::vector<blink::mojom::PRFValuesPtr> inputs;
  inputs.emplace_back(std::move(prf_value));
  auto result = assertion(std::move(inputs));
  const std::vector<uint8_t> salt1_eval = std::move(result->first);

  // The result should be consistent
  prf_value = blink::mojom::PRFValues::New();
  prf_value->first = salt1;
  inputs.emplace_back(std::move(prf_value));
  result = assertion(std::move(inputs));
  ASSERT_EQ(result->first, salt1_eval);

  // Should be able to evaluate two points at once.
  prf_value = blink::mojom::PRFValues::New();
  prf_value->first = salt1;
  prf_value->second = salt2;
  inputs.emplace_back(std::move(prf_value));
  result = assertion(std::move(inputs));
  ASSERT_EQ(result->first, salt1_eval);
  ASSERT_TRUE(result->second);
  const std::vector<uint8_t> salt2_eval = std::move(*result->second);
  ASSERT_NE(salt1_eval, salt2_eval);

  // Should be consistent if swapped.
  prf_value = blink::mojom::PRFValues::New();
  prf_value->first = salt2;
  prf_value->second = salt1;
  inputs.emplace_back(std::move(prf_value));
  result = assertion(std::move(inputs));
  ASSERT_EQ(result->first, salt2_eval);
  ASSERT_TRUE(result->second);
  ASSERT_EQ(*result->second, salt1_eval);

  // Should still trigger if the credential ID is specified
  prf_value = blink::mojom::PRFValues::New();
  prf_value->id.emplace(credential->id());
  prf_value->first = salt1;
  prf_value->second = salt2;
  inputs.emplace_back(std::move(prf_value));
  result = assertion(std::move(inputs));
  ASSERT_EQ(result->first, salt1_eval);
  ASSERT_TRUE(result->second);
  ASSERT_EQ(*result->second, salt2_eval);

  // And the specified credential ID should override any default inputs.
  prf_value = blink::mojom::PRFValues::New();
  prf_value->first = std::vector<uint8_t>(32, 3);
  inputs.emplace_back(std::move(prf_value));
  prf_value = blink::mojom::PRFValues::New();
  prf_value->id.emplace(credential->id());
  prf_value->first = salt1;
  prf_value->second = salt2;
  inputs.emplace_back(std::move(prf_value));
  result = assertion(std::move(inputs));
  ASSERT_EQ(result->first, salt1_eval);
  ASSERT_TRUE(result->second);
  ASSERT_EQ(*result->second, salt2_eval);

  // ... and that should still be true if there there are lots of dummy entries
  // in the allowlist. Note that the virtual authenticator was configured such
  // that this will cause multiple batches.
  prf_value = blink::mojom::PRFValues::New();
  prf_value->id.emplace(credential->id());
  prf_value->first = salt1;
  prf_value->second = salt2;
  inputs.emplace_back(std::move(prf_value));
  result = assertion(std::move(inputs), /*allowlist_size=*/20);
  ASSERT_EQ(result->first, salt1_eval);
  ASSERT_TRUE(result->second);
  ASSERT_EQ(*result->second, salt2_eval);

  // Default PRF values should be passed down when the allowlist is empty.
  prf_value = blink::mojom::PRFValues::New();
  prf_value->first = salt1;
  prf_value->second = salt2;
  inputs.emplace_back(std::move(prf_value));
  test_client_.expected_accounts = "01020304:name:displayName";
  test_client_.selected_user_id = {1, 2, 3, 4};
  result = assertion(std::move(inputs), /*allowlist_size=*/0);
  ASSERT_EQ(result->first, salt1_eval);
  ASSERT_TRUE(result->second);
  ASSERT_EQ(*result->second, salt2_eval);

  // And the default PRF values should be used if none of the specific values
  // match.
  prf_value = blink::mojom::PRFValues::New();
  prf_value->first = salt1;
  inputs.emplace_back(std::move(prf_value));
  prf_value = blink::mojom::PRFValues::New();
  prf_value->first = std::vector<uint8_t>(32, 3);
  prf_value->id = std::vector<uint8_t>(32, 4);
  inputs.emplace_back(std::move(prf_value));
  result = assertion(std::move(inputs), /*allowlist_size=*/20);
  ASSERT_EQ(result->first, salt1_eval);
  ASSERT_FALSE(result->second);
}

class InternalAuthenticatorImplTest : public AuthenticatorTestBase {
 protected:
  InternalAuthenticatorImplTest() = default;

  void TearDown() override {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    internal_authenticator_impl_.reset();
    AuthenticatorTestBase::TearDown();
  }

  void NavigateAndCommit(const GURL& url) {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    internal_authenticator_impl_.reset();
    content::RenderViewHostTestHarness::NavigateAndCommit(url);
  }

  InternalAuthenticatorImpl* GetAuthenticator(
      const url::Origin& effective_origin_url) {
    internal_authenticator_impl_ =
        std::make_unique<InternalAuthenticatorImpl>(main_rfh());
    internal_authenticator_impl_->SetEffectiveOrigin(effective_origin_url);
    return internal_authenticator_impl_.get();
  }

 protected:
  std::unique_ptr<InternalAuthenticatorImpl> internal_authenticator_impl_;
};

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(InternalAuthenticatorImplTest, MakeCredentialOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (auto test_case : kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    GURL origin = GURL(test_case.origin);
    if (url::Origin::Create(origin).opaque()) {
      // Opaque origins will cause DCHECK to fail.
      continue;
    }

    NavigateAndCommit(origin);
    InternalAuthenticatorImpl* authenticator =
        GetAuthenticator(url::Origin::Create(origin));
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(test_case.expected_status, callback_receiver.status());
  }

  // These instances should bypass security errors, by setting the effective
  // origin to a valid one.
  for (auto test_case : kValidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL("https://this.isthewrong.origin"));
    auto* authenticator =
        GetAuthenticator(url::Origin::Create(GURL(test_case.origin)));
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;

    ResetVirtualDevice();
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(test_case.expected_status, callback_receiver.status());
  }
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(InternalAuthenticatorImplTest, GetAssertionOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (const OriginClaimedAuthorityPair& test_case :
       kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    GURL origin = GURL(test_case.origin);
    if (url::Origin::Create(origin).opaque()) {
      // Opaque origins will cause DCHECK to fail.
      continue;
    }

    NavigateAndCommit(origin);
    InternalAuthenticatorImpl* authenticator =
        GetAuthenticator(url::Origin::Create(origin));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(test_case.expected_status, callback_receiver.status());
  }

  // These instances should bypass security errors, by setting the effective
  // origin to a valid one.
  for (const OriginClaimedAuthorityPair& test_case :
       kValidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL("https://this.isthewrong.origin"));
    InternalAuthenticatorImpl* authenticator =
        GetAuthenticator(url::Origin::Create(GURL(test_case.origin)));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    ResetVirtualDevice();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), test_case.claimed_authority));
    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(test_case.expected_status, callback_receiver.status());
  }
}

#if defined(OS_MAC)
class TouchIdConfigAuthenticatorRequestClientDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  TouchIdConfigAuthenticatorRequestClientDelegate() = default;
  ~TouchIdConfigAuthenticatorRequestClientDelegate() override = default;

  base::Optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig()
      override {
    return TouchIdAuthenticatorConfig{};
  }
};

class TouchIdConfigAuthenticatorContentBrowserClient
    : public ContentBrowserClient {
 public:
  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<TouchIdConfigAuthenticatorRequestClientDelegate>();
  }
};

class TouchIdAuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

 private:
  TouchIdConfigAuthenticatorContentBrowserClient test_client_;
  ContentBrowserClient* old_client_ = nullptr;
};

TEST_F(TouchIdAuthenticatorImplTest, IsUVPAA) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  if (__builtin_available(macOS 10.12.2, *)) {
    for (const bool touch_id_available : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "touch_id_available=" << touch_id_available);
      device::fido::mac::ScopedTouchIdTestEnvironment touch_id_test_environment;
      touch_id_test_environment.SetTouchIdAvailable(touch_id_available);
      TestIsUvpaaCallback cb;
      authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(
          cb.callback());
      cb.WaitForCallback();
      EXPECT_EQ(touch_id_available, cb.value());
    }
  }
}
#endif  // defined(OS_MAC)

class CableV2AuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  CableV2AuthenticatorImplTest()
      : network_context_(device::cablev2::NewMockTunnelServer(
            base::BindRepeating(&CableV2AuthenticatorImplTest::OnContact,
                                base::Unretained(this)))) {}

  void SetUp() override {
    AuthenticatorImplTest::SetUp();

    EnableFeature(features::kWebAuthCable);
    EnableFeature(device::kWebAuthPhoneSupport);
    NavigateAndCommit(GURL(kTestOrigin1));

    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    bssl::UniquePtr<EC_KEY> peer_identity(EC_KEY_derive_from_secret(
        p256.get(), zero_seed_.data(), zero_seed_.size()));
    CHECK_EQ(sizeof(peer_identity_x962_),
             EC_POINT_point2oct(
                 p256.get(), EC_KEY_get0_public_key(peer_identity.get()),
                 POINT_CONVERSION_UNCOMPRESSED, peer_identity_x962_,
                 sizeof(peer_identity_x962_), /*ctx=*/nullptr));
  }

  base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
  GetPairingCallback() {
    return base::BindRepeating(&CableV2AuthenticatorImplTest::OnPairing,
                               base::Unretained(this));
  }

 protected:
  class DiscoveryFactory : public device::FidoDiscoveryFactory {
   public:
    explicit DiscoveryFactory(
        std::unique_ptr<device::cablev2::Discovery> discovery)
        : discovery_(std::move(discovery)) {}

    std::vector<std::unique_ptr<device::FidoDiscoveryBase>> Create(
        device::FidoTransportProtocol transport) override {
      if (transport !=
              device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy ||
          !discovery_) {
        return {};
      }

      return SingleDiscovery(std::move(discovery_));
    }

   private:
    std::unique_ptr<device::cablev2::Discovery> discovery_;
  };

  void OnContact(
      base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
      base::span<const uint8_t> pairing_id,
      base::span<const uint8_t, device::cablev2::kClientNonceSize>
          client_nonce) {
    std::move(contact_callback_).Run(tunnel_id, pairing_id, client_nonce);
  }

  void OnPairing(std::unique_ptr<device::cablev2::Pairing> pairing) {
    pairings_.emplace_back(std::move(pairing));
  }

  const std::array<uint8_t, device::cablev2::kRootSecretSize> root_secret_ = {
      0};
  const std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key_ = {
      0};
  const std::array<uint8_t, device::cablev2::kQRSecretSize> zero_qr_secret_ = {
      0};
  const std::array<uint8_t, device::cablev2::kQRSeedSize> zero_seed_ = {0};

  std::unique_ptr<network::mojom::NetworkContext> network_context_;
  uint8_t peer_identity_x962_[device::kP256X962Length] = {0};
  device::VirtualCtap2Device virtual_device_;
  std::vector<std::unique_ptr<device::cablev2::Pairing>> pairings_;
  base::OnceCallback<void(
      base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
      base::span<const uint8_t> pairing_id,
      base::span<const uint8_t, device::cablev2::kClientNonceSize>
          client_nonce)>
      contact_callback_;
};

TEST_F(CableV2AuthenticatorImplTest, QRBasedWithNoPairing) {
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      network_context_.get(), qr_generator_key_,
      /*pairings=*/std::vector<std::unique_ptr<device::cablev2::Pairing>>(),
      GetPairingCallback());
  auto* const discovery_ptr = discovery.get();

  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<DiscoveryFactory>(std::move(discovery)));

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          device::cablev2::authenticator::NewMockPlatform(discovery_ptr,
                                                          &virtual_device_),
          network_context_.get(), root_secret_, "Test Authenticator",
          zero_qr_secret_, peer_identity_x962_,
          /*contact_id=*/base::nullopt, base::DoNothing());

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(pairings_.size(), 0u);
}

TEST_F(CableV2AuthenticatorImplTest, PairingBased) {
  // First do unpaired exchange to get pairing data.
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      network_context_.get(), qr_generator_key_,
      /*pairings=*/std::vector<std::unique_ptr<device::cablev2::Pairing>>(),
      GetPairingCallback());
  auto* discovery_ptr = discovery.get();

  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<DiscoveryFactory>(std::move(discovery)));

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          device::cablev2::authenticator::NewMockPlatform(discovery_ptr,
                                                          &virtual_device_),
          network_context_.get(), root_secret_, "Test Authenticator",
          zero_qr_secret_, peer_identity_x962_,
          /*contact_id=*/std::vector<uint8_t>({1, 2, 3}), base::DoNothing());

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(pairings_.size(), 1u);

  // Now do a pairing-based exchange.
  discovery = std::make_unique<device::cablev2::Discovery>(
      network_context_.get(), qr_generator_key_, std::move(pairings_),
      GetPairingCallback());
  discovery_ptr = discovery.get();

  const std::array<uint8_t, device::cablev2::kRoutingIdSize> routing_id = {0};
  bool contact_callback_was_called = false;
  // When the |cablev2::Discovery| starts it'll make a connection to the tunnel
  // service with the contact ID from the pairing data. This will be handled by
  // the |TestNetworkContext| and turned into a call to |contact_callback_|.
  // This simulates the tunnel server sending a cloud message to a phone. Given
  // the information from the connection, a transaction can be created.
  contact_callback_ = base::BindLambdaForTesting(
      [this, &transaction, discovery_ptr, routing_id,
       &contact_callback_was_called](
          base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
          base::span<const uint8_t> pairing_id,
          base::span<const uint8_t, device::cablev2::kClientNonceSize>
              client_nonce) -> void {
        contact_callback_was_called = true;
        transaction = device::cablev2::authenticator::TransactFromFCM(
            device::cablev2::authenticator::NewMockPlatform(discovery_ptr,
                                                            &virtual_device_),
            network_context_.get(), root_secret_, routing_id, tunnel_id,
            pairing_id, client_nonce, base::DoNothing());
      });

  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<DiscoveryFactory>(std::move(discovery)));

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(contact_callback_was_called);
}

}  // namespace content
