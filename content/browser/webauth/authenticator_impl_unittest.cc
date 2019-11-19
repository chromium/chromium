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

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/json/json_parser.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
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
#include "content/public/test/test_service_manager_context.h"
#include "content/test/test_render_frame_host.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

#if defined(OS_MACOSX)
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

typedef struct {
  const char* origin;
  // Either a relying party ID or a U2F AppID.
  const char* claimed_authority;
  AuthenticatorStatus expected_status;
} OriginClaimedAuthorityPair;

// The size of credential IDs returned by GetTestCredentials().
constexpr size_t kTestCredentialIdLength = 32u;

constexpr char kTestOrigin1[] = "https://a.google.com";
constexpr char kTestOrigin2[] = "https://acme.org";
constexpr char kTestRelyingPartyId[] = "google.com";
constexpr char kCryptotokenOrigin[] =
    "chrome-extension://kmendfapggjehodndflmmgagdbamhnfd";
constexpr char kTestExtensionOrigin[] =
    "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef";

// Test data. CBOR test data can be built using the given
// diagnostic strings and the utility at "http://CBOR.me/".
constexpr int32_t kCoseEs256 = -7;

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
      device::AuthenticatorAttachment::kAny, false,
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
  options->public_key_parameters =
      GetTestPublicKeyCredentialParameters(kCoseEs256);
  options->challenge.assign(32, 0x0A);
  options->adjusted_timeout = base::TimeDelta::FromMinutes(1);
  options->authenticator_selection = GetTestAuthenticatorSelectionCriteria();
  return options;
}

PublicKeyCredentialRequestOptionsPtr
GetTestPublicKeyCredentialRequestOptions() {
  auto options = PublicKeyCredentialRequestOptions::New();
  options->relying_party_id = std::string(kTestRelyingPartyId);
  options->challenge.assign(32, 0x0A);
  options->adjusted_timeout = base::TimeDelta::FromMinutes(1);
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

}  // namespace

class AuthenticatorTestBase : public content::RenderViewHostTestHarness {
 protected:
  AuthenticatorTestBase() = default;
  ~AuthenticatorTestBase() override {}

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
  ~AuthenticatorImplTest() override {}

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

  // Simulates navigating to a page and getting the page contents and language
  // for that navigation.
  void SimulateNavigation(const GURL& url) {
    if (main_rfh()->GetLastCommittedURL() != url)
      NavigateAndCommit(url);
  }

  mojo::Remote<blink::mojom::Authenticator> ConnectToAuthenticator() {
    authenticator_impl_ = std::make_unique<AuthenticatorImpl>(main_rfh());
    mojo::Remote<blink::mojom::Authenticator> authenticator;
    authenticator_impl_->Bind(authenticator.BindNewPipeAndPassReceiver());
    return authenticator;
  }

  mojo::Remote<blink::mojom::Authenticator> ConnectToAuthenticator(
      service_manager::Connector* connector,
      std::unique_ptr<base::OneShotTimer> timer) {
    authenticator_impl_.reset(new AuthenticatorImpl(
        main_rfh(), std::make_unique<AuthenticatorCommon>(main_rfh(), connector,
                                                          std::move(timer))));
    mojo::Remote<blink::mojom::Authenticator> authenticator;
    authenticator_impl_->Bind(authenticator.BindNewPipeAndPassReceiver());
    return authenticator;
  }

  mojo::Remote<blink::mojom::Authenticator> ConstructAuthenticatorWithTimer(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner) {
    connector_ = service_manager::Connector::Create(&receiver_);
    fake_hid_manager_ = std::make_unique<device::FakeFidoHidManager>();
    connector_->OverrideBinderForTesting(
        service_manager::ServiceFilter::ByName(device::mojom::kServiceName),
        device::mojom::HidManager::Name_,
        base::Bind(&device::FakeFidoHidManager::AddReceiver,
                   base::Unretained(fake_hid_manager_.get())));

    // Set up a timer for testing.
    auto timer =
        std::make_unique<base::OneShotTimer>(task_runner->GetMockTickClock());
    timer->SetTaskRunner(task_runner);
    return ConnectToAuthenticator(connector_.get(), std::move(timer));
  }

  url::Origin GetTestOrigin() {
    const GURL test_relying_party_url(kTestOrigin1);
    CHECK(test_relying_party_url.is_valid());
    return url::Origin::Create(test_relying_party_url);
  }

  std::string GetTestClientDataJSON(std::string type) {
    return AuthenticatorCommon::SerializeCollectedClientDataToJson(
        std::move(type), GetTestOrigin().Serialize(), GetTestChallengeBytes());
  }

  AuthenticatorStatus TryAuthenticationWithAppId(const std::string& origin,
                                                 const std::string& appid) {
    const GURL origin_url(origin);
    NavigateAndCommit(origin_url);
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = origin_url.host();
    options->appid = appid;

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();

    return callback_receiver.status();
  }

  AuthenticatorStatus TryRegistrationWithAppIdExclude(
      const std::string& origin,
      const std::string& appid_exclude) {
    const GURL origin_url(origin);
    NavigateAndCommit(origin_url);
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = origin_url.host();
    options->appid_exclude = appid_exclude;

    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();

    return callback_receiver.status();
  }

  bool SupportsTransportProtocol(::device::FidoTransportProtocol protocol) {
    return base::Contains(
        authenticator_impl_->get_authenticator_common_for_testing()
            ->enabled_transports_for_testing(),
        protocol);
  }

  void SetTransports(base::flat_set<device::FidoTransportProtocol> transports) {
    authenticator_impl_->get_authenticator_common_for_testing()
        ->set_transports_for_testing(transports);
  }

  void EnableFeature(const base::Feature& feature) {
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitAndEnableFeature(feature);
  }

  void DisableFeature(const base::Feature& feature) {
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitAndDisableFeature(feature);
  }

  // SetUpMockBluetooth returns a |unique_ptr| that must be held by the caller
  // as the testing overrides disappear when it's destroyed.
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalValuesForTesting>
  SetUpMockBluetooth() WARN_UNUSED_RESULT {
    mock_adapter_ = base::MakeRefCounted<
        ::testing::NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
    auto bluetooth_adapter_factory_overrides =
        device::BluetoothAdapterFactory::Get().InitGlobalValuesForTesting();
    bluetooth_adapter_factory_overrides->SetLESupported(true);
    return bluetooth_adapter_factory_overrides;
  }

 protected:
  std::unique_ptr<AuthenticatorImpl> authenticator_impl_;
  mojo::PendingReceiver<service_manager::mojom::Connector> receiver_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<device::FakeFidoHidManager> fake_hid_manager_;
  base::Optional<base::test::ScopedFeatureList> scoped_feature_list_;
  scoped_refptr<::testing::NiceMock<device::MockBluetoothAdapter>>
      mock_adapter_;
};

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, MakeCredentialOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (auto test_case : kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(test_case.expected_status, callback_receiver.status());
  }

  // These instances time out with NOT_ALLOWED_ERROR due to unsupported
  // algorithm.
  for (auto test_case : kValidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;
    options->public_key_parameters = GetTestPublicKeyCredentialParameters(123);

    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    // Trigger timer.
    base::RunLoop().RunUntilIdle();
    task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              callback_receiver.status());
  }
}

// Test that MakeCredential request returns INVALID_ICON_URL if the RP or user
// icon URLs are not a priori-authenticated URLs.
TEST_F(AuthenticatorImplTest, MakeCredentialInvalidIconUrl) {
  SimulateNavigation(GURL(kTestOrigin1));
  const GURL kInvalidIconUrlTestCases[] = {
      GURL("http://insecure-origin.com/kitten.png"),
      GURL("invalid:/url"),
  };

  // Test relying party icons.
  for (auto test_case : kInvalidIconUrlTestCases) {
    SCOPED_TRACE(test_case.possibly_invalid_spec());
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.icon_url = test_case;
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::INVALID_ICON_URL,
              callback_receiver.status());
  }

  // Test user icons.
  for (auto test_case : kInvalidIconUrlTestCases) {
    SCOPED_TRACE(test_case.possibly_invalid_spec());
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->user.icon_url = test_case;
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::INVALID_ICON_URL,
              callback_receiver.status());
  }
}

// Test that MakeCredential request does not return INVALID_ICON_URL for a
// priori-authenticated URLs.
TEST_F(AuthenticatorImplTest, MakeCredentialValidIconUrl) {
  const GURL kValidUrlTestCases[] = {
      GURL(),
      GURL("https://secure-origin.com/kitten.png"),
      GURL("about:blank"),
      GURL("about:srcdoc"),
      GURL("data:image/"
           "png;base64,"
           "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAACXBIWXMAAC4jAAAuIwF"
           "4pT92AAAAB3RJTUUH4wYUETEs5V5U8gAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdG"
           "ggR0lNUFeBDhcAAABGSURBVCjPY/z//"
           "z8DKYAJmcPYyICHi0UDyTYMDg2MFIUSnsAZAp5mbGT4X49DBcxLEAUsBMxrRCiFABb8"
           "gYNpLTXiAT8AAEeHFZvhj9g8AAAAAElFTkSuQmCC"),
  };
  SimulateNavigation(GURL(kTestOrigin1));

  // Test relying party icons.
  for (auto test_case : kValidUrlTestCases) {
    SCOPED_TRACE(test_case.possibly_invalid_spec());
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->public_key_parameters = GetTestPublicKeyCredentialParameters(123);
    options->relying_party.icon_url = test_case;

    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    // Trigger timer.
    base::RunLoop().RunUntilIdle();
    task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              callback_receiver.status());
  }

  // Test user icons.
  for (auto test_case : kValidUrlTestCases) {
    SCOPED_TRACE(test_case.possibly_invalid_spec());
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->public_key_parameters = GetTestPublicKeyCredentialParameters(123);
    options->user.icon_url = test_case;

    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    // Trigger timer.
    base::RunLoop().RunUntilIdle();
    task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              callback_receiver.status());
  }
}

// Test that MakeCredential request times out with NOT_ALLOWED_ERROR if no
// parameters contain a supported algorithm.
TEST_F(AuthenticatorImplTest, MakeCredentialNoSupportedAlgorithm) {
  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->public_key_parameters = GetTestPublicKeyCredentialParameters(123);

  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());
  // Trigger timer.
  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
}

// Test that MakeCredential request times out with NOT_ALLOWED_ERROR if user
// verification is required for U2F devices.
TEST_F(AuthenticatorImplTest, MakeCredentialUserVerification) {
  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->SetUserVerificationRequirementForTesting(
      device::UserVerificationRequirement::kRequired);

  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());
  // Trigger timer.
  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
}

// Test that MakeCredential request returns if resident
// key is requested on create().
TEST_F(AuthenticatorImplTest, MakeCredentialResidentKey) {
  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->SetRequireResidentKeyForTesting(true);

  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());
  // Trigger timer.
  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED,
            callback_receiver.status());

  // TODO add CTAP device
}

// Test that MakeCredential request times out with NOT_ALLOWED_ERROR if a
// platform authenticator is requested for U2F devices.
TEST_F(AuthenticatorImplTest, MakeCredentialPlatformAuthenticator) {
  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->SetAuthenticatorAttachmentForTesting(
      device::AuthenticatorAttachment::kPlatform);

  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());
  // Trigger timer.
  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
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
TEST_F(AuthenticatorImplTest, TestSerializedRegisterClientData) {
  CheckJSONIsSubsetOfJSON(kTestRegisterClientDataJsonString,
                          GetTestClientDataJSON(client_data::kCreateType));
}

TEST_F(AuthenticatorImplTest, TestSerializedSignClientData) {
  CheckJSONIsSubsetOfJSON(kTestSignClientDataJsonString,
                          GetTestClientDataJSON(client_data::kGetType));
}

TEST_F(AuthenticatorImplTest, TestMakeCredentialTimeout) {
  // The VirtualFidoAuthenticator simulates a tap immediately after it gets the
  // request. Replace by the real discovery that will wait until timeout.
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<device::FidoDiscoveryFactory>());
  SimulateNavigation(GURL(kTestOrigin1));
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  TestMakeCredentialCallback callback_receiver;

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());

  // Trigger timer.
  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
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
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(test_case.expected_status, callback_receiver.status());
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
  TestServiceManagerContext smc;

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
  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  {
    OverrideLastCommittedOrigin(main_rfh(),
                                url::Origin::Create(GURL(kCryptotokenOrigin)));
    // First, verify that the Cryptotoken request succeeds with the appid.
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = std::string(kTestOrigin1);

    // Inject a registration for the URL (which is a U2F AppID).
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestOrigin1));

    options->appid = kTestOrigin1;

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    ASSERT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());

    EXPECT_EQ(true, callback_receiver.value()->echo_appid_extension);
    EXPECT_EQ(true, callback_receiver.value()->appid_extension);
  }

  {
    ResetVirtualDevice();
    OverrideLastCommittedOrigin(
        main_rfh(), url::Origin::Create(GURL(kTestExtensionOrigin)));
    // Next, verify that other extensions cannot bypass the origin checks.
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = std::string(kTestOrigin1);

    // Inject a registration for the URL (which is a U2F AppID).
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestOrigin1));

    options->appid = kTestOrigin1;

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    ASSERT_EQ(AuthenticatorStatus::INVALID_DOMAIN, callback_receiver.status());
  }
}

// Requests originating from cryptotoken should only target U2F devices.
TEST_F(AuthenticatorImplTest, CryptoTokenU2fOnly) {
  TestServiceManagerContext smc;
  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  // TODO(martinkr): VirtualFidoDeviceFactory does not offer devices that
  // support both U2F and CTAP yet; we should test those.
  for (const bool u2f_authenticator : {true, false}) {
    SCOPED_TRACE(u2f_authenticator ? "U2F" : "CTAP");
    OverrideLastCommittedOrigin(main_rfh(),
                                url::Origin::Create(GURL(kCryptotokenOrigin)));

    virtual_device_factory_->SetSupportedProtocol(
        u2f_authenticator ? device::ProtocolVersion::kU2f
                          : device::ProtocolVersion::kCtap2);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());

    base::RunLoop().RunUntilIdle();
    task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
    callback_receiver.WaitForCallback();

    EXPECT_EQ((u2f_authenticator ? AuthenticatorStatus::SUCCESS
                                 : AuthenticatorStatus::NOT_ALLOWED_ERROR),
              callback_receiver.status());
  }
}

// Test that Cryptotoken requests should only be dispatched to USB
// authenticators.
TEST_F(AuthenticatorImplTest, CryptotokenUsbOnly) {
  TestServiceManagerContext smc;
  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  SetTransports(device::GetAllTransportProtocols());
  auto bluetooth_values = SetUpMockBluetooth();

  for (const bool is_cryptotoken_request : {false, true}) {
    // caBLE and platform discoveries cannot be instantiated through
    // VirtualFidoDeviceFactory, so we don't test them here.
    for (const device::FidoTransportProtocol transport :
         {device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
          device::FidoTransportProtocol::kBluetoothLowEnergy,
          device::FidoTransportProtocol::kNearFieldCommunication}) {
      SCOPED_TRACE(::testing::Message()
                   << "is_cryptotoken_request=" << is_cryptotoken_request
                   << ", transport=" << device::ToString(transport));

      OverrideLastCommittedOrigin(
          main_rfh(),
          url::Origin::Create(GURL(is_cryptotoken_request ? kCryptotokenOrigin
                                                          : kTestOrigin1)));

      ResetVirtualDevice();
      virtual_device_factory_->SetSupportedProtocol(
          device::ProtocolVersion::kU2f);
      virtual_device_factory_->SetTransport(transport);
      virtual_device_factory_->mutable_state()->transport = transport;

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      TestMakeCredentialCallback callback_receiver;
      authenticator->MakeCredential(std::move(options),
                                    callback_receiver.callback());
      base::RunLoop().RunUntilIdle();
      task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
      callback_receiver.WaitForCallback();
      EXPECT_EQ(
          !is_cryptotoken_request ||
                  transport ==
                      device::FidoTransportProtocol::kUsbHumanInterfaceDevice
              ? AuthenticatorStatus::SUCCESS
              : AuthenticatorStatus::NOT_ALLOWED_ERROR,
          callback_receiver.status());
    }
  }
}

// Requests originating from cryptotoken should only target U2F devices.
TEST_F(AuthenticatorImplTest, AttestationPermitted) {
  TestServiceManagerContext smc;
  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  // TODO(martinkr): VirtualFidoDeviceFactory does not offer devices that
  // support both U2F and CTAP yet; we should test those.
  for (const bool u2f_authenticator : {true, false}) {
    SCOPED_TRACE(u2f_authenticator ? "U2F" : "CTAP");
    OverrideLastCommittedOrigin(main_rfh(),
                                url::Origin::Create(GURL(kCryptotokenOrigin)));

    virtual_device_factory_->SetSupportedProtocol(
        u2f_authenticator ? device::ProtocolVersion::kU2f
                          : device::ProtocolVersion::kCtap2);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());

    base::RunLoop().RunUntilIdle();
    task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
    callback_receiver.WaitForCallback();

    EXPECT_EQ((u2f_authenticator ? AuthenticatorStatus::SUCCESS
                                 : AuthenticatorStatus::NOT_ALLOWED_ERROR),
              callback_receiver.status());
  }
}

// Verify that a credential registered with U2F can be used via webauthn.
TEST_F(AuthenticatorImplTest, AppIdExtension) {
  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

  {
    // First, test that the appid extension isn't echoed at all when not
    // requested.
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestRelyingPartyId));

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    ASSERT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());

    EXPECT_EQ(false, callback_receiver.value()->echo_appid_extension);
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

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    ASSERT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());

    EXPECT_EQ(true, callback_receiver.value()->echo_appid_extension);
    EXPECT_EQ(false, callback_receiver.value()->appid_extension);
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

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    ASSERT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());

    EXPECT_EQ(true, callback_receiver.value()->echo_appid_extension);
    EXPECT_EQ(true, callback_receiver.value()->appid_extension);
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

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    ASSERT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());

    EXPECT_EQ(true, callback_receiver.value()->echo_appid_extension);
    EXPECT_EQ(true, callback_receiver.value()->appid_extension);
  }
}

TEST_F(AuthenticatorImplTest, AppIdExcludeExtension) {
  SimulateNavigation(GURL(kTestOrigin1));
  auto authenticator = ConnectToAuthenticator();

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

      TestMakeCredentialCallback callback_receiver;
      authenticator->MakeCredential(std::move(options),
                                    callback_receiver.callback());
      callback_receiver.WaitForCallback();

      if (credential_already_exists) {
        ASSERT_EQ(AuthenticatorStatus::CREDENTIAL_EXCLUDED,
                  callback_receiver.status());
      } else {
        ASSERT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
      }
    }
  }
}

TEST_F(AuthenticatorImplTest, TestGetAssertionTimeout) {
  // The VirtualFidoAuthenticator simulates a tap immediately after it gets the
  // request. Replace by the real discovery that will wait until timeout.
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<device::FidoDiscoveryFactory>());
  SimulateNavigation(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  // Trigger timer.
  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
}

TEST_F(AuthenticatorImplTest, OversizedCredentialId) {
  TestServiceManagerContext service_manager_context;

  // 255 is the maximum size of a U2F credential ID. We also test one greater
  // (256) to ensure that nothing untoward happens.
  const std::vector<size_t> kSizes = {255, 256};

  for (const size_t size : kSizes) {
    SCOPED_TRACE(size);

    SimulateNavigation(GURL(kTestOrigin1));
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
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

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();

    if (should_be_valid) {
      EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
    } else {
      EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                callback_receiver.status());
    }
  }
}

#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_CHROMEOS)
TEST_F(AuthenticatorImplTest, TestCableDiscoveryByDefault) {
  auto authenticator = ConnectToAuthenticator();

  // caBLE should be enabled by default if BLE is supported.
  bool should_be_enabled =
      device::BluetoothAdapterFactory::Get().IsLowEnergySupported();

  EXPECT_EQ(
      should_be_enabled,
      SupportsTransportProtocol(
          device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy));
}
#endif  // defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_CHROMEOS)

TEST_F(AuthenticatorImplTest, TestCableDiscoveryDisabledWithFlag) {
  DisableFeature(features::kWebAuthCable);

  auto authenticator = ConnectToAuthenticator();
  EXPECT_FALSE(SupportsTransportProtocol(
      device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy));
}

#if defined(OS_WIN)
TEST_F(AuthenticatorImplTest, TestCableDiscoveryEnabled) {
  auto authenticator = ConnectToAuthenticator();

  // Should be enabled if the new Windows BLE stack is.
  EXPECT_EQ(
      device::BluetoothAdapterFactory::Get().IsLowEnergySupported(),
      SupportsTransportProtocol(
          device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy));
}
#endif

TEST_F(AuthenticatorImplTest, NoSilentAuthenticationForCable) {
  // https://crbug.com/954355
  auto bluetooth_values = SetUpMockBluetooth();
  EnableFeature(features::kWebAuthCable);

  TestServiceManagerContext service_manager_context;
  SimulateNavigation(GURL(kTestOrigin1));

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

    TestGetAssertionCallback callback_receiver;
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();

    if (is_cable_device) {
      EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
    } else {
      // If a caBLE device is not simulated then silent requests should be used.
      // The virtual device will return an error because
      // |reject_silent_authentication_requests| is true and then it'll
      // immediately resolve the touch request.
      EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                callback_receiver.status());
    }
  }
}

TEST_F(AuthenticatorImplTest, TestGetAssertionU2fDeviceBackwardsCompatibility) {
  SimulateNavigation(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  // Inject credential ID to the virtual device so that successful sign in is
  // possible.
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  // Trigger timer.
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
}

TEST_F(AuthenticatorImplTest, GetAssertionWithEmptyAllowCredentials) {
  SimulateNavigation(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials.clear();
  TestGetAssertionCallback callback_receiver;

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  // Trigger timer.
  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED,
            callback_receiver.status());
}

TEST_F(AuthenticatorImplTest, MakeCredentialAlreadyRegistered) {
  TestServiceManagerContext service_manager_context;

  SimulateNavigation(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  // Exclude the one already registered credential.
  options->exclude_credentials = GetTestCredentials();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->exclude_credentials[0].id(), kTestRelyingPartyId));

  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());
  callback_receiver.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_EXCLUDED,
            callback_receiver.status());
}

TEST_F(AuthenticatorImplTest, MakeCredentialPendingRequest) {
  TestServiceManagerContext service_manager_context;

  SimulateNavigation(GURL(kTestOrigin1));
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
  TestServiceManagerContext service_manager_context;

  SimulateNavigation(GURL(kTestOrigin1));
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
  TestServiceManagerContext service_manager_context;

  SimulateNavigation(GURL(kTestOrigin1));
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
                           [&]() { SimulateNavigation(GURL(kTestOrigin2)); }));
        return false;
      });

  run_loop.Run();
}

TEST_F(AuthenticatorImplTest, InvalidResponse) {
  TestServiceManagerContext service_manager_context;

  virtual_device_factory_->mutable_state()->simulate_invalid_response = true;
  SimulateNavigation(GURL(kTestOrigin1));

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    // Trigger timer.
    base::RunLoop().RunUntilIdle();
    task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              callback_receiver.status());
  }

  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    // Trigger timer.
    base::RunLoop().RunUntilIdle();
    task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              callback_receiver.status());
  }
}

TEST_F(AuthenticatorImplTest, Ctap2AssertionWithUnknownCredential) {
  TestServiceManagerContext service_manager_context;
  SimulateNavigation(GURL(kTestOrigin1));

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

    TestGetAssertionCallback callback_receiver;
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    authenticator->GetAssertion(GetTestPublicKeyCredentialRequestOptions(),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              callback_receiver.status());
    // The user must have pressed the authenticator for the operation to
    // resolve.
    EXPECT_TRUE(pressed);
  }
}

TEST_F(AuthenticatorImplTest, GetAssertionResponseWithAttestedCredentialData) {
  TestServiceManagerContext service_manager_context;

  device::VirtualCtap2Device::Config config;
  config.return_attested_cred_data_in_get_assertion_response = true;
  virtual_device_factory_->SetCtap2Config(config);
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  TestGetAssertionCallback callback_receiver;

  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
}

enum class IndividualAttestation {
  REQUESTED,
  NOT_REQUESTED,
};

enum class AttestationConsent {
  GRANTED,
  DENIED,
};

enum class AttestationType {
  ANY,
  NONE,
  NONE_WITH_NONZERO_AAGUID,
  U2F,
  SELF,
  SELF_WITH_NONZERO_AAGUID,
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
      return ::device::AttestationConveyancePreference::kEnterprise;
  }
}

class TestAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  TestAuthenticatorRequestDelegate(
      RenderFrameHost* render_frame_host,
      base::OnceClosure action_callbacks_registered_callback,
      IndividualAttestation individual_attestation,
      AttestationConsent attestation_consent,
      bool is_focused,
      bool is_uvpaa)
      : action_callbacks_registered_callback_(
            std::move(action_callbacks_registered_callback)),
        individual_attestation_(individual_attestation),
        attestation_consent_(attestation_consent),
        is_focused_(is_focused),
        is_uvpaa_(is_uvpaa) {}
  ~TestAuthenticatorRequestDelegate() override {}

  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::Closure start_over_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback,
      device::FidoRequestHandlerBase::BlePairingCallback ble_pairing_callback)
      override {
    ASSERT_TRUE(action_callbacks_registered_callback_)
        << "RegisterActionCallbacks called twice.";
    cancel_callback_.emplace(std::move(cancel_callback));
    std::move(action_callbacks_registered_callback_).Run();
  }

  bool ShouldPermitIndividualAttestation(
      const std::string& relying_party_id) override {
    return individual_attestation_ == IndividualAttestation::REQUESTED;
  }

  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      const device::FidoAuthenticator* authenticator,
      base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(attestation_consent_ ==
                            AttestationConsent::GRANTED);
  }

  bool IsUserVerifyingPlatformAuthenticatorAvailable() override {
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
  const IndividualAttestation individual_attestation_;
  const AttestationConsent attestation_consent_;
  const bool is_focused_;
  const bool is_uvpaa_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAuthenticatorRequestDelegate);
};

class TestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host,
      const std::string& relying_party_id) override {
    if (return_null_delegate)
      return nullptr;
    return std::make_unique<TestAuthenticatorRequestDelegate>(
        render_frame_host,
        action_callbacks_registered_callback
            ? std::move(action_callbacks_registered_callback)
            : base::DoNothing(),
        individual_attestation, attestation_consent, is_focused, is_uvpaa);
  }

  // If set, this closure will be called when the subsequently constructed
  // delegate is informed that the request has started.
  base::OnceClosure action_callbacks_registered_callback;

  IndividualAttestation individual_attestation =
      IndividualAttestation::NOT_REQUESTED;
  AttestationConsent attestation_consent = AttestationConsent::DENIED;
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
    IndividualAttestation individual_attestation;
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
    TestServiceManagerContext smc_;
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();

    for (size_t i = 0; i < tests.size(); i++) {
      const auto& test = tests[i];
      SCOPED_TRACE(test.attestation_consent == AttestationConsent::GRANTED
                       ? "consent granted"
                       : "consent denied");
      SCOPED_TRACE(test.individual_attestation ==
                           IndividualAttestation::REQUESTED
                       ? "individual attestation"
                       : "no individual attestation");
      SCOPED_TRACE(
          AttestationConveyancePreferenceToString(test.attestation_requested));
      SCOPED_TRACE(i);

      test_client_.individual_attestation = test.individual_attestation;
      test_client_.attestation_consent = test.attestation_consent;

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->relying_party.id = "example.com";
      options->adjusted_timeout = base::TimeDelta::FromSeconds(1);
      options->attestation =
          ConvertAttestationConveyancePreference(test.attestation_requested);
      TestMakeCredentialCallback callback_receiver;
      authenticator->MakeCredential(std::move(options),
                                    callback_receiver.callback());
      callback_receiver.WaitForCallback();
      ASSERT_EQ(test.expected_status, callback_receiver.status());

      if (test.expected_status != AuthenticatorStatus::SUCCESS) {
        ASSERT_EQ(AttestationType::ANY, test.expected_attestation);
        continue;
      }

      base::Optional<Value> attestation_value =
          Reader::Read(callback_receiver.value()->attestation_object);
      ASSERT_TRUE(attestation_value);
      ASSERT_TRUE(attestation_value->is_map());
      const auto& attestation = attestation_value->GetMap();

      base::Optional<device::AuthenticatorData> auth_data = base::nullopt;
      const auto auth_data_it = attestation.find(Value("authData"));
      if (auth_data_it != attestation.end() &&
          auth_data_it->second.is_bytestring()) {
        auth_data = device::AuthenticatorData::DecodeAuthenticatorData(
            auth_data_it->second.GetBytestring());
      }

      switch (test.expected_attestation) {
        case AttestationType::ANY:
          ASSERT_STREQ("", test.expected_certificate_substring);
          break;

        case AttestationType::NONE:
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "none");
          EXPECT_TRUE(auth_data->attested_data()->IsAaguidZero());
          break;

        case AttestationType::NONE_WITH_NONZERO_AAGUID:
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "none");
          EXPECT_FALSE(auth_data->attested_data()->IsAaguidZero());
          break;

        case AttestationType::U2F:
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "fido-u2f");
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
          EXPECT_TRUE(auth_data->attested_data()->IsAaguidZero());
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
          EXPECT_FALSE(auth_data->attested_data()->IsAaguidZero());
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
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::NONE,
          IndividualAttestation::REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          IndividualAttestation::REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::INDIRECT,
          IndividualAttestation::REQUESTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::REQUESTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::REQUESTED,
          AttestationConsent::GRANTED,
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

TEST_F(AuthenticatorContentBrowserClientTest,
       InappropriatelyIdentifyingAttestation) {
  // This common name is used by several devices that have inappropriately
  // identifying attestation certificates.
  const char kCommonName[] = "FT FIDO 0100";

  const std::vector<TestCase> kTests = {
      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED,
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
          IndividualAttestation::NOT_REQUESTED,
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
          IndividualAttestation::REQUESTED,
          AttestationConsent::GRANTED,
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
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE_WITH_NONZERO_AAGUID,
          "",
      },
      {
          // If attestation is requested, but denied, we'll return none
          // attestation. But because the transport is kInternal, the AAGUID
          // will be preserved.
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE_WITH_NONZERO_AAGUID,
          "",
      },
      {
          // If attestation is requested and granted, the self attestation
          // will be returned.
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED,
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
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::SELF,
          "",
      },
      {
          // If attestation is requested, but denied, we'll return none
          // attestation.
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          // If attestation is requested and granted, the self attestation will
          // be returned.
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED,
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
          IndividualAttestation::NOT_REQUESTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
  };

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       MakeCredentialRequestStartedCallback) {
  TestServiceManagerContext smc;
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
  TestServiceManagerContext smc;
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
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->public_key_parameters = GetTestPublicKeyCredentialParameters(123);

    TestMakeCredentialCallback cb;
    TestRequestStartedCallback request_started;
    test_client_.action_callbacks_registered_callback =
        request_started.callback();

    authenticator->MakeCredential(std::move(options), cb.callback());
    cb.WaitForCallback();

    EXPECT_EQ(AuthenticatorStatus::NOT_FOCUSED, cb.status());
    EXPECT_FALSE(request_started.was_called());
  }

  {
    TestServiceManagerContext service_manager_context;

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();

    device::PublicKeyCredentialDescriptor credential;
    credential.SetCredentialTypeForTesting(device::CredentialType::kPublicKey);
    credential.GetIdForTesting().resize(16);
    credential.GetTransportsForTesting() = {
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice};

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        credential.id(), kTestRelyingPartyId));
    options->allow_credentials.emplace_back(credential);

    TestGetAssertionCallback cb;
    TestRequestStartedCallback request_started;
    test_client_.action_callbacks_registered_callback =
        request_started.callback();

    authenticator->GetAssertion(std::move(options), cb.callback());
    cb.WaitForCallback();

    EXPECT_EQ(AuthenticatorStatus::SUCCESS, cb.status());
    EXPECT_TRUE(request_started.was_called());
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       NullDelegate_RejectsWithPendingRequest) {
  test_client_.return_null_delegate = true;

  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();

    TestMakeCredentialCallback cb;
    authenticator->MakeCredential(std::move(options), cb.callback());
    cb.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, cb.status());
  }

  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();

    TestGetAssertionCallback cb;
    authenticator->GetAssertion(std::move(options), cb.callback());
    cb.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, cb.status());
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, IsUVPAA) {
  SimulateNavigation(GURL(kTestOrigin1));
  for (const bool is_uvpaa : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "is_uvpaa=" << is_uvpaa);
    test_client_.is_uvpaa = is_uvpaa;

    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();

    TestIsUvpaaCallback cb;
    authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(cb.callback());
    cb.WaitForCallback();
    EXPECT_EQ(is_uvpaa, cb.value());
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       CryptotokenBypassesAttestationConsentPrompt) {
  TestServiceManagerContext smc;
  SimulateNavigation(GURL(kTestOrigin1));
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  OverrideLastCommittedOrigin(main_rfh(),
                              url::Origin::Create(GURL(kCryptotokenOrigin)));

  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  // Despite the direct attestation conveyance preference, the request delegate
  // is not asked for attestation consent. Hence the request will succeed,
  // despite the handler denying all attestation consent prompts.
  options->attestation = device::AttestationConveyancePreference::kDirect;
  test_client_.attestation_consent = AttestationConsent::DENIED;

  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());

  callback_receiver.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
}

TEST_F(AuthenticatorContentBrowserClientTest,
       CableCredentialWithoutCableExtension) {
  // Exercise the case where a credential is marked as "cable" but no caBLE
  // extension is provided. The AuthenticatorRequestClientDelegate should see no
  // transports, which triggers it to cancel the request. (Outside of a testing
  // environment, Chrome's AuthenticatorRequestClientDelegate will show an
  // informative error and wait for the user to cancel the request.)
  EnableFeature(features::kWebAuthCable);
  TestServiceManagerContext service_manager_context;
  SimulateNavigation(GURL(kTestOrigin1));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  std::vector<uint8_t> id(32u, 1u);
  base::flat_set<device::FidoTransportProtocol> transports{
      device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};
  options->allow_credentials.clear();
  options->allow_credentials.emplace_back(device::CredentialType::kPublicKey,
                                          std::move(id), std::move(transports));

  TestGetAssertionCallback callback_receiver;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());
  callback_receiver.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
}

class MockAuthenticatorRequestDelegateObserver
    : public TestAuthenticatorRequestDelegate {
 public:
  using InterestingFailureReasonCallback =
      base::OnceCallback<void(InterestingFailureReason)>;

  MockAuthenticatorRequestDelegateObserver(
      InterestingFailureReasonCallback failure_reasons_callback =
          base::DoNothing())
      : TestAuthenticatorRequestDelegate(
            nullptr /* render_frame_host */,
            base::DoNothing() /* did_start_request_callback */,
            IndividualAttestation::NOT_REQUESTED,
            AttestationConsent::DENIED,
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
      service_manager::Connector* connector,
      std::unique_ptr<base::OneShotTimer> timer,
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate)
      : AuthenticatorCommon(render_frame_host, connector, std::move(timer)),
        mock_delegate_(std::move(mock_delegate)) {}
  ~FakeAuthenticatorCommon() override = default;

  std::unique_ptr<AuthenticatorRequestClientDelegate> CreateRequestDelegate(
      std::string relying_party_id) override {
    DCHECK(mock_delegate_);
    return std::move(mock_delegate_);
  }

 private:
  friend class AuthenticatorImplRequestDelegateTest;

  std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate_;
};

class AuthenticatorImplRequestDelegateTest : public AuthenticatorImplTest {
 public:
  AuthenticatorImplRequestDelegateTest() {}
  ~AuthenticatorImplRequestDelegateTest() override {}

  void TearDown() override {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    authenticator_impl_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  mojo::Remote<blink::mojom::Authenticator> ConnectToFakeAuthenticator(
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> delegate,
      service_manager::Connector* connector,
      std::unique_ptr<base::OneShotTimer> timer) {
    authenticator_impl_.reset(new AuthenticatorImpl(
        main_rfh(),
        std::make_unique<FakeAuthenticatorCommon>(
            main_rfh(), connector, std::move(timer), std::move(delegate))));
    mojo::Remote<blink::mojom::Authenticator> authenticator;
    authenticator_impl_->Bind(authenticator.BindNewPipeAndPassReceiver());
    return authenticator;
  }

  mojo::Remote<blink::mojom::Authenticator> ConstructFakeAuthenticatorWithTimer(
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> delegate,
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner) {
    connector_ = service_manager::Connector::Create(&receiver_);
    fake_hid_manager_ = std::make_unique<device::FakeFidoHidManager>();
    connector_->OverrideBinderForTesting(
        service_manager::ServiceFilter::ByName(device::mojom::kServiceName),
        device::mojom::HidManager::Name_,
        base::Bind(&device::FakeFidoHidManager::AddReceiver,
                   base::Unretained(fake_hid_manager_.get())));

    // Set up a timer for testing.
    auto timer =
        std::make_unique<base::OneShotTimer>(task_runner->GetMockTickClock());
    timer->SetTaskRunner(task_runner);
    return ConnectToFakeAuthenticator(std::move(delegate), connector_.get(),
                                      std::move(timer));
  }

 protected:
  std::unique_ptr<AuthenticatorImpl> authenticator_impl_;
};

TEST_F(AuthenticatorImplRequestDelegateTest,
       TestRequestDelegateObservesFidoRequestHandler) {
  EnableFeature(features::kWebAuthBle);
  auto bluetooth_values = SetUpMockBluetooth();

  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillRepeatedly(::testing::Return(true));

  auto discovery_factory =
      std::make_unique<device::test::FakeFidoDiscoveryFactory>();
  auto* fake_ble_discovery = discovery_factory->ForgeNextBleDiscovery();
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(std::move(discovery_factory));

  SimulateNavigation(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());

  auto mock_delegate =
      std::make_unique<MockAuthenticatorRequestDelegateObserver>();
  auto* const mock_delegate_ptr = mock_delegate.get();
  auto authenticator = ConstructFakeAuthenticatorWithTimer(
      std::move(mock_delegate), task_runner);

  auto mock_ble_device = device::MockFidoDevice::MakeCtap();
  mock_ble_device->StubGetId();
  mock_ble_device->SetDeviceTransport(
      device::FidoTransportProtocol::kBluetoothLowEnergy);
  const auto device_id = mock_ble_device->GetId();

  EXPECT_CALL(*mock_delegate_ptr, OnTransportAvailabilityEnumerated(_));
  EXPECT_CALL(*mock_delegate_ptr, EmbedderControlsAuthenticatorDispatch(_))
      .WillOnce(testing::Return(true));

  base::RunLoop ble_device_found_done;
  EXPECT_CALL(*mock_delegate_ptr, FidoAuthenticatorAdded(_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&ble_device_found_done]() { ble_device_found_done.Quit(); }));

  base::RunLoop ble_device_lost_done;
  EXPECT_CALL(*mock_delegate_ptr, FidoAuthenticatorRemoved(_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&ble_device_lost_done]() { ble_device_lost_done.Quit(); }));

  authenticator->GetAssertion(std::move(options), callback_receiver.callback());
  fake_ble_discovery->WaitForCallToStartAndSimulateSuccess();
  fake_ble_discovery->AddDevice(std::move(mock_ble_device));
  ble_device_found_done.Run();

  fake_ble_discovery->RemoveDevice(device_id);
  ble_device_lost_done.Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AuthenticatorImplRequestDelegateTest, FailureReasonForTimeout) {
  // The VirtualFidoAuthenticator simulates a tap immediately after it gets the
  // request. Replace by the real discovery that will wait until timeout.
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<device::FidoDiscoveryFactory>());
  SimulateNavigation(GURL(kTestOrigin1));

  FailureReasonCallbackReceiver failure_reason_receiver;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_receiver.callback());
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructFakeAuthenticatorWithTimer(
      std::move(mock_delegate), task_runner);

  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(GetTestPublicKeyCredentialRequestOptions(),
                              callback_receiver.callback());

  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));

  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());

  ASSERT_TRUE(failure_reason_receiver.was_called());
  EXPECT_EQ(content::AuthenticatorRequestClientDelegate::
                InterestingFailureReason::kTimeout,
            std::get<0>(*failure_reason_receiver.result()));
}

TEST_F(AuthenticatorImplRequestDelegateTest,
       FailureReasonForDuplicateRegistration) {
  SimulateNavigation(GURL(kTestOrigin1));

  FailureReasonCallbackReceiver failure_reason_receiver;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_receiver.callback());
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructFakeAuthenticatorWithTimer(
      std::move(mock_delegate), task_runner);

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
  SimulateNavigation(GURL(kTestOrigin1));

  FailureReasonCallbackReceiver failure_reason_receiver;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_receiver.callback());
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructFakeAuthenticatorWithTimer(
      std::move(mock_delegate), task_runner);

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
  TestServiceManagerContext smc;
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  auto bluetooth_values = SetUpMockBluetooth();
  SetTransports(device::GetAllTransportProtocols());

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

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      TestMakeCredentialCallback callback_receiver;
      authenticator->MakeCredential(std::move(options),
                                    callback_receiver.callback());
      callback_receiver.WaitForCallback();
      EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());

      const std::vector<device::FidoTransportProtocol>& transports(
          callback_receiver.value()->transports);
      ASSERT_EQ(1u, transports.size());
      EXPECT_EQ(transport.first, transports[0]);
    }
  }
}

TEST_F(AuthenticatorImplTest, ExtensionHMACSecret) {
  TestServiceManagerContext smc;
  NavigateAndCommit(GURL(kTestOrigin1));

  for (const bool include_extension : {false, true}) {
    SCOPED_TRACE(include_extension);

    virtual_device_factory_->SetSupportedProtocol(
        device::ProtocolVersion::kCtap2);

    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->hmac_create_secret = include_extension;
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());

    base::Optional<Value> attestation_value =
        Reader::Read(callback_receiver.value()->attestation_object);
    ASSERT_TRUE(attestation_value);
    ASSERT_TRUE(attestation_value->is_map());
    const auto& attestation = attestation_value->GetMap();

    const auto auth_data_it = attestation.find(Value(device::kAuthDataKey));
    ASSERT_TRUE(auth_data_it != attestation.end());
    ASSERT_TRUE(auth_data_it->second.is_bytestring());
    const std::vector<uint8_t>& auth_data =
        auth_data_it->second.GetBytestring();
    base::Optional<device::AuthenticatorData> parsed_auth_data =
        device::AuthenticatorData::DecodeAuthenticatorData(auth_data);

    // The virtual CTAP2 device always echos the hmac-secret extension on
    // registrations. Therefore, if |hmac_secret| was set above it should be
    // serialised in the CBOR and correctly passed all the way back around to
    // the reply's authenticator data.
    bool has_hmac_secret = false;
    const auto& extensions = parsed_auth_data->extensions();
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

    EXPECT_EQ(include_extension, has_hmac_secret);
  }
}

// Tests that for an authenticator that does not support batching, credential
// lists get probed silently to work around authenticators rejecting exclude
// lists exceeding a certain size.
TEST_F(AuthenticatorImplTest, MakeCredentialWithLargeExcludeList) {
  TestServiceManagerContext smc;
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
    TestMakeCredentialCallback callback_receiver;

    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    base::RunLoop().RunUntilIdle();
    callback_receiver.WaitForCallback();
    EXPECT_EQ(callback_receiver.status(),
              has_excluded_credential ? AuthenticatorStatus::CREDENTIAL_EXCLUDED
                                      : AuthenticatorStatus::SUCCESS);
  }
}

// Tests that for an authenticator that does not support batching, credential
// lists get probed silently to work around authenticators rejecting allow lists
// exceeding a certain size.
TEST_F(AuthenticatorImplTest, GetAssertionWithLargeAllowList) {
  TestServiceManagerContext smc;
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool has_allowed_credential : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "has_allowed_credential=" << has_allowed_credential);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.reject_large_allow_and_exclude_lists = true;
    virtual_device_factory_->SetCtap2Config(config);

    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = GetTestCredentials(/*num_credentials=*/10);
    if (has_allowed_credential) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          options->allow_credentials.back().id(), kTestRelyingPartyId));
    }

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    base::RunLoop().RunUntilIdle();
    callback_receiver.WaitForCallback();
    EXPECT_EQ(callback_receiver.status(),
              has_allowed_credential ? AuthenticatorStatus::SUCCESS
                                     : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

// Tests that, regardless of batching support, GetAssertion requests with a
// single allowed credential ID don't result in a silent probing request.
TEST_F(AuthenticatorImplTest, GetAssertionSingleElementAllowListDoesNotProbe) {
  TestServiceManagerContext smc;
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

    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();

    auto test_credentials = GetTestCredentials(/*num_credentials=*/1);
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        test_credentials.front().id(), kTestRelyingPartyId));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = std::move(test_credentials);

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    base::RunLoop().RunUntilIdle();
    callback_receiver.WaitForCallback();

    EXPECT_EQ(callback_receiver.status(), AuthenticatorStatus::SUCCESS);
  }
}

// Tests that an allow list that fits into a single batch does not result in a
// silent probing request.
TEST_F(AuthenticatorImplTest, GetAssertionSingleBatchListDoesNotProbe) {
  TestServiceManagerContext smc;
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

    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();

    auto test_credentials = GetTestCredentials(
        /*num_credentials=*/kBatchSize +
        (allow_list_fits_single_batch ? 0 : 1));
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        test_credentials.back().id(), kTestRelyingPartyId));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = std::move(test_credentials);

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    base::RunLoop().RunUntilIdle();
    callback_receiver.WaitForCallback();

    EXPECT_EQ(callback_receiver.status(),
              allow_list_fits_single_batch
                  ? AuthenticatorStatus::SUCCESS
                  : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, NoUnexpectedAuthenticatorExtensions) {
  TestServiceManagerContext smc;
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.add_extra_extension = true;
  virtual_device_factory_->SetCtap2Config(config);

  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // Check that extra authenticator extensions are rejected when creating a
  // credential.
  TestMakeCredentialCallback create_callback;
  authenticator->MakeCredential(GetTestPublicKeyCredentialCreationOptions(),
                                create_callback.callback());
  base::RunLoop().RunUntilIdle();
  create_callback.WaitForCallback();
  EXPECT_EQ(create_callback.status(), AuthenticatorStatus::NOT_ALLOWED_ERROR);

  // Extensions should also be rejected when getting an assertion.
  PublicKeyCredentialRequestOptionsPtr assertion_options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      assertion_options->allow_credentials.back().id(), kTestRelyingPartyId));
  TestGetAssertionCallback assertion_callback;
  authenticator->GetAssertion(std::move(assertion_options),
                              assertion_callback.callback());
  base::RunLoop().RunUntilIdle();
  assertion_callback.WaitForCallback();
  EXPECT_EQ(assertion_callback.status(),
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// Tests that on an authenticator that supports batching, exclude lists that fit
// into a single batch are sent without probing.
TEST_F(AuthenticatorImplTest, ExcludeListBatching) {
  TestServiceManagerContext smc;
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

    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->exclude_credentials = std::move(test_credentials);
    TestMakeCredentialCallback callback;
    authenticator->MakeCredential(std::move(options), callback.callback());
    base::RunLoop().RunUntilIdle();
    callback.WaitForCallback();

    EXPECT_EQ(callback.status(), authenticator_has_excluded_credential
                                     ? AuthenticatorStatus::CREDENTIAL_EXCLUDED
                                     : AuthenticatorStatus::SUCCESS);
  }
}

class UVAuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  UVAuthenticatorImplTest() = default;

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

  static bool HasUV(const TestMakeCredentialCallback& callback) {
    DCHECK_EQ(AuthenticatorStatus::SUCCESS, callback.status());
    base::Optional<Value> attestation_value =
        Reader::Read(callback.value()->attestation_object);
    DCHECK(attestation_value);
    DCHECK(attestation_value->is_map());
    const auto& attestation = attestation_value->GetMap();

    const auto auth_data_it = attestation.find(Value("authData"));
    DCHECK(auth_data_it != attestation.end() &&
           auth_data_it->second.is_bytestring());
    base::Optional<device::AuthenticatorData> auth_data =
        device::AuthenticatorData::DecodeAuthenticatorData(
            auth_data_it->second.GetBytestring());
    return auth_data->obtained_user_verification();
  }

  static bool HasUV(const TestGetAssertionCallback& callback) {
    DCHECK_EQ(AuthenticatorStatus::SUCCESS, callback.status());
    base::Optional<device::AuthenticatorData> auth_data =
        device::AuthenticatorData::DecodeAuthenticatorData(
            callback.value()->authenticator_data);
    return auth_data->obtained_user_verification();
  }

 private:
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

  void FinishCollectPIN() override {}

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
      RenderFrameHost* render_frame_host,
      const std::string& relying_party_id) override {
    return std::make_unique<PINTestAuthenticatorRequestDelegate>(
        supports_pin, expected, &failure_reason);
  }

  bool supports_pin = true;
  PINList expected;
  base::Optional<InterestingFailureReason> failure_reason;
};

static constexpr char kTestPIN[] = "1234";

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
    AuthenticatorImplTest::TearDown();
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

  void ConfigureVirtualDevice(int support_level) {
    device::VirtualCtap2Device::Config config;
    switch (support_level) {
      case 0:
        // No support.
        config.pin_support = false;
        virtual_device_factory_->mutable_state()->pin = "";
        virtual_device_factory_->mutable_state()->retries = 0;
        break;

      case 1:
        // PIN supported, but no PIN set.
        config.pin_support = true;
        virtual_device_factory_->mutable_state()->pin = "";
        virtual_device_factory_->mutable_state()->retries = 0;
        break;

      case 2:
        // PIN set.
        config.pin_support = true;
        virtual_device_factory_->mutable_state()->pin = kTestPIN;
        virtual_device_factory_->mutable_state()->retries = 8;
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
  TestServiceManagerContext smc;

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

  for (bool ui_support : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "ui_support=" << ui_support);
    const Expectations& expected =
        ui_support ? kExpectedWithUISupport : kExpectedWithoutUISupport;
    test_client_.supports_pin = ui_support;

    for (int support_level = 0; support_level <= 2; support_level++) {
      SCOPED_TRACE(kPINSupportDescription[support_level]);
      ConfigureVirtualDevice(support_level);

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

        mojo::Remote<blink::mojom::Authenticator> authenticator =
            ConnectToAuthenticator();
        TestMakeCredentialCallback callback_receiver;
        authenticator->MakeCredential(
            make_credential_options(kUVLevel[uv_level]),
            callback_receiver.callback());
        callback_receiver.WaitForCallback();

        switch (expected[support_level][uv_level]) {
          case kFailure:
            EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                      callback_receiver.status());
            break;

          case kNoPIN:
            EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
            EXPECT_EQ("", virtual_device_factory_->mutable_state()->pin);
            EXPECT_FALSE(HasUV(callback_receiver));
            break;

          case kSetPIN:
          case kUsePIN:
            EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
            EXPECT_EQ(kTestPIN, virtual_device_factory_->mutable_state()->pin);
            EXPECT_TRUE(HasUV(callback_receiver));
            break;

          default:
            NOTREACHED();
        }
      }
    }
  }
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialSoftLock) {
  TestServiceManagerContext smc;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->retries = 8;

  test_client_.expected = {{8, "wrong"}, {7, "wrong"}, {6, "wrong"}};
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(make_credential_options(),
                                callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->retries);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->soft_locked);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kSoftPINBlock,
            *test_client_.failure_reason);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialHardLock) {
  TestServiceManagerContext smc;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->retries = 1;

  test_client_.expected = {{1, "wrong"}};
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(make_credential_options(),
                                callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
  EXPECT_EQ(0, virtual_device_factory_->mutable_state()->retries);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kHardPINBlock,
            *test_client_.failure_reason);
}

TEST_F(PINAuthenticatorImplTest, GetAssertion) {
  TestServiceManagerContext smc;

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

  for (bool ui_support : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "ui_support=" << ui_support);
    const Expectations& expected =
        ui_support ? kExpectedWithUISupport : kExpectedWithoutUISupport;
    test_client_.supports_pin = ui_support;

    for (int support_level = 0; support_level <= 2; support_level++) {
      SCOPED_TRACE(kPINSupportDescription[support_level]);
      ConfigureVirtualDevice(support_level);

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

        mojo::Remote<blink::mojom::Authenticator> authenticator =
            ConnectToAuthenticator();
        TestGetAssertionCallback callback_receiver;
        authenticator->GetAssertion(get_credential_options(kUVLevel[uv_level]),
                                    callback_receiver.callback());
        callback_receiver.WaitForCallback();

        switch (expected[support_level][uv_level]) {
          case kFailure:
            EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                      callback_receiver.status());
            break;

          case kNoPIN:
            EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
            EXPECT_FALSE(HasUV(callback_receiver));
            break;

          case kUsePIN:
            EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
            EXPECT_EQ(kTestPIN, virtual_device_factory_->mutable_state()->pin);
            EXPECT_TRUE(HasUV(callback_receiver));
            break;

          default:
            NOTREACHED();
        }
      }
    }
  }
}

TEST_F(PINAuthenticatorImplTest, GetAssertionSoftLock) {
  TestServiceManagerContext smc;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->retries = 8;

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  test_client_.expected = {{8, "wrong"}, {7, "wrong"}, {6, "wrong"}};
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->retries);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->soft_locked);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kSoftPINBlock,
            *test_client_.failure_reason);
}

TEST_F(PINAuthenticatorImplTest, GetAssertionHardLock) {
  TestServiceManagerContext smc;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->retries = 1;

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  test_client_.expected = {{1, "wrong"}};
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
  EXPECT_EQ(0, virtual_device_factory_->mutable_state()->retries);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kHardPINBlock,
            *test_client_.failure_reason);
}

class InternalUVAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  InternalUVAuthenticatorImplTest() = default;

  void SetUp() override {
    UVAuthenticatorImplTest::SetUp();
    device::VirtualCtap2Device::Config config;
    config.internal_uv_support = true;
    config.u2f_support = true;
    virtual_device_factory_->SetCtap2Config(config);
    NavigateAndCommit(GURL(kTestOrigin1));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalUVAuthenticatorImplTest);
};

TEST_F(InternalUVAuthenticatorImplTest, MakeCredential) {
  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;

    for (const auto uv : {device::UserVerificationRequirement::kDiscouraged,
                          device::UserVerificationRequirement::kPreferred,
                          device::UserVerificationRequirement::kRequired}) {
      SCOPED_TRACE(UVToString(uv));

      auto options = make_credential_options(uv);
      // UV cannot be satisfied without fingerprints.
      const bool should_timeout =
          !fingerprints_enrolled &&
          uv == device::UserVerificationRequirement::kRequired;
      if (should_timeout) {
        options->adjusted_timeout = base::TimeDelta::FromMilliseconds(100);
      }

      TestMakeCredentialCallback callback_receiver;
      authenticator->MakeCredential(std::move(options),
                                    callback_receiver.callback());
      callback_receiver.WaitForCallback();

      if (should_timeout) {
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                  callback_receiver.status());
      } else {
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
        EXPECT_EQ(fingerprints_enrolled, HasUV(callback_receiver));
      }
    }
  }
}

TEST_F(InternalUVAuthenticatorImplTest, MakeCredentialCryptotoken) {
  TestServiceManagerContext smc;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  OverrideLastCommittedOrigin(main_rfh(),
                              url::Origin::Create(GURL(kCryptotokenOrigin)));

  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(
        make_credential_options(
            device::UserVerificationRequirement::kPreferred),
        callback_receiver.callback());

    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
    // The credential should have been created over U2F.
    for (const auto& registration :
         virtual_device_factory_->mutable_state()->registrations) {
      EXPECT_TRUE(registration.second.is_u2f);
    }
  }
}

TEST_F(InternalUVAuthenticatorImplTest, GetAssertion) {
  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
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

      auto options = get_credential_options(uv);
      // Without a fingerprint enrolled we assume that a UV=required request
      // cannot be satisfied by an authenticator that cannot do UV. It is
      // possible for a credential to be created without UV and then later
      // asserted with UV=required, but that would be bizarre behaviour from
      // an RP and we currently don't worry about it.
      const bool should_be_unrecognized =
          !fingerprints_enrolled &&
          uv == device::UserVerificationRequirement::kRequired;

      TestGetAssertionCallback callback_receiver;
      authenticator->GetAssertion(std::move(options),
                                  callback_receiver.callback());
      callback_receiver.WaitForCallback();

      if (should_be_unrecognized) {
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                  callback_receiver.status());
      } else {
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
        EXPECT_EQ(fingerprints_enrolled &&
                      uv != device::UserVerificationRequirement::kDiscouraged,
                  HasUV(callback_receiver));
      }
    }
  }
}

TEST_F(InternalUVAuthenticatorImplTest, GetAssertionCryptotoken) {
  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  OverrideLastCommittedOrigin(main_rfh(),
                              url::Origin::Create(GURL(kCryptotokenOrigin)));
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;
    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(
        get_credential_options(device::UserVerificationRequirement::kPreferred),
        callback_receiver.callback());

    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
  }
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

  void FinishCollectPIN() override {}

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
      RenderFrameHost* render_frame_host,
      const std::string& relying_party_id) override {
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
    virtual_device_factory_->mutable_state()->retries = 8;
    NavigateAndCommit(GURL(kTestOrigin1));
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

 protected:
  ResidentKeyTestAuthenticatorContentBrowserClient test_client_;

  static PublicKeyCredentialCreationOptionsPtr make_credential_options() {
    PublicKeyCredentialCreationOptionsPtr options =
        UVAuthenticatorImplTest::make_credential_options();
    options->authenticator_selection->SetRequireResidentKeyForTesting(true);
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

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredential) {
  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

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

    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(make_credential_options(),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());

    EXPECT_TRUE(test_client_.might_create_resident_credential);
    EXPECT_TRUE(HasUV(callback_receiver));
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

TEST_F(ResidentKeyAuthenticatorImplTest, StorageFull) {
  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

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

  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(make_credential_options(),
                                callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(AuthenticatorRequestClientDelegate::InterestingFailureReason::
                kStorageFull,
            test_client_.failure_reason);
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionSingleNoPII) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, base::nullopt, base::nullopt));

  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetAssertionCallback callback_receiver;
  // |SelectAccount| should not be called when there's only a single response
  // with no identifying user info because the UI is bad in that case: we can
  // only display the single choice of "Unknown user".
  test_client_.expected_accounts = "<invalid>";
  authenticator->GetAssertion(get_credential_options(),
                              callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
  EXPECT_TRUE(HasUV(callback_receiver));
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionSingleWithPII) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, base::nullopt, "Test User"));

  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetAssertionCallback callback_receiver;
  // |SelectAccount| should be called when PII is available.
  test_client_.expected_accounts = "01020304::Test User";
  test_client_.selected_user_id = {1, 2, 3, 4};
  authenticator->GetAssertion(get_credential_options(),
                              callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
  EXPECT_TRUE(HasUV(callback_receiver));
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionMulti) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, "test@example.com", "Test User"));
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 2}}, kTestRelyingPartyId,
      /*user_id=*/{{5, 6, 7, 8}}, "test2@example.com", "Test User 2"));

  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetAssertionCallback callback_receiver;
  test_client_.expected_accounts =
      "01020304:test@example.com:Test User/"
      "05060708:test2@example.com:Test User 2";
  test_client_.selected_user_id = {1, 2, 3, 4};
  authenticator->GetAssertion(get_credential_options(),
                              callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
  EXPECT_TRUE(HasUV(callback_receiver));
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

  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetAssertionCallback callback_receiver;
  // |SelectAccount| should not be called when there's only a single response
  // without identifying information.
  test_client_.expected_accounts = "<invalid>";
  PublicKeyCredentialRequestOptionsPtr options(get_credential_options());
  options->user_verification =
      device::UserVerificationRequirement::kDiscouraged;
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
  // The UV=discouraged should have been ignored for a resident-credential
  // request.
  EXPECT_TRUE(HasUV(callback_receiver));
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

TEST_F(ResidentKeyAuthenticatorImplTest, CredProtectRegistration) {
  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

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
    options->authenticator_selection->SetRequireResidentKeyForTesting(
        test.is_resident);
    options->protection_policy = test.protection;
    options->enforce_protection_policy = test.enforce;
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        test.uv ? device::UserVerificationRequirement::kRequired
                : device::UserVerificationRequirement::kDiscouraged);

    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();

    switch (test.expected_outcome) {
      case kOk: {
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
        ASSERT_EQ(
            1u, virtual_device_factory_->mutable_state()->registrations.size());
        const base::Optional<device::CredProtect> result =
            virtual_device_factory_->mutable_state()
                ->registrations.begin()
                ->second.protection;

        switch (test.resulting_policy) {
          case UNSPECIFIED:
            NOTREACHED();
            break;
          case NONE:
            EXPECT_FALSE(result);
            break;
          case UV_OR_CRED:
            ASSERT_TRUE(result);
            EXPECT_EQ(device::CredProtect::kUVOrCredIDRequired, *result);
            break;
          case UV_REQ:
            ASSERT_TRUE(result);
            EXPECT_EQ(device::CredProtect::kUVRequired, *result);
            break;
        }
        break;
      }
      case kNonsense:
        EXPECT_EQ(AuthenticatorStatus::PROTECTION_POLICY_INCONSISTENT,
                  callback_receiver.status());
        break;
      case kNotAllow:
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                  callback_receiver.status());
        break;
      default:
        NOTREACHED();
    }
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

  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetAssertionCallback callback_receiver;
  // |SelectAccount| should not be called when there's only a single response.
  test_client_.expected_accounts = "<invalid>";

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  options->allow_credentials = GetTestCredentials(5);
  options->allow_credentials[0].GetIdForTesting() = {4, 3, 2, 1};

  authenticator->GetAssertion(std::move(options), callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
  EXPECT_TRUE(HasUV(callback_receiver));
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

  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetAssertionCallback callback_receiver;
  // |SelectAccount| should not be called when there's only a single response
  // without identifying information.
  test_client_.expected_accounts = "<invalid>";

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  options->appid = kTestOrigin1;

  authenticator->GetAssertion(std::move(options), callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
  EXPECT_TRUE(HasUV(callback_receiver));
}

#if defined(OS_WIN)
// Requests with a credProtect extension that have |enforce_protection_policy|
// set should be rejected if the Windows WebAuthn API doesn't support
// credProtect.
TEST_F(ResidentKeyAuthenticatorImplTest, WinCredProtectApiVersion) {
  // The canned response returned by the Windows API fake is for acme.com.
  NavigateAndCommit(GURL("https://acme.com"));
  TestServiceManagerContext smc;
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
    options->authenticator_selection->SetRequireResidentKeyForTesting(true);
    options->protection_policy =
        blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED;
    options->enforce_protection_policy = true;

    TestMakeCredentialCallback callback_receiver;
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();

    EXPECT_EQ(callback_receiver.status(),
              supports_cred_protect ? AuthenticatorStatus::SUCCESS
                                    : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}
#endif  // defined(OS_WIN)

// Tests that an allowList with only credential IDs of a length exceeding the
// maxCredentialIdLength parameter is not mistakenly interpreted as an empty
// allow list.
TEST_F(ResidentKeyAuthenticatorImplTest,
       AllowListWithOnlyOversizedCredentialIds) {
  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.pin_support = true;
  config.resident_key_support = true;
  config.max_credential_id_length = kTestCredentialIdLength;
  config.max_credential_count_in_list = 10;
  virtual_device_factory_->SetCtap2Config(config);
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/std::vector<uint8_t>(kTestCredentialIdLength, 1),
      kTestRelyingPartyId,
      /*user_id=*/{{1}}, base::nullopt, base::nullopt));
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/std::vector<uint8_t>(kTestCredentialIdLength, 2),
      kTestRelyingPartyId,
      /*user_id=*/{{2}}, base::nullopt, base::nullopt));

  TestServiceManagerContext smc;
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestGetAssertionCallback callback_receiver;
  // |SelectAccount| should not be called since this is not a resident key
  // request.
  test_client_.expected_accounts = "<invalid>";

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  options->appid = kTestOrigin1;
  options->allow_credentials = {device::PublicKeyCredentialDescriptor(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength + 1, 0))};

  authenticator->GetAssertion(std::move(options), callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());
}

class InternalAuthenticatorImplTest : public AuthenticatorTestBase {
 protected:
  InternalAuthenticatorImplTest() = default;

  void TearDown() override {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    internal_authenticator_impl_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  void NavigateAndCommit(const GURL& url) {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    internal_authenticator_impl_.reset();
    content::RenderViewHostTestHarness::NavigateAndCommit(url);
  }

  mojo::Remote<blink::mojom::InternalAuthenticator> ConnectToAuthenticator(
      GURL effective_origin_url) {
    internal_authenticator_impl_ = std::make_unique<InternalAuthenticatorImpl>(
        main_rfh(), url::Origin::Create(effective_origin_url));
    mojo::Remote<blink::mojom::InternalAuthenticator> authenticator;
    internal_authenticator_impl_->Bind(
        authenticator.BindNewPipeAndPassReceiver());
    return authenticator;
  }

  mojo::Remote<blink::mojom::InternalAuthenticator> ConnectToAuthenticator(
      GURL effective_origin_url,
      service_manager::Connector* connector,
      std::unique_ptr<base::OneShotTimer> timer) {
    internal_authenticator_impl_.reset(new InternalAuthenticatorImpl(
        main_rfh(), url::Origin::Create(effective_origin_url),
        std::make_unique<AuthenticatorCommon>(main_rfh(), connector,
                                              std::move(timer))));
    mojo::Remote<blink::mojom::InternalAuthenticator> authenticator;
    internal_authenticator_impl_->Bind(
        authenticator.BindNewPipeAndPassReceiver());
    return authenticator;
  }

  mojo::Remote<blink::mojom::InternalAuthenticator>
  ConstructAuthenticatorWithTimer(
      GURL effective_origin_url,
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner) {
    connector_ = service_manager::Connector::Create(&receiver_);
    fake_hid_manager_ = std::make_unique<device::FakeFidoHidManager>();
    connector_->OverrideBinderForTesting(
        service_manager::ServiceFilter::ByName(device::mojom::kServiceName),
        device::mojom::HidManager::Name_,
        base::BindRepeating(&device::FakeFidoHidManager::AddReceiver,
                            base::Unretained(fake_hid_manager_.get())));

    // Set up a timer for testing.
    auto timer =
        std::make_unique<base::OneShotTimer>(task_runner->GetMockTickClock());
    timer->SetTaskRunner(task_runner);
    return ConnectToAuthenticator(effective_origin_url, connector_.get(),
                                  std::move(timer));
  }

 protected:
  std::unique_ptr<InternalAuthenticatorImpl> internal_authenticator_impl_;
  mojo::PendingReceiver<service_manager::mojom::Connector> receiver_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<device::FakeFidoHidManager> fake_hid_manager_;
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
    mojo::Remote<blink::mojom::InternalAuthenticator> authenticator =
        ConnectToAuthenticator(origin);
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
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    auto authenticator =
        ConstructAuthenticatorWithTimer(GURL(test_case.origin), task_runner);
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
    mojo::Remote<blink::mojom::InternalAuthenticator> authenticator =
        ConnectToAuthenticator(origin);
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
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    auto authenticator =
        ConstructAuthenticatorWithTimer(GURL(test_case.origin), task_runner);
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

}  // namespace content
