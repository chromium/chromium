// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/json/json_parser.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_service_manager_context.h"
#include "content/test/test_render_frame_host.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/scoped_virtual_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#endif

namespace content {

using ::testing::_;

using blink::mojom::AttestationConveyancePreference;
using blink::mojom::AuthenticatorPtr;
using blink::mojom::AuthenticatorSelectionCriteria;
using blink::mojom::AuthenticatorSelectionCriteriaPtr;
using blink::mojom::AuthenticatorStatus;
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
using blink::mojom::AuthenticatorTransport;
using cbor::Value;
using cbor::Reader;

namespace {

using InterestingFailureReason =
    ::content::AuthenticatorRequestClientDelegate::InterestingFailureReason;
using FailureReasonCallbackReceiver =
    ::device::test::TestCallbackReceiver<InterestingFailureReason>;

typedef struct {
  const char* origin;
  // Either a relying party ID or a U2F AppID.
  const char* claimed_authority;
} OriginClaimedAuthorityPair;

constexpr char kTestOrigin1[] = "https://a.google.com";
constexpr char kTestRelyingPartyId[] = "google.com";

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
    {"http://localhost", "localhost"},
    {"https://myawesomedomain", "myawesomedomain"},
    {"https://foo.bar.google.com", "foo.bar.google.com"},
    {"https://foo.bar.google.com", "bar.google.com"},
    {"https://foo.bar.google.com", "google.com"},
    {"https://earth.login.awesomecompany", "login.awesomecompany"},
    {"https://google.com:1337", "google.com"},

    // Hosts with trailing dot valid for rpIds with or without trailing dot.
    // Hosts without trailing dots only matches rpIDs without trailing dot.
    // Two trailing dots only matches rpIDs with two trailing dots.
    {"https://google.com.", "google.com"},
    {"https://google.com.", "google.com."},
    {"https://google.com..", "google.com.."},

    // Leading dots are ignored in canonicalized hosts.
    {"https://.google.com", "google.com"},
    {"https://..google.com", "google.com"},
    {"https://.google.com", ".google.com"},
    {"https://..google.com", ".google.com"},
    {"https://accounts.google.com", ".google.com"},
};

constexpr OriginClaimedAuthorityPair kInvalidRelyingPartyTestCases[] = {
    {"https://google.com", "com"},
    {"http://google.com", "google.com"},
    {"http://myawesomedomain", "myawesomedomain"},
    {"https://google.com", "foo.bar.google.com"},
    {"http://myawesomedomain", "randomdomain"},
    {"https://myawesomedomain", "randomdomain"},
    {"https://notgoogle.com", "google.com)"},
    {"https://not-google.com", "google.com)"},
    {"https://evil.appspot.com", "appspot.com"},
    {"https://evil.co.uk", "co.uk"},

    {"https://google.com", "google.com."},
    {"https://google.com", "google.com.."},
    {"https://google.com", ".google.com"},
    {"https://google.com..", "google.com"},
    {"https://.com", "com."},
    {"https://.co.uk", "co.uk."},

    {"https://1.2.3", "1.2.3"},
    {"https://1.2.3", "2.3"},

    {"https://127.0.0.1", "127.0.0.1"},
    {"https://127.0.0.1", "27.0.0.1"},
    {"https://127.0.0.1", ".0.0.1"},
    {"https://127.0.0.1", "0.0.1"},

    {"https://[::127.0.0.1]", "127.0.0.1"},
    {"https://[::127.0.0.1]", "[127.0.0.1]"},

    {"https://[::1]", "1"},
    {"https://[::1]", "1]"},
    {"https://[::1]", "::1"},
    {"https://[::1]", "[::1]"},
    {"https://[1::1]", "::1"},
    {"https://[1::1]", "::1]"},
    {"https://[1::1]", "[::1]"},

    {"http://google.com:443", "google.com"},
    {"data:google.com", "google.com"},
    {"data:text/html,google.com", "google.com"},
    {"ws://google.com", "google.com"},
    {"gopher://google.com", "google.com"},
    {"ftp://google.com", "google.com"},
    {"file:///google.com", "google.com"},
    // Use of webauthn from a WSS origin may be technically valid, but we
    // prohibit use on non-HTTPS origins. (At least for now.)
    {"wss://google.com", "google.com"},

    {"data:,", ""},
    {"https://google.com", ""},
    {"ws:///google.com", ""},
    {"wss:///google.com", ""},
    {"gopher://google.com", ""},
    {"ftp://google.com", ""},
    {"file:///google.com", ""},

    // This case is acceptable according to spec, but both renderer
    // and browser handling currently do not permit it.
    {"https://login.awesomecompany", "awesomecompany"},

    // These are AppID test cases, but should also be invalid relying party
    // examples too.
    {"https://example.com", "https://com/"},
    {"https://example.com", "https://com/foo"},
    {"https://example.com", "https://foo.com/"},
    {"https://example.com", "http://example.com"},
    {"http://example.com", "https://example.com"},
    {"https://127.0.0.1", "https://127.0.0.1"},
    {"https://www.notgoogle.com",
     "https://www.gstatic.com/securitykey/origins.json"},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json#x"},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json2"},
    {"https://www.google.com", "https://gstatic.com/securitykey/origins.json"},
    {"https://ggoogle.com", "https://www.gstatic.com/securitykey/origi"},
    {"https://com", "https://www.gstatic.com/securitykey/origins.json"},
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

PublicKeyCredentialRpEntityPtr GetTestPublicKeyCredentialRPEntity() {
  auto entity = PublicKeyCredentialRpEntity::New();
  entity->id = std::string(kTestRelyingPartyId);
  entity->name = "TestRP@example.com";
  return entity;
}

PublicKeyCredentialUserEntityPtr GetTestPublicKeyCredentialUserEntity() {
  auto entity = PublicKeyCredentialUserEntity::New();
  entity->display_name = "User A. Name";
  std::vector<uint8_t> id(32, 0x0A);
  entity->id = id;
  entity->name = "username@example.com";
  entity->icon = GURL("fakeurl2.png");
  return entity;
}

std::vector<PublicKeyCredentialParametersPtr>
GetTestPublicKeyCredentialParameters(int32_t algorithm_identifier) {
  std::vector<PublicKeyCredentialParametersPtr> parameters;
  auto fake_parameter = PublicKeyCredentialParameters::New();
  fake_parameter->type = blink::mojom::PublicKeyCredentialType::PUBLIC_KEY;
  fake_parameter->algorithm_identifier = algorithm_identifier;
  parameters.push_back(std::move(fake_parameter));
  return parameters;
}

AuthenticatorSelectionCriteriaPtr GetTestAuthenticatorSelectionCriteria() {
  auto criteria = AuthenticatorSelectionCriteria::New();
  criteria->authenticator_attachment =
      blink::mojom::AuthenticatorAttachment::NO_PREFERENCE;
  criteria->require_resident_key = false;
  criteria->user_verification =
      blink::mojom::UserVerificationRequirement::PREFERRED;
  return criteria;
}

std::vector<PublicKeyCredentialDescriptorPtr> GetTestAllowCredentials() {
  std::vector<PublicKeyCredentialDescriptorPtr> descriptors;
  auto credential = PublicKeyCredentialDescriptor::New();
  credential->type = PublicKeyCredentialType::PUBLIC_KEY;
  std::vector<uint8_t> id(32, 0x0A);
  credential->id = id;
  credential->transports.push_back(AuthenticatorTransport::USB);
  credential->transports.push_back(AuthenticatorTransport::BLE);
  descriptors.push_back(std::move(credential));
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
  options->user_verification =
      blink::mojom::UserVerificationRequirement::PREFERRED;
  options->allow_credentials = GetTestAllowCredentials();
  return options;
}

}  // namespace

class AuthenticatorImplTest : public content::RenderViewHostTestHarness {
 public:
  AuthenticatorImplTest() {}
  ~AuthenticatorImplTest() override {}

 protected:
  void TearDown() override {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    authenticator_impl_.reset();
    content::RenderViewHostTestHarness::TearDown();
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

  AuthenticatorPtr ConnectToAuthenticator() {
    authenticator_impl_ = std::make_unique<AuthenticatorImpl>(main_rfh());
    AuthenticatorPtr authenticator;
    authenticator_impl_->Bind(mojo::MakeRequest(&authenticator));
    return authenticator;
  }

  AuthenticatorPtr ConnectToAuthenticator(
      service_manager::Connector* connector,
      std::unique_ptr<base::OneShotTimer> timer) {
    authenticator_impl_.reset(
        new AuthenticatorImpl(main_rfh(), connector, std::move(timer)));
    AuthenticatorPtr authenticator;
    authenticator_impl_->Bind(mojo::MakeRequest(&authenticator));
    return authenticator;
  }

  AuthenticatorPtr ConstructAuthenticatorWithTimer(
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner) {
    connector_ = service_manager::Connector::Create(&request_);
    fake_hid_manager_ = std::make_unique<device::FakeHidManager>();
    service_manager::Connector::TestApi test_api(connector_.get());
    test_api.OverrideBinderForTesting(
        service_manager::Identity(device::mojom::kServiceName),
        device::mojom::HidManager::Name_,
        base::Bind(&device::FakeHidManager::AddBinding,
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
    return AuthenticatorImpl::SerializeCollectedClientDataToJson(
        std::move(type), GetTestOrigin(), GetTestChallengeBytes());
  }

  AuthenticatorStatus TryAuthenticationWithAppId(const std::string& origin,
                                                 const std::string& appid) {
    const GURL origin_url(origin);
    NavigateAndCommit(origin_url);
    AuthenticatorPtr authenticator = ConnectToAuthenticator();
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

  bool SupportsTransportProtocol(::device::FidoTransportProtocol protocol) {
    return base::ContainsKey(
        authenticator_impl_->enabled_transports_for_testing(), protocol);
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
  service_manager::mojom::ConnectorRequest request_;
  std::unique_ptr<service_manager::Connector> connector_;
  std::unique_ptr<device::FakeHidManager> fake_hid_manager_;
  base::Optional<base::test::ScopedFeatureList> scoped_feature_list_;
};

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, MakeCredentialOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (auto test_case : kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));
    AuthenticatorPtr authenticator = ConnectToAuthenticator();
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party->id = test_case.claimed_authority;
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::INVALID_DOMAIN, callback_receiver.status());
  }

  // These instances time out with NOT_ALLOWED_ERROR due to unsupported
  // algorithm.
  for (auto test_case : kValidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));
    device::test::ScopedVirtualFidoDevice scoped_virtual_device;
    auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
        base::Time::Now(), base::TimeTicks::Now());
    auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party->id = test_case.claimed_authority;
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

// Test that MakeCredential request times out with NOT_ALLOWED_ERROR if no
// parameters contain a supported algorithm.
TEST_F(AuthenticatorImplTest, MakeCredentialNoSupportedAlgorithm) {
  SimulateNavigation(GURL(kTestOrigin1));
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
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

// Test that service returns NOT_ALLOWED_ERROR if user verification is REQUIRED
// for get().
TEST_F(AuthenticatorImplTest, GetAssertionUserVerification) {
  SimulateNavigation(GURL(kTestOrigin1));
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->user_verification =
      blink::mojom::UserVerificationRequirement::REQUIRED;
  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

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
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->user_verification =
      blink::mojom::UserVerificationRequirement::REQUIRED;

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
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->require_resident_key = true;

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
  options->authenticator_selection->authenticator_attachment =
      blink::mojom::AuthenticatorAttachment::PLATFORM;

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
  std::unique_ptr<base::Value> subset(base::JSONReader::Read(subset_str));
  ASSERT_TRUE(subset);
  ASSERT_TRUE(subset->is_dict());
  std::unique_ptr<base::Value> test(base::JSONReader::Read(test_str));
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
    AuthenticatorPtr authenticator = ConnectToAuthenticator();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::INVALID_DOMAIN, callback_receiver.status());
  }
}

constexpr OriginClaimedAuthorityPair kValidAppIdCases[] = {
    {"https://example.com", "https://example.com"},
    {"https://www.example.com", "https://example.com"},
    {"https://example.com", "https://www.example.com"},
    {"https://example.com", "https://foo.bar.example.com"},
    {"https://example.com", "https://foo.bar.example.com/foo/bar"},
    {"https://google.com", "https://www.gstatic.com/securitykey/origins.json"},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json"},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/a/google.com/origins.json"},
    {"https://accounts.google.com",
     "https://www.gstatic.com/securitykey/origins.json"},
};

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, AppIdExtensionValues) {
  TestServiceManagerContext smc;
  device::test::ScopedVirtualFidoDevice virtual_device;

  for (const auto& test_case : kValidAppIdCases) {
    SCOPED_TRACE(std::string(test_case.origin) + " " +
                 std::string(test_case.claimed_authority));

    EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_NOT_RECOGNIZED,
              TryAuthenticationWithAppId(test_case.origin,
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

    EXPECT_EQ(AuthenticatorStatus::INVALID_DOMAIN,
              TryAuthenticationWithAppId(test_case.origin,
                                         test_case.claimed_authority));
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
    device::test::ScopedVirtualFidoDevice virtual_device;
    ASSERT_TRUE(virtual_device.mutable_state()->InjectRegistration(
        options->allow_credentials[0]->id, kTestRelyingPartyId));

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
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    device::test::ScopedVirtualFidoDevice virtual_device;
    ASSERT_TRUE(virtual_device.mutable_state()->InjectRegistration(
        options->allow_credentials[0]->id, kTestRelyingPartyId));

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
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    device::test::ScopedVirtualFidoDevice virtual_device;
    // Inject a registration for the URL (which is a U2F AppID).
    ASSERT_TRUE(virtual_device.mutable_state()->InjectRegistration(
        options->allow_credentials[0]->id, kTestOrigin1));

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

TEST_F(AuthenticatorImplTest, TestGetAssertionTimeout) {
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
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
  TestServiceManagerContext service_manager_context;

  // 255 is the maximum size of a U2F credential ID. We also test one greater
  // (256) to ensure that nothing untoward happens.
  const std::vector<size_t> kSizes = {255, 256};

  for (const size_t size : kSizes) {
    SCOPED_TRACE(size);

    SimulateNavigation(GURL(kTestOrigin1));
    AuthenticatorPtr authenticator = ConnectToAuthenticator();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    auto credential = PublicKeyCredentialDescriptor::New();
    credential->type = PublicKeyCredentialType::PUBLIC_KEY;
    credential->id.resize(size);
    credential->transports.push_back(AuthenticatorTransport::USB);

    const bool should_be_valid = size < 256;
    if (should_be_valid) {
      ASSERT_TRUE(scoped_virtual_device.mutable_state()->InjectRegistration(
          credential->id, kTestRelyingPartyId));
    }

    options->allow_credentials.emplace_back(std::move(credential));

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();

    if (should_be_valid) {
      EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
    } else {
      EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_NOT_RECOGNIZED,
                callback_receiver.status());
    }
  }
}

TEST_F(AuthenticatorImplTest, TestCableDiscoveryByDefault) {
  auto authenticator = ConnectToAuthenticator();

  // caBLE should be enabled by default if BLE is supported.
  EXPECT_EQ(
      device::BluetoothAdapterFactory::Get().IsLowEnergySupported(),
      SupportsTransportProtocol(
          device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy));
}

TEST_F(AuthenticatorImplTest, TestCableDiscoveryDisabledWithFlag) {
  DisableFeature(features::kWebAuthCable);

  auto authenticator = ConnectToAuthenticator();
  EXPECT_FALSE(SupportsTransportProtocol(
      device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy));
}

#if defined(OS_WIN)
TEST_F(AuthenticatorImplTest, TestCableDiscoveryEnabledWithWinFlag) {
  EnableFeature(features::kWebAuthCableWin);

  auto authenticator = ConnectToAuthenticator();

  // Should be enabled if the new Windows BLE stack is.
  EXPECT_EQ(
      device::BluetoothAdapterFactory::Get().IsLowEnergySupported(),
      SupportsTransportProtocol(
          device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy));
}

// Tests that caBLE is not supported when features::kWebAuthCable is disabled,
// regardless of the state of features::kWebAuthCableWin.
TEST_F(AuthenticatorImplTest, TestCableDiscoveryDisabledWithoutFlagWin) {
  for (bool enable_win_flag : {false, true}) {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features = {features::kWebAuthCable};
    enable_win_flag ? enabled_features.push_back(features::kWebAuthCableWin)
                    : disabled_features.push_back(features::kWebAuthCableWin);

    scoped_feature_list_.emplace();
    scoped_feature_list_->InitWithFeatures(enabled_features, disabled_features);

    auto authenticator = ConnectToAuthenticator();
    EXPECT_FALSE(SupportsTransportProtocol(
        device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy));
  }
}
#endif

TEST_F(AuthenticatorImplTest, TestGetAssertionU2fDeviceBackwardsCompatibility) {
  SimulateNavigation(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  device::test::ScopedVirtualFidoDevice virtual_device;
  // Inject credential ID to the virtual device so that successful sign in is
  // possible.
  ASSERT_TRUE(virtual_device.mutable_state()->InjectRegistration(
      options->allow_credentials[0]->id, kTestRelyingPartyId));

  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  // Trigger timer.
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());
}

TEST_F(AuthenticatorImplTest, GetAssertionWithEmptyAllowCredentials) {
  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<device::MockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent())
      .WillRepeatedly(::testing::Return(true));
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  auto bluetooth_adapter_factory_overrides =
      device::BluetoothAdapterFactory::Get().InitGlobalValuesForTesting();
  bluetooth_adapter_factory_overrides->SetLESupported(true);

  SimulateNavigation(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials.clear();
  TestGetAssertionCallback callback_receiver;

  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>(
      base::Time::Now(), base::TimeTicks::Now());
  auto authenticator = ConstructAuthenticatorWithTimer(task_runner);
  device::test::ScopedVirtualFidoDevice virtual_device;
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  // Trigger timer.
  base::RunLoop().RunUntilIdle();
  task_runner->FastForwardBy(base::TimeDelta::FromMinutes(1));
  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED,
            callback_receiver.status());
}

TEST_F(AuthenticatorImplTest, MakeCredentialAlreadyRegistered) {
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
  TestServiceManagerContext service_manager_context;

  SimulateNavigation(GURL(kTestOrigin1));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  // Exclude the one already registered credential.
  options->exclude_credentials = GetTestAllowCredentials();
  ASSERT_TRUE(scoped_virtual_device.mutable_state()->InjectRegistration(
      options->exclude_credentials[0]->id, kTestRelyingPartyId));

  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());
  callback_receiver.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_EXCLUDED,
            callback_receiver.status());
}

TEST_F(AuthenticatorImplTest, MakeCredentialPendingRequest) {
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
  TestServiceManagerContext service_manager_context;

  SimulateNavigation(GURL(kTestOrigin1));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

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
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
  TestServiceManagerContext service_manager_context;

  SimulateNavigation(GURL(kTestOrigin1));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

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
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
  TestServiceManagerContext service_manager_context;

  SimulateNavigation(GURL(kTestOrigin1));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

  base::RunLoop run_loop;
  authenticator.set_connection_error_handler(run_loop.QuitClosure());

  // Make first request.
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  // Delete the |AuthenticatorImpl| during the registration operation to
  // simulate a navigation while waiting for the user to press the token.
  scoped_virtual_device.mutable_state()->simulate_press_callback =
      base::BindRepeating(
          [](std::unique_ptr<AuthenticatorImpl>* ptr) {
            base::ThreadTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindOnce(
                               [](std::unique_ptr<AuthenticatorImpl>* ptr) {
                                 ptr->reset();
                               },
                               ptr));
          },
          &authenticator_impl_);

  run_loop.Run();
}

TEST_F(AuthenticatorImplTest, InvalidResponse) {
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
  TestServiceManagerContext service_manager_context;

  scoped_virtual_device.mutable_state()->simulate_invalid_response = true;
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
  U2F,
  SELF,
};

class TestAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  TestAuthenticatorRequestDelegate(
      RenderFrameHost* render_frame_host,
      base::OnceClosure action_callbacks_registered_callback,
      IndividualAttestation individual_attestation,
      AttestationConsent attestation_consent,
      bool is_focused)
      : action_callbacks_registered_callback_(
            std::move(action_callbacks_registered_callback)),
        individual_attestation_(individual_attestation),
        attestation_consent_(attestation_consent),
        is_focused_(is_focused) {}
  ~TestAuthenticatorRequestDelegate() override {}

  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback,
      device::FidoRequestHandlerBase::BlePairingCallback ble_pairing_callback)
      override {
    ASSERT_TRUE(action_callbacks_registered_callback_)
        << "RegisterActionCallbacks called twice.";
    std::move(action_callbacks_registered_callback_).Run();
  }

  bool ShouldPermitIndividualAttestation(
      const std::string& relying_party_id) override {
    return individual_attestation_ == IndividualAttestation::REQUESTED;
  }

  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(attestation_consent_ ==
                            AttestationConsent::GRANTED);
  }

  bool IsFocused() override { return is_focused_; }

  base::OnceClosure action_callbacks_registered_callback_;
  const IndividualAttestation individual_attestation_;
  const AttestationConsent attestation_consent_;
  const bool is_focused_;

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
        individual_attestation, attestation_consent, is_focused);
  }

#if defined(OS_MACOSX)
  bool IsWebAuthenticationTouchIdAuthenticatorSupported() override {
    return supports_touch_id;
  }

  bool supports_touch_id = true;
#endif

  // If set, this closure will be called when the subsequently constructed
  // delegate is informed that the request has started.
  base::OnceClosure action_callbacks_registered_callback;

  IndividualAttestation individual_attestation =
      IndividualAttestation::NOT_REQUESTED;
  AttestationConsent attestation_consent = AttestationConsent::DENIED;
  bool is_focused = true;

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
    AuthenticatorPtr authenticator = ConnectToAuthenticator();

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
      options->relying_party->id = "example.com";
      options->adjusted_timeout = base::TimeDelta::FromSeconds(1);
      options->attestation = test.attestation_requested;
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

      switch (test.expected_attestation) {
        case AttestationType::ANY:
          ASSERT_STREQ("", test.expected_certificate_substring);
          break;

        case AttestationType::NONE:
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "none");
          break;

        case AttestationType::U2F:
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "fido-u2f");
          if (strlen(test.expected_certificate_substring) > 0) {
            ExpectCertificateContainingSubstring(
                attestation, test.expected_certificate_substring);
          }
          break;

        case AttestationType::SELF:
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

          // The AAGUID should be all zero.
          const auto auth_data_it = attestation.find(Value("authData"));
          ASSERT_TRUE(auth_data_it != attestation.end());
          ASSERT_TRUE(auth_data_it->second.is_bytestring());
          const std::vector<uint8_t>& auth_data =
              auth_data_it->second.GetBytestring();
          base::Optional<device::AuthenticatorData> parsed_auth_data =
              device::AuthenticatorData::DecodeAuthenticatorData(auth_data);
          ASSERT_TRUE(parsed_auth_data);
          const base::Optional<device::AttestedCredentialData>& cred_data(
              parsed_auth_data->attested_data());
          ASSERT_TRUE(cred_data);
          EXPECT_TRUE(cred_data->IsAaguidZero());
          break;
      }
    }
  }

 protected:
  TestAuthenticatorContentBrowserClient test_client_;
  device::test::ScopedVirtualFidoDevice virtual_device_;

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

  // Asserts that the webauthn attestation CBOR map in
  // |attestation| contains a single X.509 certificate containing |substring|.
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
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS, AttestationType::NONE, "",
      },
      {
          AttestationConveyancePreference::NONE,
          IndividualAttestation::REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS, AttestationType::NONE, "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::NOT_ALLOWED_ERROR, AttestationType::ANY, "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          IndividualAttestation::REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::NOT_ALLOWED_ERROR, AttestationType::ANY, "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS, AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::INDIRECT,
          IndividualAttestation::REQUESTED, AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS, AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::NOT_ALLOWED_ERROR, AttestationType::ANY, "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::NOT_ALLOWED_ERROR, AttestationType::ANY, "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS, AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::REQUESTED, AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS, AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::NOT_ALLOWED_ERROR, AttestationType::ANY, "",
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::NOT_ALLOWED_ERROR, AttestationType::ANY, "",
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS, AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::REQUESTED, AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS, AttestationType::U2F,
          kIndividualCommonName,
      },
  };

  virtual_device_.mutable_state()->attestation_cert_common_name =
      kStandardCommonName;
  virtual_device_.mutable_state()->individual_attestation_cert_common_name =
      kIndividualCommonName;
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
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::NOT_ALLOWED_ERROR, AttestationType::ANY, "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          // If individual attestation was not requested then the attestation
          // certificate will be removed, even if consent is given, because
          // the consent isn't to be tracked.
          AttestationType::NONE, "",
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          // If individual attestation was not requested then the attestation
          // certificate will be removed, even if consent is given, because
          // the consent isn't to be tracked.
          AttestationType::NONE, "",
      },

      {
          AttestationConveyancePreference::ENTERPRISE,
          IndividualAttestation::REQUESTED, AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS, AttestationType::U2F, kCommonName,
      },
  };

  virtual_device_.mutable_state()->attestation_cert_common_name = kCommonName;
  virtual_device_.mutable_state()->individual_attestation_cert_common_name =
      kCommonName;
  NavigateAndCommit(GURL("https://example.com"));

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest, Ctap2SelfAttestation) {
  virtual_device_.SetSupportedProtocol(device::ProtocolVersion::kCtap);
  virtual_device_.mutable_state()->self_attestation = true;
  NavigateAndCommit(GURL("https://example.com"));

  const std::vector<TestCase> kTests = {
      {
          // If no attestation is requested, we'll return the self attestation
          // rather than erasing it.
          AttestationConveyancePreference::NONE,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS, AttestationType::SELF, "",
      },
      {
          // If attestation is requested, but denied, we'll still fail the
          // request.
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::NOT_ALLOWED_ERROR, AttestationType::ANY, "",
      },
      {
          // If attestation is requested and granted, the self attestation
          // will be returned.
          AttestationConveyancePreference::DIRECT,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS, AttestationType::SELF, "",
      },
  };

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       Ctap2SelfAttestationNonZeroAaguid) {
  virtual_device_.SetSupportedProtocol(device::ProtocolVersion::kCtap);
  virtual_device_.mutable_state()->self_attestation = true;
  virtual_device_.mutable_state()->non_zero_aaguid_with_self_attestation = true;
  NavigateAndCommit(GURL("https://example.com"));

  const std::vector<TestCase> kTests = {
      {
          // Since the virtual device is configured to set a non-zero AAGUID
          // the self-attestation should still be replaced with a "none"
          // attestation.
          AttestationConveyancePreference::NONE,
          IndividualAttestation::NOT_REQUESTED, AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS, AttestationType::NONE, "",
      },
  };

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       MakeCredentialRequestStartedCallback) {
  TestServiceManagerContext smc;
  NavigateAndCommit(GURL(kTestOrigin1));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

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
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

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
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

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

    auto credential = PublicKeyCredentialDescriptor::New();
    credential->type = PublicKeyCredentialType::PUBLIC_KEY;
    credential->id.resize(16);
    credential->transports = {AuthenticatorTransport::USB};

    ASSERT_TRUE(virtual_device_.mutable_state()->InjectRegistration(
        credential->id, kTestRelyingPartyId));
    options->allow_credentials.emplace_back(std::move(credential));

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
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

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

#if defined(OS_MACOSX)
TEST_F(AuthenticatorContentBrowserClientTest,
       IsUVPAAFalseIfEmbedderDoesNotSupportTouchId) {
  if (__builtin_available(macOS 10.12.2, *)) {
    // Touch ID is hardware-supported, and flag-enabled, but not enabled by the
    // embedder.
    EnableFeature(device::kWebAuthTouchId);
    device::fido::mac::ScopedTouchIdTestEnvironment touch_id_test_environment;
    touch_id_test_environment.SetTouchIdAvailable(true);
    test_client_.supports_touch_id = false;

    NavigateAndCommit(GURL(kTestOrigin1));
    AuthenticatorPtr authenticator = ConnectToAuthenticator();

    TestIsUvpaaCallback cb;
    authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(cb.callback());
    cb.WaitForCallback();
    EXPECT_FALSE(cb.value());
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, IsUVPAAFalseIfFeatureFlagOff) {
  if (__builtin_available(macOS 10.12.2, *)) {
    // Touch ID is hardware-supported and embedder-enabled, but the flag is off.
    DisableFeature(device::kWebAuthTouchId);
    device::fido::mac::ScopedTouchIdTestEnvironment touch_id_test_environment;
    touch_id_test_environment.SetTouchIdAvailable(true);
    test_client_.supports_touch_id = true;

    NavigateAndCommit(GURL(kTestOrigin1));
    AuthenticatorPtr authenticator = ConnectToAuthenticator();

    TestIsUvpaaCallback cb;
    authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(cb.callback());
    cb.WaitForCallback();
    EXPECT_FALSE(cb.value());
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, IsUVPAATrueIfTouchIdAvailable) {
  if (__builtin_available(macOS 10.12.2, *)) {
    // Touch ID is available.
    EnableFeature(device::kWebAuthTouchId);
    device::fido::mac::ScopedTouchIdTestEnvironment touch_id_test_environment;
    touch_id_test_environment.SetTouchIdAvailable(true);
    test_client_.supports_touch_id = true;

    NavigateAndCommit(GURL(kTestOrigin1));
    AuthenticatorPtr authenticator = ConnectToAuthenticator();

    TestIsUvpaaCallback cb;
    authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(cb.callback());
    cb.WaitForCallback();
    EXPECT_TRUE(cb.value());
  }
}
#endif  // defined(OS_MACOSX)

#if !defined(OS_MACOSX)
TEST_F(AuthenticatorContentBrowserClientTest, IsUVPAAFalse) {
  // No platform authenticator on non-macOS platforms.
  NavigateAndCommit(GURL(kTestOrigin1));
  AuthenticatorPtr authenticator = ConnectToAuthenticator();

  TestIsUvpaaCallback cb;
  authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(cb.callback());
  cb.WaitForCallback();
  EXPECT_FALSE(cb.value());
}
#endif  // !defined(OS_MACOSX)

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
            true /* is_focused */),
        failure_reasons_callback_(std::move(failure_reasons_callback)) {}
  ~MockAuthenticatorRequestDelegateObserver() override = default;

  void DidFailWithInterestingReason(InterestingFailureReason reason) override {
    ASSERT_TRUE(failure_reasons_callback_);
    std::move(failure_reasons_callback_).Run(reason);
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

// Fake test construct that shares all other behavior with AuthenticatorImpl
// except that:
//  - FakeAuthenticatorImpl does not trigger UI activity.
//  - MockAuthenticatorRequestDelegateObserver is injected to
//  |request_delegate_|
//    instead of ChromeAuthenticatorRequestDelegate.
class FakeAuthenticatorImpl : public AuthenticatorImpl {
 public:
  explicit FakeAuthenticatorImpl(
      RenderFrameHost* render_frame_host,
      service_manager::Connector* connector,
      std::unique_ptr<base::OneShotTimer> timer,
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate)
      : AuthenticatorImpl(render_frame_host, connector, std::move(timer)),
        mock_delegate_(std::move(mock_delegate)) {}
  ~FakeAuthenticatorImpl() override = default;

  void UpdateRequestDelegate() override {
    DCHECK(mock_delegate_);
    request_delegate_ = std::move(mock_delegate_);
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

  AuthenticatorPtr ConnectToFakeAuthenticator(
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> delegate,
      service_manager::Connector* connector,
      std::unique_ptr<base::OneShotTimer> timer) {
    authenticator_impl_.reset(new FakeAuthenticatorImpl(
        main_rfh(), connector, std::move(timer), std::move(delegate)));
    AuthenticatorPtr authenticator;
    authenticator_impl_->Bind(mojo::MakeRequest(&authenticator));
    return authenticator;
  }

  AuthenticatorPtr ConstructFakeAuthenticatorWithTimer(
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> delegate,
      scoped_refptr<base::TestMockTimeTaskRunner> task_runner) {
    connector_ = service_manager::Connector::Create(&request_);
    fake_hid_manager_ = std::make_unique<device::FakeHidManager>();
    service_manager::Connector::TestApi test_api(connector_.get());
    test_api.OverrideBinderForTesting(
        service_manager::Identity(device::mojom::kServiceName),
        device::mojom::HidManager::Name_,
        base::Bind(&device::FakeHidManager::AddBinding,
                   base::Unretained(fake_hid_manager_.get())));

    // Set up a timer for testing.
    auto timer =
        std::make_unique<base::OneShotTimer>(task_runner->GetMockTickClock());
    timer->SetTaskRunner(task_runner);
    return ConnectToFakeAuthenticator(std::move(delegate), connector_.get(),
                                      std::move(timer));
  }

 protected:
  std::unique_ptr<FakeAuthenticatorImpl> authenticator_impl_;
};

TEST_F(AuthenticatorImplRequestDelegateTest,
       TestRequestDelegateObservesFidoRequestHandler) {
  EnableFeature(features::kWebAuthBle);
  auto mock_adapter =
      base::MakeRefCounted<::testing::NiceMock<device::MockBluetoothAdapter>>();
  EXPECT_CALL(*mock_adapter, IsPresent())
      .WillRepeatedly(::testing::Return(true));
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);
  auto bluetooth_adapter_factory_overrides =
      device::BluetoothAdapterFactory::Get().InitGlobalValuesForTesting();
  bluetooth_adapter_factory_overrides->SetLESupported(true);

  device::test::ScopedFakeFidoDiscoveryFactory discovery_factory;
  auto* fake_ble_discovery = discovery_factory.ForgeNextBleDiscovery();

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
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
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
  options->exclude_credentials = GetTestAllowCredentials();
  ASSERT_TRUE(scoped_virtual_device.mutable_state()->InjectRegistration(
      options->exclude_credentials[0]->id, kTestRelyingPartyId));

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
  device::test::ScopedVirtualFidoDevice scoped_virtual_device;
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
  EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_NOT_RECOGNIZED,
            callback_receiver.status());

  ASSERT_TRUE(failure_reason_receiver.was_called());
  EXPECT_EQ(content::AuthenticatorRequestClientDelegate::
                InterestingFailureReason::kKeyNotRegistered,
            std::get<0>(*failure_reason_receiver.result()));
}

TEST_F(AuthenticatorImplTest, Transports) {
  TestServiceManagerContext smc;
  NavigateAndCommit(GURL(kTestOrigin1));

  for (auto protocol :
       {device::ProtocolVersion::kU2f, device::ProtocolVersion::kCtap}) {
    SCOPED_TRACE(static_cast<int>(protocol));

    device::test::ScopedVirtualFidoDevice scoped_virtual_device;
    scoped_virtual_device.SetSupportedProtocol(protocol);

    AuthenticatorPtr authenticator = ConnectToAuthenticator();
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, callback_receiver.status());

    const std::vector<blink::mojom::AuthenticatorTransport>& transports(
        callback_receiver.value()->transports);
    ASSERT_EQ(2u, transports.size());
    EXPECT_EQ(blink::mojom::AuthenticatorTransport::USB, transports[0]);
    // VirtualFidoDevice generates an attestation certificate that asserts NFC
    // support via an extension.
    EXPECT_EQ(blink::mojom::AuthenticatorTransport::NFC, transports[1]);
  }
}

TEST_F(AuthenticatorImplTest, ExtensionHMACSecret) {
  TestServiceManagerContext smc;
  NavigateAndCommit(GURL(kTestOrigin1));

  for (const bool include_extension : {false, true}) {
    SCOPED_TRACE(include_extension);

    device::test::ScopedVirtualFidoDevice scoped_virtual_device;
    scoped_virtual_device.SetSupportedProtocol(device::ProtocolVersion::kCtap);

    AuthenticatorPtr authenticator = ConnectToAuthenticator();
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

}  // namespace content
