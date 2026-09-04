// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/apple/owned_objc.h"
#include "base/base64url.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/stack_allocated.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/webauth/authenticator_common_impl.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/authenticator_impl_unittest_test_base.h"
#include "content/browser/webauth/authenticator_request_outcome_enums.h"
#include "content/browser/webauth/authenticator_test_base.h"
#include "content/browser/webauth/client_data_json.h"
#include "content/browser/webauth/default_authenticator_request_client_delegate.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_authenticator_manager_impl.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_authentication_delegate.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "content/public/browser/webauth_request_security_checker.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "crypto/evp.h"
#include "crypto/hash.h"
#include "crypto/hmac.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_user_verification_requirement.h"
#include "device/fido/filter.h"
#include "device/fido/large_blob.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/multiple_virtual_fido_device_factory.h"
#include "device/fido/pin.h"
#include "device/fido/public/authenticator_selection_criteria.h"
#include "device/fido/public/features.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/public/fido_types.h"
#include "device/fido/public/public_key_credential_descriptor.h"
#include "device/fido/public/public_key_credential_params.h"
#include "device/fido/public/public_key_credential_rp_entity.h"
#include "device/fido/public/public_key_credential_user_entity.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/data_decoder/gzipper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "url/origin.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/files/file_path.h"
#include "base/path_service.h"
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

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#include "device/bluetooth/floss/floss_features.h"
#endif

namespace content {

using ::testing::_;

using blink::mojom::AttestationConveyancePreference;
using blink::mojom::AuthenticationExtensionsClientInputs;
using blink::mojom::AuthenticationExtensionsClientOutputs;
using blink::mojom::AuthenticatorSelectionCriteria;
using blink::mojom::AuthenticatorSelectionCriteriaPtr;
using blink::mojom::AuthenticatorStatus;
using blink::mojom::AuthenticatorTransport;
using blink::mojom::CommonCredentialInfo;
using blink::mojom::GetAssertionAuthenticatorResponse;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
using blink::mojom::GetCredentialOptions;
using blink::mojom::GetCredentialOptionsPtr;
using blink::mojom::MakeCredentialAuthenticatorResponse;
using blink::mojom::MakeCredentialAuthenticatorResponsePtr;
using blink::mojom::PublicKeyCredentialCreationOptions;
using blink::mojom::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::PublicKeyCredentialDescriptor;
using blink::mojom::PublicKeyCredentialDescriptorPtr;
using blink::mojom::PublicKeyCredentialParameters;
using blink::mojom::PublicKeyCredentialParametersPtr;
using blink::mojom::PublicKeyCredentialReportOptions;
using blink::mojom::PublicKeyCredentialReportOptionsPtr;
using blink::mojom::PublicKeyCredentialRequestOptions;
using blink::mojom::PublicKeyCredentialRequestOptionsPtr;
using blink::mojom::PublicKeyCredentialRpEntity;
using blink::mojom::PublicKeyCredentialRpEntityPtr;
using blink::mojom::PublicKeyCredentialType;
using blink::mojom::PublicKeyCredentialUserEntity;
using blink::mojom::PublicKeyCredentialUserEntityPtr;
using blink::mojom::RemoteDesktopClientOverride;
using blink::mojom::RemoteDesktopClientOverridePtr;
using blink::mojom::WebAuthnDOMExceptionDetails;
using blink::mojom::WebAuthnDOMExceptionDetailsPtr;
using cbor::Reader;
using cbor::Value;
using device::VirtualCtap2Device;
using device::VirtualFidoDevice;

namespace {

constexpr char kTestRegisterClientDataJsonString[] =
    R"({"challenge":"aHE0loIi7BcgLkJQX47SsWriLxa7BbiMJdueYCZF8UE","origin":)"
    R"("https://a.google.com", "type":"webauthn.create"})";

constexpr char kTestSignClientDataJsonString[] =
    R"({"challenge":"aHE0loIi7BcgLkJQX47SsWriLxa7BbiMJdueYCZF8UE","origin":)"
    R"("https://a.google.com", "type":"webauthn.get"})";

}  // namespace

TEST_F(AuthenticatorImplTest, ClientDataJSONSerialization) {
  // First test that the output is in the expected form. Some verifiers may be
  // depending on the exact JSON serialisation. Since the serialisation may add
  // extra elements, this can only test that the expected value is a prefix of
  // the returned value.
  std::vector<uint8_t> challenge_bytes = {1, 2, 3};
  EXPECT_EQ(
      BuildClientDataJson({webauthn::ClientDataRequestType::kWebAuthnCreate,
                           GetTestOrigin(), GetTestOrigin(), challenge_bytes,
                           false})
          .find(
              "{\"type\":\"webauthn.create\",\"challenge\":\"AQID\",\"origin\":"
              "\"https://a.google.com\",\"crossOrigin\":false"),
      0u);

  // Second, check that a generic JSON parser correctly parses the result.
  static const struct {
    const webauthn::ClientDataRequestType type;
    url::Origin origin;
    url::Origin top_origin;
    std::vector<uint8_t> challenge;
    bool is_cross_origin;
  } kTestCases[] = {
      {
          webauthn::ClientDataRequestType::kWebAuthnGet,
          GetTestOrigin(),
          GetTestOrigin(),
          {1, 2, 3},
          false,
      },
      {
          webauthn::ClientDataRequestType::kPaymentGet,
          GetTestOrigin(),
          GetTestOrigin(),
          {1, 2, 3},
          false,
      },
      {
          webauthn::ClientDataRequestType::kWebAuthnCreate,
          GetTestOrigin(),
          url::Origin::Create(GURL("https://toplevel.example")),
          {1, 2, 3},
          false,
      },
  };

  size_t num = 0;
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(num++);

    const std::string json =
        BuildClientDataJson({test.type, test.origin, test.top_origin,
                             test.challenge, test.is_cross_origin});

    const auto parsed =
        base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    ASSERT_TRUE(parsed.has_value());
    std::string type_key;
    std::string expected_type;
    switch (test.type) {
      case webauthn::ClientDataRequestType::kWebAuthnCreate:
        type_key = "type";
        expected_type = "webauthn.create";
        break;
      case webauthn::ClientDataRequestType::kWebAuthnGet:
        type_key = "type";
        expected_type = "webauthn.get";
        break;
      case webauthn::ClientDataRequestType::kPaymentGet:
        type_key = "type";
        expected_type = "payment.get";
        break;
    }
    ASSERT_TRUE(parsed->is_dict());
    EXPECT_EQ(*parsed->GetDict().FindString(type_key), expected_type);
    EXPECT_EQ(*parsed->GetDict().FindString("origin"), test.origin.Serialize());
    std::string expected_challenge;
    base::Base64UrlEncode(test.challenge,
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &expected_challenge);
    EXPECT_EQ(*parsed->GetDict().FindString("challenge"), expected_challenge);
    EXPECT_EQ(*parsed->GetDict().FindBool("crossOrigin"), test.is_cross_origin);
    if (test.is_cross_origin) {
      EXPECT_EQ(*parsed->GetDict().FindString("topOrigin"),
                test.top_origin.Serialize());
    } else {
      EXPECT_EQ(parsed->GetDict().FindString("topOrigin"), nullptr);
    }
  }
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, MakeCredentialOriginAndRpIds) {
  std::vector<OriginClaimedAuthorityPair> tests;
  std::ranges::copy(webauthn::test::GetValidRpTestCases(),
                    std::back_inserter(tests));
  std::ranges::copy(webauthn::test::GetInvalidRpTestCases(),
                    std::back_inserter(tests));

  int test_case_count = 0;
  for (const auto& test_case : tests) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    NavigateAndCommit(GURL(test_case.origin));
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              test_case.expected_status);
    VerifyMakeCredentialOutcomeUkm(
        test_case_count++,
        (test_case.expected_status == AuthenticatorStatus::SUCCESS)
            ? MakeCredentialOutcome::kSuccess
            : MakeCredentialOutcome::kSecurityError,
        AuthenticationRequestMode::kModalWebAuthn);
  }
}

// Test that MakeCredential request times out with NOT_ALLOWED_ERROR if user
// verification is required for U2F devices.
TEST_F(AuthenticatorImplTest, MakeCredentialUserVerification) {
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->user_verification_requirement =
      device::UserVerificationRequirement::kRequired;

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, MakeCredentialResidentKeyUnsupported) {
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->resident_key =
      device::ResidentKeyRequirement::kRequired;

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kRkNotSupported,
                                 AuthenticationRequestMode::kModalWebAuthn);
}

// Test that MakeCredential request times out with NOT_ALLOWED_ERROR if a
// platform authenticator is requested for U2F devices.
TEST_F(AuthenticatorImplTest, MakeCredentialPlatformAuthenticator) {
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->authenticator_attachment =
      device::AuthenticatorAttachment::kPlatform;

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kUiTimeout,
                                 AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorImplTest, GetClientCapabilities) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{device::kWebAuthnAmbientSignin});

  NavigateAndCommit(GURL(kTestOrigin1));

  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();

  std::vector<std::string> capability_names;
  std::ranges::transform(
      capabilities, std::back_inserter(capability_names),
      [](const auto& capability) { return capability->name; });

  const std::vector<std::string_view> kRequiredCapabilities = {
      client_capabilities::kConditionalGet,
      client_capabilities::kHybridTransport,
      client_capabilities::kPasskeyPlatformAuthenticator,
      client_capabilities::kUserVerifyingPlatformAuthenticator,
      client_capabilities::kRelatedOrigins,
      client_capabilities::kConditionalCreate,
      client_capabilities::kSignalAllAcceptedCredentials,
      client_capabilities::kSignalCurrentUserDetails,
      client_capabilities::kSignalUnknownCredential,
      client_capabilities::kImmediateGet,
  };

  // Ensure no extra capabilities
  EXPECT_EQ(kRequiredCapabilities.size(), capabilities.size());

  // Check that each required capability is present exactly once.
  for (const auto& capability : kRequiredCapabilities) {
    EXPECT_EQ(1u, static_cast<size_t>(
                      std::ranges::count(capability_names, capability)));
  }
}

TEST_F(AuthenticatorImplTest, GetClientCapabilities_HybridTransportSupported) {
  NavigateAndCommit(GURL(kTestOrigin1));
  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
  ExpectCapability(capabilities, client_capabilities::kHybridTransport, true);
}

TEST_F(AuthenticatorImplTest,
       GetClientCapabilities_HybridTransport_NoBluetoothAdapter) {
  NavigateAndCommit(GURL(kTestOrigin1));
  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(false));
  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
  ExpectCapability(capabilities, client_capabilities::kHybridTransport, false);
}

TEST_F(AuthenticatorImplTest,
       GetClientCapabilities_HybridTransport_LowEnergyNotSupported) {
  SetBluetoothLESupported(false);

  NavigateAndCommit(GURL(kTestOrigin1));
  EXPECT_CALL(*mock_adapter_, IsPresent).Times(0);
  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
  ExpectCapability(capabilities, client_capabilities::kHybridTransport, false);
}

TEST_F(AuthenticatorImplTest, GetClientCapabilities_RelatedOrigins) {
  NavigateAndCommit(GURL(kTestOrigin1));
  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
  ExpectCapability(capabilities, client_capabilities::kRelatedOrigins, true);
}

TEST_F(AuthenticatorImplTest, GetClientCapabilities_ConditionalCreate) {
  NavigateAndCommit(GURL(kTestOrigin1));
  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
  ExpectCapability(capabilities, client_capabilities::kConditionalCreate, true);
}

TEST_F(AuthenticatorImplTest, GetClientCapabilities_AmbientGet) {
  for (const bool enabled : {false, true}) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatureState(device::kWebAuthnAmbientSignin, enabled);
    NavigateAndCommit(GURL(kTestOrigin1));
    ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
    ExpectCapability(capabilities, client_capabilities::kAmbientGet,
                     enabled ? std::optional<bool>(true) : std::nullopt);
  }
}

TEST_F(AuthenticatorImplTest, GetClientCapabilities_SignalApi) {
  NavigateAndCommit(GURL(kTestOrigin1));
  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
  ExpectCapability(capabilities,
                   client_capabilities::kSignalAllAcceptedCredentials, true);
  ExpectCapability(capabilities, client_capabilities::kSignalCurrentUserDetails,
                   true);
  ExpectCapability(capabilities, client_capabilities::kSignalUnknownCredential,
                   true);
}

// Parses its arguments as JSON and expects that all the keys in the first are
// also in the second, and with the same value.
static void CheckJSONIsSubsetOfJSON(std::string_view subset_str,
                                    std::string_view test_str) {
  std::optional<base::Value> subset =
      base::JSONReader::Read(subset_str, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(subset);
  ASSERT_TRUE(subset->is_dict());
  const base::DictValue& subset_dict = subset->GetDict();
  std::optional<base::Value> test =
      base::JSONReader::Read(test_str, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(test);
  ASSERT_TRUE(test->is_dict());
  const base::DictValue& test_dict = test->GetDict();

  for (auto item : subset_dict) {
    const base::Value* test_value = test_dict.Find(item.first);
    if (test_value == nullptr) {
      ADD_FAILURE() << item.first << " does not exist in the test dictionary";
      continue;
    }

    EXPECT_EQ(item.second, *test_value);
  }
}

// Test that client data serializes to JSON properly.
TEST(ClientDataSerializationTest, Register) {
  CheckJSONIsSubsetOfJSON(
      kTestRegisterClientDataJsonString,
      AuthenticatorImplTest::GetTestClientDataJSON(
          webauthn::ClientDataRequestType::kWebAuthnCreate));
}

TEST(ClientDataSerializationTest, Sign) {
  CheckJSONIsSubsetOfJSON(kTestSignClientDataJsonString,
                          AuthenticatorImplTest::GetTestClientDataJSON(
                              webauthn::ClientDataRequestType::kWebAuthnGet));
}

TEST_F(AuthenticatorImplTest, TestMakeCredentialTimeout) {
  base::HistogramTester histogram_tester;

  // Don't provide an authenticator tap so the request times out.
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting(
          [&](device::VirtualFidoDevice* device) { return false; });
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
  histogram_tester.ExpectUniqueSample("WebAuthentication.MakeCredential.Result",
                                      CredentialRequestResult::kTimeout, 1);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kUiTimeout,
                                 AuthenticationRequestMode::kModalWebAuthn);
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, GetAssertionOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (const OriginClaimedAuthorityPair& test_case :
       webauthn::test::GetInvalidRpTestCases()) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    NavigateAndCommit(GURL(test_case.origin));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              test_case.expected_status);
  }
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, ReportOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (const OriginClaimedAuthorityPair& test_case :
       webauthn::test::GetInvalidRpTestCases()) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    NavigateAndCommit(GURL(test_case.origin));
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = test_case.claimed_authority;
    options->unknown_credential_id = std::vector<uint8_t>(32, 0x0A);

    EXPECT_EQ(AuthenticatorReport(std::move(options)),
              test_case.expected_status);
  }
}

TEST_F(AuthenticatorImplTest, PdfProcessBlocked) {
  process()->SetIsPdf(true);

  mojo::Remote<blink::mojom::Authenticator> authenticator;
  static_cast<RenderFrameHostImpl*>(main_rfh())
      ->GetWebAuthenticationService(authenticator.BindNewPipeAndPassReceiver());

  EXPECT_EQ(1, process()->bad_msg_count());
}

constexpr auto kValidAppIdCases = std::to_array<OriginClaimedAuthorityPair>({
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
});

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, AppIdExtensionValues) {
  for (const auto& test_case : kValidAppIdCases) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              TryAuthenticationWithAppId(test_case.origin,
                                         test_case.claimed_authority));

    EXPECT_EQ(AuthenticatorStatus::SUCCESS,
              TryRegistrationWithAppIdExclude(test_case.origin,
                                              test_case.claimed_authority));
  }

  // All the invalid relying party test cases should also be invalid as AppIDs.
  for (const auto& test_case : webauthn::test::GetInvalidRpTestCases()) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    if (test_case.claimed_authority.empty()) {
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

// Verify that a credential registered with U2F can be used via webauthn.
TEST_F(AuthenticatorImplTest, AppIdExtension) {
  NavigateAndCommit(GURL(kTestOrigin1));

  {
    // First, test that the appid extension isn't echoed at all when not
    // requested.
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id, kTestRelyingPartyId));

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->extensions->echo_appid_extension, false);
  }

  {
    // Second, test that the appid extension is echoed, but is false, when appid
    // is requested but not used.
    ResetVirtualDevice();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id, kTestRelyingPartyId));

    // This AppID won't be used because the RP ID will be tried (successfully)
    // first.
    options->extensions->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->extensions->echo_appid_extension, true);
    EXPECT_EQ(result.response->extensions->appid_extension, false);
  }

  {
    // Lastly, when used, the appid extension result should be "true".
    ResetVirtualDevice();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    // Inject a registration for the URL (which is a U2F AppID).
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id, kTestOrigin1));

    options->extensions->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->extensions->echo_appid_extension, true);
    EXPECT_EQ(result.response->extensions->appid_extension, true);
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
        options->allow_credentials[0].id, kTestOrigin1));

    options->extensions->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->extensions->echo_appid_extension, true);
    EXPECT_EQ(result.response->extensions->appid_extension, true);
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
                options->exclude_credentials[0].id, kTestOrigin1));
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
      ASSERT_GT(cred.id.size(), config.max_credential_id_length);
    }

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
  }
}

TEST_F(AuthenticatorImplTest, TestGetAssertionTimeout) {
  // The VirtualFidoAuthenticator simulates a tap immediately after it gets the
  // request. Replace by the real discovery that will wait until timeout.
  ReplaceDiscoveryFactory(std::make_unique<device::FidoDiscoveryFactory>());

  NavigateAndCommit(GURL(kTestOrigin1));
  base::HistogramTester histogram_tester;
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();

  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
  histogram_tester.ExpectUniqueSample("WebAuthentication.GetAssertion.Result",
                                      CredentialRequestResult::kTimeout, 1);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kUiTimeout,
                               AuthenticationRequestMode::kModalWebAuthn);
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
    credential.credential_type = device::CredentialType::kPublicKey;
    credential.id.resize(size);
    credential.transports.emplace(
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice);

    const bool should_be_valid = size < 256;
    if (should_be_valid) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          credential.id, kTestRelyingPartyId));
    }

    options->allow_credentials.emplace_back(credential);

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              should_be_valid ? AuthenticatorStatus::SUCCESS
                              : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, NoSilentAuthenticationForCable) {
  // https://crbug.com/954355
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool is_cable_device : {false, true}) {
    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.reject_silent_authentication_requests = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = GetTestCredentials(/*num_credentials=*/2);

    if (is_cable_device) {
      virtual_device_factory_->SetTransport(
          device::FidoTransportProtocol::kHybrid);
      for (auto& cred : options->allow_credentials) {
        cred.transports.clear();
        cred.transports.emplace(device::FidoTransportProtocol::kHybrid);
      }
    }

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id, kTestRelyingPartyId));

    // If a caBLE device is not simulated then silent requests should be used.
    // The virtual device will return an error because
    // |reject_silent_authentication_requests| is true and then it'll
    // immediately resolve the touch request.
    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              is_cable_device ? AuthenticatorStatus::SUCCESS
                              : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, GuessAtTransportsForCable) {
  // Even without any reported transports, if the transaction was done over
  // hybrid, we should guess at the transports and report them.

  NavigateAndCommit(GURL(kTestOrigin1));

  ResetVirtualDevice();
  device::VirtualCtap2Device::Config config;
  config.include_transports_in_attestation_certificate = false;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->SetTransport(device::FidoTransportProtocol::kHybrid);

  const auto result = AuthenticatorMakeCredential();
  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_THAT(
      result.response->transports,
      testing::UnorderedElementsAre(device::FidoTransportProtocol::kHybrid,
                                    device::FidoTransportProtocol::kInternal));
}

TEST_F(AuthenticatorImplTest, TestGetAssertionU2fDeviceBackwardsCompatibility) {
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  // Inject credential ID to the virtual device so that successful sign in is
  // possible.
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, kTestRelyingPartyId));

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
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kRkNotSupported,
                               AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorImplTest, MakeCredentialAlreadyRegistered) {
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  // Exclude the one already registered credential.
  options->exclude_credentials = GetTestCredentials();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->exclude_credentials[0].id, kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::CREDENTIAL_EXCLUDED);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kCredentialExcluded,
                                 AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorImplTest, MakeCredentialPendingRequest) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // Make first request.
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  TestMakeCredentialFuture future;
  authenticator->MakeCredential(std::move(options), future.GetCallback());

  // Make second request.
  // TODO(crbug.com/41355992): Rework to ensure there are potential race
  // conditions once we have VirtualAuthenticatorEnvironment.
  PublicKeyCredentialCreationOptionsPtr options2 =
      GetTestPublicKeyCredentialCreationOptions();
  TestMakeCredentialFuture future2;
  authenticator->MakeCredential(std::move(options2), future2.GetCallback());
  EXPECT_TRUE(future2.Wait());

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, std::get<0>(future2.Get()));

  EXPECT_TRUE(future.Wait());
}

TEST_F(AuthenticatorImplTest, GetAssertionPendingRequest) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // Make first request.
  GetCredentialOptionsPtr options = GetTestGetCredentialOptions();
  TestGetCredentialFuture future;
  authenticator->GetCredential(std::move(options), future.GetCallback());

  // Make second request.
  // TODO(crbug.com/41355992): Rework to ensure there are potential race
  // conditions once we have VirtualAuthenticatorEnvironment.
  GetCredentialOptionsPtr options2 = GetTestGetCredentialOptions();
  TestGetCredentialFuture future2;
  authenticator->GetCredential(std::move(options2), future2.GetCallback());
  EXPECT_TRUE(future2.Wait());

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST,
            future2.Get()->get_get_assertion_response()->status);

  EXPECT_TRUE(future.Wait());
}

TEST_F(AuthenticatorImplTest, ReportPendingRequest) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // Make first request.
  GetCredentialOptionsPtr options = GetTestGetCredentialOptions();
  TestGetCredentialFuture future;
  authenticator->GetCredential(std::move(options), future.GetCallback());

  // Make second request.
  PublicKeyCredentialReportOptionsPtr options2 =
      GetTestPublicKeyCredentialReportOptions();
  TestReportFuture future2;
  authenticator->Report(std::move(options2), future2.GetCallback());
  EXPECT_TRUE(future2.Wait());

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, std::get<0>(future2.Get()));

  EXPECT_TRUE(future.Wait());
}

TEST_F(AuthenticatorImplTest, NavigationDuringOperation) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  base::RunLoop run_loop;
  authenticator.set_disconnect_handler(run_loop.QuitClosure());

  // Make first request.
  GetCredentialOptionsPtr options = GetTestGetCredentialOptions();
  TestGetCredentialFuture future;
  authenticator->GetCredential(std::move(options), future.GetCallback());

  // Simulate a navigation while waiting for the user to press the token.
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
    VerifyGetAssertionOutcomeUkm(0,
                                 GetAssertionOutcome::kCredentialNotRecognized,
                                 AuthenticationRequestMode::kModalWebAuthn);
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
      options->allow_credentials[0].id, kTestRelyingPartyId));

  NavigateAndCommit(GURL(kTestOrigin1));

  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

#if BUILDFLAG(IS_WIN)
TEST_F(AuthenticatorImplTest, Win_IsUVPAA) {
  virtual_device_factory_->set_discover_win_webauthn_api_authenticator(true);
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  for (const bool enable_win_webauthn_api : {false, true}) {
    SCOPED_TRACE(enable_win_webauthn_api ? "enable_win_webauthn_api"
                                         : "!enable_win_webauthn_api");
    for (const bool is_uvpaa : {false, true}) {
      SCOPED_TRACE(is_uvpaa ? "is_uvpaa" : "!is_uvpaa");
      for (bool is_off_the_record : {true, false}) {
        SCOPED_TRACE(is_off_the_record ? "off the record" : "on the record");
        static_cast<TestBrowserContext*>(GetBrowserContext())
            ->set_is_off_the_record(is_off_the_record);
        fake_win_webauthn_api_.set_available(enable_win_webauthn_api);
        fake_win_webauthn_api_.set_is_uvpaa(is_uvpaa);
        EXPECT_EQ(AuthenticatorIsUvpaa(), enable_win_webauthn_api && is_uvpaa);
      }
    }
  }
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(AuthenticatorImplTest, IsUVPAA) {
  NavigateAndCommit(GURL(kTestOrigin1));
  EXPECT_FALSE(AuthenticatorIsUvpaa());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

enum class EnterprisePolicy {
  LISTED,
  NOT_LISTED,
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

const char* AttestationConveyancePreferenceToString(
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
  }
}

const char* AttestationConveyancePreferenceToString(
    device::AttestationConveyancePreference v) {
  switch (v) {
    case device::AttestationConveyancePreference::kNone:
      return "none";
    case device::AttestationConveyancePreference::kIndirect:
      return "indirect";
    case device::AttestationConveyancePreference::kDirect:
      return "direct";
    case device::AttestationConveyancePreference::
        kEnterpriseIfRPListedOnAuthenticator:
      return "enterprise(ep=1)";
    case device::AttestationConveyancePreference::kEnterpriseApprovedByBrowser:
      return "enterprise(ep=2)";
  }
}

// A test class that installs and removes an
// |TestAuthenticatorContentBrowserClient| automatically and can run tests
// against simulated attestation results.
class AuthenticatorContentBrowserClientTest : public AuthenticatorImplTest {
 public:
  AuthenticatorContentBrowserClientTest() = default;

  AuthenticatorContentBrowserClientTest(
      const AuthenticatorContentBrowserClientTest&) = delete;
  AuthenticatorContentBrowserClientTest& operator=(
      const AuthenticatorContentBrowserClientTest&) = delete;

  struct TestCase {
    AttestationConveyancePreference attestation_requested;
    EnterprisePolicy enterprise_policy;
    AuthenticatorStatus expected_status;
    AttestationType expected_attestation;
    std::string_view expected_certificate_substring;
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
      SCOPED_TRACE(test.enterprise_policy == EnterprisePolicy::LISTED
                       ? "individual attestation"
                       : "no individual attestation");
      SCOPED_TRACE(
          AttestationConveyancePreferenceToString(test.attestation_requested));
      SCOPED_TRACE(i);

      test_client_.GetTestWebAuthenticationDelegate()
          ->permit_individual_attestation =
          test.enterprise_policy == EnterprisePolicy::LISTED;

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->relying_party.id = "example.com";
      options->timeout = base::Seconds(1);
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

      std::optional<Value> attestation_value =
          Reader::Read(result.response->attestation_object);
      ASSERT_TRUE(attestation_value);
      ASSERT_TRUE(attestation_value->is_map());
      const auto& attestation = attestation_value->GetMap();

      switch (test.expected_attestation) {
        case AttestationType::ANY:
          ASSERT_TRUE(test.expected_certificate_substring.empty());
          break;

        case AttestationType::NONE:
          ASSERT_TRUE(test.expected_certificate_substring.empty());
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "none");
          EXPECT_TRUE(auth_data.attested_data()->IsAaguidZero());
          break;

        case AttestationType::NONE_WITH_NONZERO_AAGUID:
          ASSERT_TRUE(test.expected_certificate_substring.empty());
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "none");
          EXPECT_FALSE(auth_data.attested_data()->IsAaguidZero());
          break;

        case AttestationType::U2F:
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "fido-u2f");
          if (!test.expected_certificate_substring.empty()) {
            ExpectCertificateContainingSubstring(
                attestation, test.expected_certificate_substring);
          }
          break;

        case AttestationType::PACKED:
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "packed");
          if (!test.expected_certificate_substring.empty()) {
            ExpectCertificateContainingSubstring(
                attestation, test.expected_certificate_substring);
          }
          break;

        case AttestationType::SELF: {
          ASSERT_TRUE(test.expected_certificate_substring.empty());
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
          ASSERT_TRUE(test.expected_certificate_substring.empty());
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
      std::string_view substring) {
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
    std::string_view cert = x5c[0].GetBytestringAsString();
    EXPECT_TRUE(cert.find(substring) != cert.npos);
  }

  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

TEST_F(AuthenticatorContentBrowserClientTest, MakeCredentialActorIsActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(device::kWebAuthnActorCheck, true);

  NavigateAndCommit(GURL(kTestOrigin1));
  test_client_.should_disallow_credential_request = true;
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kBlockedByEmbedder,
                                 AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorContentBrowserClientTest, GetCredentialActorIsActive) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(device::kWebAuthnActorCheck, true);

  test_client_.should_disallow_credential_request = true;
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kBlockedByEmbedder,
                               AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorContentBrowserClientTest, MakeCredentialTLSError) {
  NavigateAndCommit(GURL(kTestOrigin1));
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::CERTIFICATE_ERROR);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kOtherFailure,
                                 AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorContentBrowserClientTest, GetAssertionTLSError) {
  NavigateAndCommit(GURL(kTestOrigin1));
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::CERTIFICATE_ERROR);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kOtherFailure,
                               AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       MakeCredentialSkipTLSCheckWithVirtualEnvironment) {
  NavigateAndCommit(GURL(kTestOrigin1));
  content::AuthenticatorEnvironment::GetInstance()
      ->EnableVirtualAuthenticatorFor(
          static_cast<content::RenderFrameHostImpl*>(main_rfh())
              ->frame_tree_node(),
          /*enable_ui=*/false);
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       GetAssertionSkipTLSCheckWithVirtualEnvironment) {
  NavigateAndCommit(GURL(kTestOrigin1));
  content::AuthenticatorEnvironment::GetInstance()
      ->EnableVirtualAuthenticatorFor(
          static_cast<content::RenderFrameHostImpl*>(main_rfh())
              ->frame_tree_node(),
          /*enable_ui=*/false);
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, kTestRelyingPartyId));
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorContentBrowserClientTest, ReportTLSError) {
  NavigateAndCommit(GURL(kTestOrigin1));
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialReportOptionsPtr options =
      GetTestPublicKeyCredentialReportOptions();
  EXPECT_EQ(AuthenticatorReport(std::move(options)),
            AuthenticatorStatus::CERTIFICATE_ERROR);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       ReportSkipTLSCheckWithVirtualEnvironment) {
  NavigateAndCommit(GURL(kTestOrigin1));
  content::AuthenticatorEnvironment::GetInstance()
      ->EnableVirtualAuthenticatorFor(
          static_cast<content::RenderFrameHostImpl*>(main_rfh())
              ->frame_tree_node(),
          /*enable_ui=*/false);
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialReportOptionsPtr options =
      GetTestPublicKeyCredentialReportOptions();
  EXPECT_EQ(AuthenticatorReport(std::move(options)),
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorContentBrowserClientTest, TestGetAssertionCancel) {
  NavigateAndCommit(GURL(kTestOrigin1));
  test_client_.simulate_user_cancelled_ = true;
  base::HistogramTester histogram_tester;

  EXPECT_EQ(AuthenticatorGetAssertion().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  histogram_tester.ExpectUniqueSample("WebAuthentication.GetAssertion.Result",
                                      CredentialRequestResult::kUserCancelled,
                                      1);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kUserCancellation,
                               AuthenticationRequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorContentBrowserClientTest, TestMakeCredentialCancel) {
  NavigateAndCommit(GURL(kTestOrigin1));
  test_client_.simulate_user_cancelled_ = true;
  base::HistogramTester histogram_tester;

  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  histogram_tester.ExpectUniqueSample("WebAuthentication.MakeCredential.Result",
                                      CredentialRequestResult::kUserCancelled,
                                      1);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kUserCancellation,
                                 AuthenticationRequestMode::kModalWebAuthn);
}

// Tests that the enclave authenticator should only be discovered for make
// credential requests it can fulfill.
TEST_F(AuthenticatorContentBrowserClientTest,
       DiscoverEnclaveAuthenticatorMakeCredential) {
  NavigateAndCommit(GURL(kTestOrigin1));
  struct TestCase {
    bool available;
    device::AuthenticatorAttachment attachment;
    bool expected_discovered;
  } kTestCases[] = {
      {false, device::AuthenticatorAttachment::kAny, false},
      {true, device::AuthenticatorAttachment::kCrossPlatform, false},
      {true, device::AuthenticatorAttachment::kAny, true},
      {true, device::AuthenticatorAttachment::kPlatform, true},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "available=" << test_case.available);
    SCOPED_TRACE(testing::Message()
                 << "attachment=" << static_cast<int>(test_case.attachment));
    test_client_.GetTestWebAuthenticationDelegate()
        ->browser_provided_passkeys_available = test_case.available;
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->authenticator_selection->authenticator_attachment =
        test_case.attachment;
    AuthenticatorMakeCredential(std::move(options));
    ASSERT_TRUE(
        test_client_.enclave_authenticator_should_be_discovered_.has_value());
    EXPECT_EQ(*test_client_.enclave_authenticator_should_be_discovered_,
              test_case.expected_discovered);
  }
}

// Tests that the enclave authenticator should only be discovered for get
// assertion requests it can fulfill.
TEST_F(AuthenticatorContentBrowserClientTest,
       DiscoverEnclaveAuthenticatorGetAssertion) {
  NavigateAndCommit(GURL(kTestOrigin1));
  device::PublicKeyCredentialDescriptor internal_cred(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 1),
      {device::FidoTransportProtocol::kInternal});
  device::PublicKeyCredentialDescriptor sk_cred(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 2),
      {device::FidoTransportProtocol::kUsbHumanInterfaceDevice});
  device::PublicKeyCredentialDescriptor unknown_cred(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 3), {});
  struct TestCase {
    bool available;
    std::vector<device::PublicKeyCredentialDescriptor> creds;
    bool expected_discovered;
  } kTestCases[] = {
      {false, {internal_cred}, false},
      {true, {sk_cred}, false},
      {true, {internal_cred}, true},
      {true, {unknown_cred}, true},
  };
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << "available=" << test_case.available);
    testing::Message creds_trace;
    creds_trace << "creds=[";
    if (test_case.creds.empty()) {
      creds_trace << "empty]";
    } else {
      creds_trace << test_case.creds.at(0).id.at(0) << "]";
    }
    SCOPED_TRACE(creds_trace);
    test_client_.GetTestWebAuthenticationDelegate()
        ->browser_provided_passkeys_available = test_case.available;
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = std::move(test_case.creds);
    AuthenticatorGetAssertion(std::move(options));
    ASSERT_TRUE(
        test_client_.enclave_authenticator_should_be_discovered_.has_value());
    EXPECT_EQ(*test_client_.enclave_authenticator_should_be_discovered_,
              test_case.expected_discovered);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, TransportsFromAllowList) {
  NavigateAndCommit(GURL(kTestOrigin1));
  InjectVirtualAuthenticatorForAllTransports();
  device::PublicKeyCredentialDescriptor internal_cred(
      device::CredentialType::kPublicKey, {1, 2, 3, 4},
      {device::FidoTransportProtocol::kInternal});
  device::PublicKeyCredentialDescriptor sk_cred(
      device::CredentialType::kPublicKey, {1, 2, 3, 4},
      {device::FidoTransportProtocol::kUsbHumanInterfaceDevice});
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials = {std::move(internal_cred), std::move(sk_cred)};
  AuthenticatorGetAssertion(std::move(options));
  EXPECT_THAT(test_client_.discovered_transports_,
              testing::UnorderedElementsAre(
                  device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
                  device::FidoTransportProtocol::kInternal));
}

TEST_F(AuthenticatorContentBrowserClientTest,
       AllTransportsAllowedIfHasAllowedCredentialWithEmptyTransportsList) {
  NavigateAndCommit(GURL(kTestOrigin1));
  InjectVirtualAuthenticatorForAllTransports();
  device::PublicKeyCredentialDescriptor cred(device::CredentialType::kPublicKey,
                                             {1, 2, 3, 4}, {});
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials = {std::move(cred)};
  AuthenticatorGetAssertion(std::move(options));
  EXPECT_THAT(test_client_.discovered_transports_,
              testing::UnorderedElementsAre(
                  device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
                  device::FidoTransportProtocol::kNearFieldCommunication,
                  device::FidoTransportProtocol::kBluetoothLowEnergy,
                  device::FidoTransportProtocol::kHybrid,
                  device::FidoTransportProtocol::kInternal,
                  device::FidoTransportProtocol::kSmartCard));
}

TEST_F(AuthenticatorContentBrowserClientTest,
       AllTransportsAllowedIfAllowCredentialsListIsEmpty) {
  test_client_.web_authentication_delegate.supports_resident_keys = true;
  NavigateAndCommit(GURL(kTestOrigin1));
  InjectVirtualAuthenticatorForAllTransports();
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials.clear();
  AuthenticatorGetAssertion(std::move(options));
  EXPECT_THAT(test_client_.discovered_transports_,
              testing::UnorderedElementsAre(
                  device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
                  device::FidoTransportProtocol::kNearFieldCommunication,
                  device::FidoTransportProtocol::kBluetoothLowEnergy,
                  device::FidoTransportProtocol::kHybrid,
                  device::FidoTransportProtocol::kInternal,
                  device::FidoTransportProtocol::kSmartCard));
}

// Test that credentials can be created and used from an extension origin when
// permitted by the delegate.
TEST_F(AuthenticatorContentBrowserClientTest, ChromeExtensions) {
  constexpr char kExtensionId[] = "abcdefg";
  static const std::string kExtensionOrigin =
      std::string(kExtensionScheme) + "://" + kExtensionId;

  NavigateAndCommit(GURL(kExtensionOrigin + "/test.html"));

  for (bool permit_webauthn_in_extensions : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "permit=" << permit_webauthn_in_extensions);
    test_client_.GetTestWebAuthenticationDelegate()->permit_extensions =
        permit_webauthn_in_extensions;

    std::vector<uint8_t> credential_id;
    {
      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->relying_party.id = kExtensionId;

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));
      if (permit_webauthn_in_extensions) {
        EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
        credential_id = result.response->info->raw_id;
      } else {
        EXPECT_EQ(result.status, AuthenticatorStatus::INVALID_PROTOCOL);
      }
    }

    {
      PublicKeyCredentialRequestOptionsPtr options =
          GetTestPublicKeyCredentialRequestOptions();
      options->relying_party_id = kExtensionId;
      options->allow_credentials[0] = device::PublicKeyCredentialDescriptor(
          device::CredentialType::kPublicKey, std::move(credential_id));

      EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
                permit_webauthn_in_extensions
                    ? AuthenticatorStatus::SUCCESS
                    : AuthenticatorStatus::INVALID_PROTOCOL);
    }
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, ChromeExtensionBadRpIds) {
  // Permit WebAuthn in extensions.
  static const std::string kExtensionOrigin =
      base::StrCat({kExtensionScheme, "://abcdefg"});
  test_client_.GetTestWebAuthenticationDelegate()->permit_extensions = true;

  // Extensions are not permitted to assert RP IDs different from their
  // extension ID.
  for (auto* rp_id : {"", "xyz", "localhost", "xyz.com",
                      "chrome-extension://abcdefg", "https://abcdefg"}) {
    NavigateAndCommit(GURL(kExtensionOrigin + "/test.html"));
    {
      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->relying_party.id = rp_id;

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));
      EXPECT_EQ(result.status, AuthenticatorStatus::INVALID_PROTOCOL);
    }

    {
      PublicKeyCredentialRequestOptionsPtr options =
          GetTestPublicKeyCredentialRequestOptions();
      options->relying_party_id = rp_id;
      GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
      EXPECT_EQ(result.status, AuthenticatorStatus::INVALID_PROTOCOL);
    }
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, AttestationBehaviour) {
  const char kStandardCommonName[] = "U2F Attestation";
  const char kIndividualCommonName[] = "Individual Cert";

  const std::vector<TestCase> kTests = {
      {
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::NOT_LISTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::LISTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          EnterprisePolicy::NOT_LISTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::INDIRECT,
          EnterprisePolicy::LISTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::NOT_LISTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::LISTED,
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
            AuthenticatorStatus::SUCCESS,
            AttestationType::PACKED,
            kIndividualCommonName,
        },
        {
            AttestationConveyancePreference::ENTERPRISE,
            EnterprisePolicy::NOT_LISTED,
            AuthenticatorStatus::SUCCESS,
            AttestationType::PACKED,
            kStandardCommonName,
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
            AuthenticatorStatus::SUCCESS,
            AttestationType::PACKED,
            kIndividualCommonName,
        },
        {
            AttestationConveyancePreference::ENTERPRISE,
            EnterprisePolicy::LISTED,
            AuthenticatorStatus::SUCCESS,
            AttestationType::PACKED,
            kIndividualCommonName,
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
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::LISTED,
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
  test_client_.GetTestWebAuthenticationDelegate()->is_uvpaa_override = true;
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
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE_WITH_NONZERO_AAGUID,
          "",
      },
      {
          // Attestation is always returned if requested because it is privacy
          // preserving. The AttestationConsent value is irrelevant.
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
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
          AuthenticatorStatus::SUCCESS,
          AttestationType::SELF,
          "",
      },
      {
          // And if direct attestation was requested.
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
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
    const char* filter_json;
    AttestationConveyancePreference attestation;
    EnterprisePolicy enterprise_policy;
    AttestationType result;
  } kTests[] = {
      // Empty or nonsense filter doesn't block anything.
      {
          "",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::U2F,
      },
      {
          R"({"filters": []})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::U2F,
      },
      // Direct listing of domain blocks...
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "example.com",
            "action": "no-attestation"
          }]})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::NONE,
      },
      // ... unless attestation is permitted by policy.
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "example.com",
            "action": "no-attestation"
          }]})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AttestationType::U2F,
      },
      // The whole domain can be blocked. (Note, blocking a domain would
      // normally want to list both the base domain and a pattern for
      // subdomains because the below also matches fooexample.com.)
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "*example.com",
            "action": "no-attestation"
          }]})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::NONE,
      },
      // Policy again overrides
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "*example.com",
            "action": "no-attestation"
          }]})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AttestationType::U2F,
      },
      // An explicit wildcard will match everything, be careful. (Omitting
      // both RP ID and device is a parse error, however.)
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "*",
            "action": "no-attestation"
          }]})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::NONE,
      },
  };

  int test_num = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_num++);
    SCOPED_TRACE(test.filter_json);

    device::fido_filter::ScopedFilterForTesting filter(test.filter_json);

    const std::vector<TestCase> kTestCase = {
        {
            test.attestation,
            test.enterprise_policy,
            AuthenticatorStatus::SUCCESS,
            test.result,
            "",
        },
    };

    RunTestCases(kTestCase);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, FilteringMakeCredential) {
  static const struct {
    const char* filter_json;
    bool expect_make_credential_success;
  } kTests[] = {
      {
          R"()",
          true,
      },
      // Block by device.
      {
          R"({"filters": [{
          "operation": "mc",
          "device": "VirtualFidoDevice-*",
          "action": "block",
          }]})",
          false,
      },
      // Shouldn't block when the device is unrelated.
      {
          R"({"filters": [{
          "operation": "mc",
          "device": "OtherDevice-*",
          "action": "block",
          }]})",
          true,
      },
      // Block by RP ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "google.com",
          "action": "block",
          }]})",
          false,
      },
      // Unrelated RP ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "other.com",
          "action": "block",
          }]})",
          true,
      },
      // Block specific user ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "user",
          "id": "0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A",
          "action": "block",
          }]})",
          false,
      },
      // Different user ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "user",
          "id": "FF0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A",
          "action": "block",
          }]})",
          true,
      },
      // Block by user ID length.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "user",
          "id_min_size": 32,
          "id_max_size": 32,
          "action": "block",
          }]})",
          false,
      },
      // Block user IDs that are longer than specified by
      // |GetTestPublicKeyCredentialUserEntity|.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "user",
          "id_min_size": 33,
          "action": "block",
          }]})",
          true,
      },
      // Block excluded credential ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "cred",
          "id": "0000000000000000000000000000000000000000000000000000000000000000",
          "action": "block",
          }]})",
          false,
      },
      // Block different credential ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "cred",
          "id": "FF00000000000000000000000000000000000000000000000000000000000000",
          "action": "block",
          }]})",
          true,
      },
      // Block by excluded credential ID length.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "cred",
          "id_min_size": 32,
          "id_max_size": 32,
          "action": "block",
          }]})",
          false,
      },
      // Block longer credentials IDs than are used.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "cred",
          "id_min_size": 33,
          "action": "block",
          }]})",
          true,
      },
  };

  NavigateAndCommit(GURL(kTestOrigin1));

  int test_num = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_num++);
    SCOPED_TRACE(test.filter_json);
    device::fido_filter::ScopedFilterForTesting filter(test.filter_json);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->exclude_credentials = GetTestCredentials();
    EXPECT_EQ(AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options))
                      .status == AuthenticatorStatus::SUCCESS,
              test.expect_make_credential_success);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, FilteringGetAssertion) {
  static const struct {
    const char* filter_json;
    bool expect_get_assertion_success;
  } kTests[] = {
      {
          R"()",
          true,
      },
      // Block by device.
      {
          R"({"filters": [{
          "operation": "ga",
          "device": "VirtualFidoDevice-*",
          "action": "block",
          }]})",
          false,
      },
      // Shouldn't block when the device is unrelated.
      {
          R"({"filters": [{
          "operation": "ga",
          "device": "OtherDevice-*",
          "action": "block",
          }]})",
          true,
      },
      // Block by RP ID.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "google.com",
          "action": "block",
          }]})",
          false,
      },
      // Unrelated RP ID.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "other.com",
          "action": "block",
          }]})",
          true,
      },
      // Block allowList credential ID.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "*",
          "id_type": "cred",
          "id": "0000000000000000000000000000000000000000000000000000000000000000",
          "action": "block",
          }]})",
          false,
      },
      // Block different credential ID.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "*",
          "id_type": "cred",
          "id": "FF00000000000000000000000000000000000000000000000000000000000000",
          "action": "block",
          }]})",
          true,
      },
      // Block by allowList credential ID length for credentials returned by
      // |GetTestCredentials|.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "*",
          "id_type": "cred",
          "id_min_size": 32,
          "id_max_size": 32,
          "action": "block",
          }]})",
          false,
      },
      // Block longer credentials IDs than are used.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "*",
          "id_type": "cred",
          "id_min_size": 33,
          "action": "block",
          }]})",
          true,
      },
  };

  NavigateAndCommit(GURL(kTestOrigin1));

  int test_num = 0;
  bool credential_added = false;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_num++);
    SCOPED_TRACE(test.filter_json);
    device::fido_filter::ScopedFilterForTesting filter(test.filter_json);

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    if (!credential_added) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          options->allow_credentials[0].id, kTestRelyingPartyId));
      credential_added = true;
    }

    EXPECT_EQ(
        AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status ==
            AuthenticatorStatus::SUCCESS,
        test.expect_get_assertion_success);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, FilteringFailsOpen) {
  // Setting the filter to invalid JSON should not filter anything.
  device::fido_filter::ScopedFilterForTesting filter(
      "nonsense",
      device::fido_filter::ScopedFilterForTesting::PermitInvalidJSON::kYes);

  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = GetTestCredentials();
  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       MakeCredentialRequestStartedCallback) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  TestRequestStartedFuture request_started_future;
  test_client_.action_callbacks_registered_callback =
      request_started_future.GetCallback();
  authenticator->MakeCredential(std::move(options), base::DoNothing());
  EXPECT_TRUE(request_started_future.Wait());
}

TEST_F(AuthenticatorContentBrowserClientTest,
       GetAssertionRequestStartedCallback) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  GetCredentialOptionsPtr options = GetTestGetCredentialOptions();

  TestRequestStartedFuture request_started_future;
  test_client_.action_callbacks_registered_callback =
      request_started_future.GetCallback();
  authenticator->GetCredential(std::move(options), base::DoNothing());
  EXPECT_TRUE(request_started_future.Wait());
}

TEST_F(AuthenticatorContentBrowserClientTest, MakeCredentialStartOver) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  // Make the request fail so that it's started over.
  options->authenticator_selection->user_verification_requirement =
      device::UserVerificationRequirement::kRequired;

  TestRequestStartedFuture request_started_future;
  test_client_.action_callbacks_registered_callback =
      request_started_future.GetCallback();
  TestRequestStartedFuture request_restarted_future;
  test_client_.started_over_callback_ = request_restarted_future.GetCallback();

  authenticator->MakeCredential(std::move(options), base::DoNothing());
  EXPECT_TRUE(request_started_future.Wait());
  EXPECT_TRUE(request_restarted_future.Wait());

  const auto& discoveries_trace = virtual_device_factory_->trace()->discoveries;
  ASSERT_EQ(discoveries_trace.size(), 2u);
  EXPECT_TRUE(discoveries_trace[0].is_stopped);
  EXPECT_TRUE(discoveries_trace[0].is_destroyed);
  EXPECT_FALSE(discoveries_trace[1].is_stopped);
  EXPECT_FALSE(discoveries_trace[1].is_destroyed);
}

TEST_F(AuthenticatorContentBrowserClientTest, GetAssertionStartOver) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  GetCredentialOptionsPtr options = GetTestGetCredentialOptions();

  TestRequestStartedFuture request_started_future;
  test_client_.action_callbacks_registered_callback =
      request_started_future.GetCallback();
  TestRequestStartedFuture request_restarted_future;
  test_client_.started_over_callback_ = request_restarted_future.GetCallback();

  authenticator->GetCredential(std::move(options), base::DoNothing());
  EXPECT_TRUE(request_started_future.Wait());
  EXPECT_TRUE(request_restarted_future.Wait());

  const auto& discoveries_trace = virtual_device_factory_->trace()->discoveries;
  ASSERT_EQ(discoveries_trace.size(), 2u);
  EXPECT_TRUE(discoveries_trace[0].is_stopped);
  EXPECT_TRUE(discoveries_trace[0].is_destroyed);
  EXPECT_FALSE(discoveries_trace[1].is_stopped);
  EXPECT_FALSE(discoveries_trace[1].is_destroyed);
}

TEST_F(AuthenticatorContentBrowserClientTest, Unfocused) {
  // When the |ContentBrowserClient| considers the tab to be unfocused,
  // registration requests should fail with a |NOT_FOCUSED| error, but getting
  // assertions should still work.
  test_client_.GetTestWebAuthenticationDelegate()->is_focused = false;

  NavigateAndCommit(GURL(kTestOrigin1));

  {
    TestRequestStartedFuture request_started_future;
    test_client_.action_callbacks_registered_callback =
        request_started_future.GetCallback();

    EXPECT_EQ(
        AuthenticatorMakeCredential(GetTestPublicKeyCredentialCreationOptions())
            .status,
        AuthenticatorStatus::NOT_FOCUSED);
    EXPECT_FALSE(request_started_future.IsReady());
  }

  {
    device::PublicKeyCredentialDescriptor credential;
    credential.credential_type = device::CredentialType::kPublicKey;
    credential.id.resize(16);
    credential.transports = {
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice};

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        credential.id, kTestRelyingPartyId));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials.emplace_back(credential);

    TestRequestStartedFuture request_started_future;
    test_client_.action_callbacks_registered_callback =
        request_started_future.GetCallback();

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
    EXPECT_TRUE(request_started_future.IsReady());
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

  for (const bool is_uvpaa : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "is_uvpaa=" << is_uvpaa);
    test_client_.GetTestWebAuthenticationDelegate()->is_uvpaa_override =
        is_uvpaa;

    EXPECT_EQ(AuthenticatorIsUvpaa(), is_uvpaa);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       GetClientCapabilities_CheckUvpaaPlumbing) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (const bool is_uvpaa : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "is_uvpaa=" << is_uvpaa);
    test_client_.GetTestWebAuthenticationDelegate()->is_uvpaa_override =
        is_uvpaa;

    ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
    ExpectCapability(capabilities,
                     client_capabilities::kUserVerifyingPlatformAuthenticator,
                     is_uvpaa);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       GetClientCapabilities_CheckPPAAPlumbing) {
  NavigateAndCommit(GURL(kTestOrigin1));

  // Verify: PPAA == `is_uvpaa` || HybridTransport (false).
  for (const bool is_uvpaa : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "is_uvpaa=" << is_uvpaa);
    test_client_.GetTestWebAuthenticationDelegate()->is_uvpaa_override =
        is_uvpaa;
    // Simulate `hybrid_transport = false`.
    EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(false));

    ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
    ExpectCapability(capabilities,
                     client_capabilities::kPasskeyPlatformAuthenticator,
                     is_uvpaa);
  }

  // Verify: PPAA == isUVPAA (false) || `hybrid_transport`.
  for (const bool hybrid_transport : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "hybrid_transport=" << hybrid_transport);
    // Simulate `isUVPAA = false`.
    test_client_.GetTestWebAuthenticationDelegate()->is_uvpaa_override = false;

    EXPECT_CALL(*mock_adapter_, IsPresent())
        .WillOnce(::testing::Return(hybrid_transport));

    ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
    ExpectCapability(capabilities,
                     client_capabilities::kPasskeyPlatformAuthenticator,
                     hybrid_transport);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       GetClientCapabilities_ConditionalGet_ReturnsFalse) {
  NavigateAndCommit(GURL(kTestOrigin1));
  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
  ExpectCapability(capabilities, client_capabilities::kConditionalGet, true);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       GPMPasskeys_IsConditionalMediationAvailable) {
  NavigateAndCommit(GURL(kTestOrigin1));
  ASSERT_TRUE(AuthenticatorIsConditionalMediationAvailable());
}

class AuthenticatorImplRemoteDesktopTestBase
    : public AuthenticatorContentBrowserClientTest {
 public:
  enum class RemoteDesktopExtension {
    kRemoteDesktopClientOverride,
    kRemoteClientDataJSON,
  };

 protected:
  static constexpr char kOtherRdpOrigin[] = "https://myrdp.test";
  static constexpr char kExampleOrigin[] = "https://example.test";
  static constexpr char kExampleRpId[] = "example.test";
  static constexpr char kExampleAppid[] = "https://example.test/appid.json";
  static constexpr char kOtherRpId[] = "other.test";
  static constexpr char kOtherAppid[] = "https://other.test/appid.json";

  void SetUp() override {
    AuthenticatorContentBrowserClientTest::SetUp();
    // Simulate enterprise-policy authorization for `kCorpCrdOrigin`.
    test_client_.GetTestWebAuthenticationDelegate()
        ->remote_desktop_client_override_origin =
        url::Origin::Create(GURL(kCorpCrdOrigin));
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kWebAuthRemoteDesktopSupport);
  }

  static std::string MakeClientDataJSON(const std::string& origin) {
    return R"({"type":"webauthn.create","challenge":"dGVzdA","origin":")" +
           origin + R"(","crossOrigin":false})";
  }

  static std::string MakeGetClientDataJSON(const std::string& origin) {
    return R"({"type":"webauthn.get","challenge":"dGVzdA","origin":")" +
           origin + R"(","crossOrigin":false})";
  }

  static void SetRemoteDesktopExtension(
      blink::mojom::PublicKeyCredentialCreationOptions& options,
      const std::string& origin,
      RemoteDesktopExtension extension) {
    if (extension == RemoteDesktopExtension::kRemoteClientDataJSON) {
      options.remote_client_data_json = MakeClientDataJSON(origin);
      return;
    }
    options.remote_desktop_client_override = RemoteDesktopClientOverride::New(
        url::Origin::Create(GURL(origin)), true);
  }

  static void SetRemoteDesktopExtension(
      blink::mojom::PublicKeyCredentialRequestOptions& options,
      const std::string& origin,
      RemoteDesktopExtension extension) {
    if (extension == RemoteDesktopExtension::kRemoteClientDataJSON) {
      options.extensions->remote_client_data_json =
          MakeGetClientDataJSON(origin);
      return;
    }
    options.extensions->remote_desktop_client_override =
        RemoteDesktopClientOverride::New(url::Origin::Create(GURL(origin)),
                                         true);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

// Tests both remote-desktop extension encodings.
class AuthenticatorImplRemoteDesktopExtensionTest
    : public AuthenticatorImplRemoteDesktopTestBase,
      public testing::WithParamInterface<
          AuthenticatorImplRemoteDesktopTestBase::RemoteDesktopExtension> {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        device::kWebAuthnRemoteClientDataJson,
        GetParam() == RemoteDesktopExtension::kRemoteClientDataJSON);
    AuthenticatorImplRemoteDesktopTestBase::SetUp();
  }
};

TEST_P(AuthenticatorImplRemoteDesktopExtensionTest, MakeCredential) {
  const struct TestCase {
    std::string local_origin;
    std::string remote_origin;
    std::string rp_id;
    bool success;
  } test_cases[] = {
      {kCorpCrdOrigin, kExampleOrigin, kExampleRpId, true},
      {kOtherRdpOrigin, kExampleOrigin, kExampleRpId, false},
      {kOtherRdpOrigin, kExampleOrigin, kOtherRpId, false},
      {kExampleOrigin, kExampleOrigin, kExampleRpId, false},
  };

  for (const auto& test : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "local=" << test.local_origin
                 << " remote=" << test.remote_origin << " rp=" << test.rp_id);
    NavigateAndCommit(GURL(test.local_origin));

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test.rp_id;
    SetRemoteDesktopExtension(*options, test.remote_origin, GetParam());

    auto result = AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status,
              test.success ? AuthenticatorStatus::SUCCESS
                           : AuthenticatorStatus::
                                 REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED);
    // remoteClientDataJSON must reach the authenticator unchanged.
    if (test.success && result.response &&
        GetParam() == RemoteDesktopExtension::kRemoteClientDataJSON) {
      EXPECT_EQ(base::as_string_view(result.response->info->client_data_json),
                MakeClientDataJSON(test.remote_origin));
    }
  }
}

TEST_P(AuthenticatorImplRemoteDesktopExtensionTest, GetAssertion) {
  const struct TestCase {
    std::string local_origin;
    std::string remote_origin;
    std::string rp_id;
    bool success;
  } test_cases[] = {
      {kCorpCrdOrigin, kExampleOrigin, kExampleRpId, true},
      {kOtherRdpOrigin, kExampleOrigin, kExampleRpId, false},
      {kOtherRdpOrigin, kExampleOrigin, kOtherRpId, false},
      {kExampleOrigin, kExampleOrigin, kExampleRpId, false},
  };

  for (const auto& test : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "local=" << test.local_origin
                 << " remote=" << test.remote_origin << " rp=" << test.rp_id);
    ResetVirtualDevice();
    NavigateAndCommit(GURL(test.local_origin));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test.rp_id;
    SetRemoteDesktopExtension(*options, test.remote_origin, GetParam());

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id, test.rp_id));

    auto result = AuthenticatorGetAssertion(std::move(options));
    EXPECT_EQ(result.status,
              test.success ? AuthenticatorStatus::SUCCESS
                           : AuthenticatorStatus::
                                 REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED);
    // remoteClientDataJSON must reach the authenticator unchanged.
    if (test.success && result.response &&
        GetParam() == RemoteDesktopExtension::kRemoteClientDataJSON) {
      EXPECT_EQ(base::as_string_view(result.response->info->client_data_json),
                MakeGetClientDataJSON(test.remote_origin));
    }
  }
}

TEST_P(AuthenticatorImplRemoteDesktopExtensionTest, MakeCredentialAppid) {
  const struct TestCase {
    std::string local_origin;
    std::string remote_origin;
    std::string rp_id;
    std::string app_id;
    AuthenticatorStatus expected;
  } test_cases[] = {
      {kCorpCrdOrigin, kExampleOrigin, kExampleRpId, kExampleAppid,
       AuthenticatorStatus::SUCCESS},
      {kCorpCrdOrigin, kExampleOrigin, kExampleRpId, kOtherAppid,
       AuthenticatorStatus::INVALID_DOMAIN},
      {kOtherRdpOrigin, kExampleOrigin, kExampleRpId, kExampleAppid,
       AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED},
      {kOtherRdpOrigin, kExampleOrigin, kExampleRpId, kOtherAppid,
       AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED},
      {kExampleOrigin, kExampleOrigin, kExampleRpId, kExampleAppid,
       AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED},
      {kExampleOrigin, kExampleOrigin, kExampleRpId, kOtherAppid,
       AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED},
  };

  for (const auto& test : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "local=" << test.local_origin
                 << " remote=" << test.remote_origin << " rp=" << test.rp_id
                 << " appid=" << test.app_id);
    NavigateAndCommit(GURL(test.local_origin));

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test.rp_id;
    options->appid_exclude = test.app_id;
    SetRemoteDesktopExtension(*options, test.remote_origin, GetParam());

    auto result = AuthenticatorMakeCredential(std::move(options));
    // remoteClientDataJSON delegates AppID validation to the remote client;
    // both extensions still require caller authorization.
    const AuthenticatorStatus expected =
        GetParam() == RemoteDesktopExtension::kRemoteClientDataJSON &&
                test.local_origin == kCorpCrdOrigin
            ? AuthenticatorStatus::SUCCESS
            : test.expected;
    EXPECT_EQ(result.status, expected);
  }
}

TEST_P(AuthenticatorImplRemoteDesktopExtensionTest, GetAssertionAppid) {
  const struct TestCase {
    std::string local_origin;
    std::string remote_origin;
    std::string rp_id;
    std::string app_id;
    AuthenticatorStatus expected;
  } test_cases[] = {
      {kCorpCrdOrigin, kExampleOrigin, kExampleRpId, kExampleAppid,
       AuthenticatorStatus::SUCCESS},
      {kCorpCrdOrigin, kExampleOrigin, kExampleRpId, kOtherAppid,
       AuthenticatorStatus::INVALID_DOMAIN},
      {kOtherRdpOrigin, kExampleOrigin, kExampleRpId, kExampleAppid,
       AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED},
      {kOtherRdpOrigin, kExampleOrigin, kExampleRpId, kOtherAppid,
       AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED},
      {kExampleOrigin, kExampleOrigin, kExampleRpId, kExampleAppid,
       AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED},
      {kExampleOrigin, kExampleOrigin, kExampleRpId, kOtherAppid,
       AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED},
  };

  for (const auto& test : test_cases) {
    SCOPED_TRACE(testing::Message()
                 << "local=" << test.local_origin
                 << " remote=" << test.remote_origin << " rp=" << test.rp_id
                 << " appid=" << test.app_id);
    ResetVirtualDevice();
    NavigateAndCommit(GURL(test.local_origin));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test.rp_id;
    options->extensions->appid = test.app_id;
    SetRemoteDesktopExtension(*options, test.remote_origin, GetParam());

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id, test.rp_id));

    auto result = AuthenticatorGetAssertion(std::move(options));
    // remoteClientDataJSON delegates AppID validation to the remote client;
    // both extensions still require caller authorization.
    const AuthenticatorStatus expected =
        GetParam() == RemoteDesktopExtension::kRemoteClientDataJSON &&
                test.local_origin == kCorpCrdOrigin
            ? AuthenticatorStatus::SUCCESS
            : test.expected;
    EXPECT_EQ(result.status, expected);
  }
}

// A Chrome extension should not be authorized to use the
// remote-desktop request extensions.
TEST_P(AuthenticatorImplRemoteDesktopExtensionTest, ExtensionCallerOrigin) {
  static const std::string kExtensionOrigin =
      base::StrCat({kExtensionScheme, "://abcdefg"});
  test_client_.GetTestWebAuthenticationDelegate()->permit_extensions = true;
  NavigateAndCommit(GURL(kExtensionOrigin + "/test.html"));

  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = kExampleRpId;
    SetRemoteDesktopExtension(*options, kExampleOrigin, GetParam());
    EXPECT_EQ(
        AuthenticatorMakeCredential(std::move(options)).status,
        AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED);
  }

  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = kExampleRpId;
    SetRemoteDesktopExtension(*options, kExampleOrigin, GetParam());
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id, kExtensionOrigin));
    EXPECT_EQ(
        AuthenticatorGetAssertion(std::move(options)).status,
        AuthenticatorStatus::REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED);
  }
}

TEST_P(AuthenticatorImplRemoteDesktopExtensionTest,
       GetAssertionImmediateMediation) {
  NavigateAndCommit(GURL(kCorpCrdOrigin));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials.clear();
  options->relying_party_id = kExampleRpId;
  SetRemoteDesktopExtension(*options, kExampleOrigin, GetParam());

  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  base::test::TestFuture<void> mojo_error_future;
  SetMojoErrorHandler(base::BindLambdaForTesting([&](const std::string& error) {
    EXPECT_EQ(error,
              "Immediate mediation cannot be used with a remote desktop "
              "override request");
    mojo_error_future.SetValue();
  }));

  auto get_credential_options = GetCredentialOptions::New();
  get_credential_options->public_key = std::move(options);
  get_credential_options->mediation = blink::mojom::Mediation::IMMEDIATE;
  authenticator->GetCredential(std::move(get_credential_options),
                               base::DoNothing());
  EXPECT_TRUE(mojo_error_future.Wait());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AuthenticatorImplRemoteDesktopExtensionTest,
    testing::Values(AuthenticatorImplRemoteDesktopTestBase::
                        RemoteDesktopExtension::kRemoteDesktopClientOverride,
                    AuthenticatorImplRemoteDesktopTestBase::
                        RemoteDesktopExtension::kRemoteClientDataJSON));

// Tests behavior specific to the RemoteClientDataJSON extension.
class AuthenticatorImplRemoteClientDataJSONTest
    : public AuthenticatorImplRemoteDesktopTestBase {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        device::kWebAuthnRemoteClientDataJson);
    AuthenticatorImplRemoteDesktopTestBase::SetUp();
  }

  // Verifies that MakeCredential rejects malformed clientDataJSON.
  void ExpectMakeCredentialRejectsBadJson(const std::string& bad_json) {
    NavigateAndCommit(GURL(kCorpCrdOrigin));

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = kExampleRpId;
    options->remote_client_data_json = bad_json;
    auto result = AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status,
              AuthenticatorStatus::REMOTE_CLIENT_DATA_JSON_INVALID);
  }
};

// The renderer gates this field, so receiving it with the browser feature
// disabled is treated as a bad message.
class AuthenticatorImplRemoteClientDataJSONDisabledTest
    : public AuthenticatorImplRemoteDesktopTestBase {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(
        device::kWebAuthnRemoteClientDataJson);
    AuthenticatorImplRemoteDesktopTestBase::SetUp();
  }
};

TEST_F(AuthenticatorImplRemoteClientDataJSONDisabledTest,
       MakeCredentialRejectsExtensionWhenFeatureDisabled) {
  NavigateAndCommit(GURL(kExampleOrigin));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = kExampleRpId;
  options->remote_client_data_json = MakeClientDataJSON(kExampleOrigin);

  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  base::test::TestFuture<void> mojo_error_future;
  SetMojoErrorHandler(base::BindLambdaForTesting([&](const std::string& error) {
    EXPECT_EQ(error, "invalid remoteClientDataJSON request");
    mojo_error_future.SetValue();
  }));

  authenticator->MakeCredential(std::move(options), base::DoNothing());
  EXPECT_TRUE(mojo_error_future.Wait());
}

TEST_F(AuthenticatorImplRemoteClientDataJSONDisabledTest,
       GetAssertionRejectsExtensionWhenFeatureDisabled) {
  NavigateAndCommit(GURL(kExampleOrigin));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->relying_party_id = kExampleRpId;
  options->extensions->remote_client_data_json =
      MakeGetClientDataJSON(kExampleOrigin);

  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  base::test::TestFuture<void> mojo_error_future;
  SetMojoErrorHandler(base::BindLambdaForTesting([&](const std::string& error) {
    EXPECT_EQ(error,
              "remoteClientDataJSON sent without the "
              "WebAuthenticationRemoteClientDataJson feature enabled");
    mojo_error_future.SetValue();
  }));

  auto get_credential_options = GetCredentialOptions::New();
  get_credential_options->public_key = std::move(options);
  authenticator->GetCredential(std::move(get_credential_options),
                               base::DoNothing());
  EXPECT_TRUE(mojo_error_future.Wait());
}

TEST_F(AuthenticatorImplRemoteClientDataJSONTest, PrecedenceOverOldExtension) {
  // When both remoteClientDataJSON and remoteDesktopClientOverride are
  // provided, remoteClientDataJSON takes precedence.
  NavigateAndCommit(GURL(kCorpCrdOrigin));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = kExampleRpId;
  std::string expected_json = MakeClientDataJSON(kExampleOrigin);
  options->remote_client_data_json = expected_json;
  // Also set the old extension with a different origin. remoteClientDataJSON
  // should win.
  options->remote_desktop_client_override = RemoteDesktopClientOverride::New(
      url::Origin::Create(GURL("https://other-ignored.test")), true);

  auto result = AuthenticatorMakeCredential(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);

  // Verify the clientDataJSON matches what we provided, not what
  // remoteDesktopClientOverride would have produced.
  ASSERT_TRUE(result.response);
  EXPECT_EQ(base::as_string_view(result.response->info->client_data_json),
            expected_json);
}

TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       GetAssertionConditionalMediation) {
  // Verify that remoteClientDataJSON rejects conditional mediation.
  NavigateAndCommit(GURL(kCorpCrdOrigin));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->relying_party_id = kExampleRpId;
  options->extensions->remote_client_data_json =
      MakeGetClientDataJSON(kExampleOrigin);

  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  base::test::TestFuture<void> mojo_error_future;
  SetMojoErrorHandler(base::BindLambdaForTesting([&](const std::string& error) {
    EXPECT_EQ(error,
              "Conditional mediation cannot be used with a "
              "remoteClientDataJSON request");
    mojo_error_future.SetValue();
  }));

  auto get_credential_options = GetCredentialOptions::New();
  get_credential_options->public_key = std::move(options);
  get_credential_options->mediation = blink::mojom::Mediation::CONDITIONAL;
  authenticator->GetCredential(std::move(get_credential_options),
                               base::DoNothing());
  EXPECT_TRUE(mojo_error_future.Wait());
}

TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       MakeCredentialConditionalMediation) {
  // Verify that remoteClientDataJSON rejects conditional mediation (the
  // passkey upgrade flow) on create.
  NavigateAndCommit(GURL(kCorpCrdOrigin));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = kExampleRpId;
  options->remote_client_data_json = MakeClientDataJSON(kExampleOrigin);
  options->is_conditional = true;

  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  base::test::TestFuture<void> mojo_error_future;
  SetMojoErrorHandler(base::BindLambdaForTesting([&](const std::string& error) {
    EXPECT_EQ(error, "invalid remoteClientDataJSON request");
    mojo_error_future.SetValue();
  }));

  authenticator->MakeCredential(std::move(options), base::DoNothing());
  EXPECT_TRUE(mojo_error_future.Wait());
}

// Verifies that JSON containing a `// comment` (a
// `JSON_PARSE_CHROMIUM_EXTENSIONS` extension that the browser previously
// accepted) is rejected by the strict `JSON_PARSE_RFC` parser
// (RFC 8259 strict).
// https://w3c.github.io/webauthn/#sctn-remote-client-data-json-extension
TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       MakeCredentialRejectsJsonWithComments) {
  ExpectMakeCredentialRejectsBadJson(
      R"({"type":"webauthn.create","challenge":"dGVzdA",)"
      R"("origin":"https://example.test","crossOrigin":false // comment
        })");
}

// Verifies that a clientDataJSON with the wrong `type` (e.g., `webauthn.get`
// in a create flow) is rejected.
TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       MakeCredentialRejectsWrongType) {
  ExpectMakeCredentialRejectsBadJson(
      R"({"type":"webauthn.get","challenge":"dGVzdA",)"
      R"("origin":"https://example.test","crossOrigin":false})");
}

// Verifies that a clientDataJSON missing the `type` field is rejected.
TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       MakeCredentialRejectsMissingType) {
  ExpectMakeCredentialRejectsBadJson(
      R"({"challenge":"dGVzdA","origin":"https://example.test","crossOrigin":false})");
}

// Verifies that a clientDataJSON missing the `origin` field is rejected.
TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       MakeCredentialRejectsMissingOrigin) {
  ExpectMakeCredentialRejectsBadJson(
      R"({"type":"webauthn.create","challenge":"dGVzdA","crossOrigin":false})");
}

// Verifies that a clientDataJSON with a non-boolean `crossOrigin` is rejected
// (sanity check that the field, if present, has the spec-required type).
TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       MakeCredentialRejectsNonBoolCrossOrigin) {
  ExpectMakeCredentialRejectsBadJson(
      R"({"type":"webauthn.create","challenge":"dGVzdA",)"
      R"("origin":"https://example.test","crossOrigin":"not_a_bool"})");
}

// Verifies that completely malformed JSON (unclosed brace, etc.) is rejected.
TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       MakeCredentialRejectsMalformedJson) {
  ExpectMakeCredentialRejectsBadJson("not valid json {{{ ");
}

// Verifies that an empty `rp.id` is rejected with NotAllowedError when the
// extension is present. The Blink IDL allows `rp.id` to be absent (USVString
// with no presence requirement); the browser explicitly rejects empty values.
TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       MakeCredentialRejectsEmptyRpId) {
  NavigateAndCommit(GURL(kCorpCrdOrigin));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = "";  // Empty.
  options->remote_client_data_json = MakeClientDataJSON(kExampleOrigin);

  auto result = AuthenticatorMakeCredential(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// Verifies the get() flow also rejects an empty `rp.id` with NotAllowedError
// when the extension is present, mirroring the create() guard. The spec
// requires `rp.id` to be present for both registration and authentication.
TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       GetAssertionRejectsEmptyRpId) {
  NavigateAndCommit(GURL(kCorpCrdOrigin));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->relying_party_id = "";  // Empty.
  options->extensions->remote_client_data_json =
      MakeGetClientDataJSON(kExampleOrigin);

  auto result = AuthenticatorGetAssertion(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// Verifies the same parse rejection in the GetAssertion flow. webauthn.create
// in a get flow is rejected with REMOTE_CLIENT_DATA_JSON_INVALID (mapped to
// EncodingError at the renderer boundary).
TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       GetAssertionRejectsWrongType) {
  NavigateAndCommit(GURL(kCorpCrdOrigin));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->relying_party_id = kExampleRpId;
  // webauthn.create instead of webauthn.get for a get flow.
  options->extensions->remote_client_data_json =
      MakeClientDataJSON(kExampleOrigin);

  auto result = AuthenticatorGetAssertion(std::move(options));
  EXPECT_EQ(result.status,
            AuthenticatorStatus::REMOTE_CLIENT_DATA_JSON_INVALID);
}

// Verifies that a clientDataJSON whose `origin` is a non-web format (as a
// native app on another platform might supply) is accepted: origin validation
// is delegated to the remote client, so the browser does not reject opaque or
// otherwise non-URL origins.
TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       MakeCredentialAcceptsNonWebOrigin) {
  NavigateAndCommit(GURL(kCorpCrdOrigin));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = kExampleRpId;
  // A non-web origin format parses to an opaque origin; it must still be
  // accepted because origin validation is delegated to the remote client.
  options->remote_client_data_json =
      MakeClientDataJSON("android:apk-key-hash:abc123");

  auto result = AuthenticatorMakeCredential(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorImplRemoteClientDataJSONTest,
       GetAssertionAcceptsNonWebOrigin) {
  ResetVirtualDevice();
  NavigateAndCommit(GURL(kCorpCrdOrigin));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->relying_party_id = kExampleRpId;
  // A non-web origin format parses to an opaque origin; it must still be
  // accepted because origin validation is delegated to the remote client.
  options->extensions->remote_client_data_json =
      MakeGetClientDataJSON("android:apk-key-hash:abc123");

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, kExampleRpId));

  auto result = AuthenticatorGetAssertion(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorImplRequestDelegateTest,
       TestRequestDelegateObservesFidoRequestHandler) {
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillRepeatedly(::testing::Return(true));

  auto discovery_factory =
      std::make_unique<device::test::FakeFidoDiscoveryFactory>();
  auto* fake_hid_discovery = discovery_factory->ForgeNextHidDiscovery();
  ReplaceDiscoveryFactory(std::move(discovery_factory));

  NavigateAndCommit(GURL(kTestOrigin1));
  GetCredentialOptionsPtr options = GetTestGetCredentialOptions();
  TestGetCredentialFuture future;

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

  authenticator->GetCredential(std::move(options), future.GetCallback());
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
  ReplaceDiscoveryFactory(std::make_unique<device::FidoDiscoveryFactory>());

  NavigateAndCommit(GURL(kTestOrigin1));

  FailureReasonFuture failure_reason_future;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_future.GetCallback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  TestGetCredentialFuture future;
  authenticator->GetCredential(GetTestGetCredentialOptions(),
                               future.GetCallback());

  task_environment()->FastForwardBy(kTestTimeout);

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
            future.Get()->get_get_assertion_response()->status);

  ASSERT_TRUE(failure_reason_future.IsReady());
  EXPECT_EQ(
      AuthenticatorRequestClientDelegate::InterestingFailureReason::kTimeout,
      failure_reason_future.Get());
}

TEST_F(AuthenticatorImplRequestDelegateTest,
       FailureReasonForDuplicateRegistration) {
  NavigateAndCommit(GURL(kTestOrigin1));

  FailureReasonFuture failure_reason_future;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_future.GetCallback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = GetTestCredentials();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->exclude_credentials[0].id, kTestRelyingPartyId));

  TestMakeCredentialFuture future;
  authenticator->MakeCredential(std::move(options), future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_EXCLUDED,
            std::get<0>(future.Get()));

  ASSERT_TRUE(failure_reason_future.IsReady());
  EXPECT_EQ(AuthenticatorRequestClientDelegate::InterestingFailureReason::
                kKeyAlreadyRegistered,
            failure_reason_future.Get());
}

TEST_F(AuthenticatorImplRequestDelegateTest,
       FailureReasonForMissingRegistration) {
  NavigateAndCommit(GURL(kTestOrigin1));

  FailureReasonFuture failure_reason_future;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_future.GetCallback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  TestGetCredentialFuture future;
  authenticator->GetCredential(GetTestGetCredentialOptions(),
                               future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
            future.Get()->get_get_assertion_response()->status);

  ASSERT_TRUE(failure_reason_future.IsReady());
  EXPECT_EQ(AuthenticatorRequestClientDelegate::InterestingFailureReason::
                kKeyNotRegistered,
            failure_reason_future.Get());
}

TEST_F(AuthenticatorImplTest, NoNonAuthoritativeTransports) {
  NavigateAndCommit(GURL(kTestOrigin1));
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  device::VirtualCtap2Device::Config config;
  // If there are no transports in the attestation certificate, and none from
  // getInfo, then none should be reported because there isn't enough
  // information to say.
  config.include_transports_in_attestation_certificate = false;
  virtual_device_factory_->SetCtap2Config(config);

  MakeCredentialResult result = AuthenticatorMakeCredential();
  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);

  EXPECT_TRUE(result.response->transports.empty());
}

TEST_F(AuthenticatorImplTest, TransportsFromGetInfo) {
  NavigateAndCommit(GURL(kTestOrigin1));
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  device::VirtualCtap2Device::Config config;
  config.include_transports_in_attestation_certificate = false;
  config.transports_in_get_info = {
      device::FidoTransportProtocol::kBluetoothLowEnergy};
  virtual_device_factory_->SetCtap2Config(config);

  MakeCredentialResult result = AuthenticatorMakeCredential();
  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);

  base::flat_set<device::FidoTransportProtocol> reported(
      result.response->transports.begin(), result.response->transports.end());
  EXPECT_EQ(reported.size(), 2u);
  // The transports from the getInfo are authoritative and so they should be
  // reported. In addition to 'ble' from getInfo, 'usb' should be included
  // because that's what was used to communicate with the virtual authenticator.
  EXPECT_TRUE(
      reported.contains(device::FidoTransportProtocol::kBluetoothLowEnergy));
  EXPECT_TRUE(reported.contains(
      device::FidoTransportProtocol::kUsbHumanInterfaceDevice));
}

TEST_F(AuthenticatorImplTest, TransportsInAttestationCertificate) {
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
      for (const bool pin_support : {false, true}) {
        SCOPED_TRACE(include_extension);
        SCOPED_TRACE(authenticator_support);
        SCOPED_TRACE(pin_support);

        device::VirtualCtap2Device::Config config;
        config.hmac_secret_support = authenticator_support;
        config.pin_support = pin_support;
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
        // serialised in the CBOR and correctly passed all the way back around
        // to the reply's authenticator data.
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

        EXPECT_EQ(include_extension && authenticator_support && pin_support,
                  has_hmac_secret);
      }
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
          options->exclude_credentials.back().id, kTestRelyingPartyId));
    }

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              has_excluded_credential ? AuthenticatorStatus::CREDENTIAL_EXCLUDED
                                      : AuthenticatorStatus::SUCCESS);
  }
}

TEST_F(AuthenticatorImplTest, GetAssertionResultMetricError) {
  NavigateAndCommit(GURL(kTestOrigin1));

  base::HistogramTester histogram_tester;
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  histogram_tester.ExpectUniqueSample("WebAuthentication.GetAssertion.Result",
                                      CredentialRequestResult::kOtherError, 1);
}

TEST_F(AuthenticatorImplTest, GetAssertionResultMetricSuccess) {
  NavigateAndCommit(GURL(kTestOrigin1));

  base::HistogramTester histogram_tester;
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials.back().id, kTestRelyingPartyId));
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  histogram_tester.ExpectUniqueSample("WebAuthentication.GetAssertion.Result",
                                      CredentialRequestResult::kOtherSuccess,
                                      1);
}

TEST_F(AuthenticatorImplTest, MakeCredentialResultMetricError) {
  NavigateAndCommit(GURL(kTestOrigin1));

  base::HistogramTester histogram_tester;
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = GetTestCredentials();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->exclude_credentials[0].id, kTestRelyingPartyId));
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::CREDENTIAL_EXCLUDED);
  histogram_tester.ExpectUniqueSample("WebAuthentication.MakeCredential.Result",
                                      CredentialRequestResult::kOtherError, 1);
}

TEST_F(AuthenticatorImplTest, MakeCredentialResultMetricSuccess) {
  NavigateAndCommit(GURL(kTestOrigin1));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  histogram_tester.ExpectUniqueSample("WebAuthentication.MakeCredential.Result",
                                      CredentialRequestResult::kOtherSuccess,
                                      1);
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
          options->allow_credentials.back().id, kTestRelyingPartyId));
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
        test_credentials.front().id, kTestRelyingPartyId));

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
        test_credentials.back().id, kTestRelyingPartyId));

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
          cred.id, kTestRelyingPartyId));
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
    virtual_device_factory_->mutable_state()->allow_list_history.clear();

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    if (has_app_id) {
      options->extensions->appid = kTestOrigin1;
    }
    options->allow_credentials = {device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, cred_id)};

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::NOT_ALLOWED_ERROR);
    const auto& allow_list_history =
        virtual_device_factory_->mutable_state()->allow_list_history;
    // No empty allow-list requests should have been made.
    EXPECT_TRUE(std::ranges::none_of(
        allow_list_history,
        [](const std::vector<device::PublicKeyCredentialDescriptor>&
               allow_list) { return allow_list.empty(); }));
  }
}

// Tests that duplicate credential IDs are filtered from an assertion allow_list
// parameter.
TEST_F(AuthenticatorImplTest, AllowListWithDuplicateCredentialIds) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.max_credential_id_length = kTestCredentialIdLength;
  config.max_credential_count_in_list = 10;
  virtual_device_factory_->SetCtap2Config(config);

  device::PublicKeyCredentialDescriptor cred_a(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 1), {});
  device::PublicKeyCredentialDescriptor cred_b(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 2),
      {device::FidoTransportProtocol::kUsbHumanInterfaceDevice});
  // Same ID as `cred_a` and `cred_b` but with different transports. Transport
  // hints from descriptors with equal IDs should be merged.
  device::PublicKeyCredentialDescriptor cred_c(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 1),
      {device::FidoTransportProtocol::kBluetoothLowEnergy});
  device::PublicKeyCredentialDescriptor cred_d(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 2),
      {device::FidoTransportProtocol::kBluetoothLowEnergy});

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      cred_b.id, kTestRelyingPartyId));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials.clear();
  options->allow_credentials.insert(options->allow_credentials.end(), 5,
                                    cred_a);
  options->allow_credentials.push_back(cred_b);
  options->allow_credentials.insert(options->allow_credentials.end(), 3,
                                    cred_c);
  options->allow_credentials.insert(options->allow_credentials.end(), 2,
                                    cred_d);

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(virtual_device_factory_->mutable_state()->allow_list_history.size(),
            1u);
  device::PublicKeyCredentialDescriptor cred_a_and_c(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 1));
  device::PublicKeyCredentialDescriptor cred_b_and_d(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 2));
  EXPECT_THAT(
      virtual_device_factory_->mutable_state()->allow_list_history.at(0),
      testing::UnorderedElementsAre(cred_a_and_c, cred_b_and_d));
}

// Tests that duplicate credential IDs are filtered from a registration
// exclude_list parameter.
TEST_F(AuthenticatorImplTest, ExcludeListWithDuplicateCredentialIds) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.max_credential_id_length = kTestCredentialIdLength;
  config.max_credential_count_in_list = 100;
  virtual_device_factory_->SetCtap2Config(config);

  device::PublicKeyCredentialDescriptor cred_a(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 1), {});
  device::PublicKeyCredentialDescriptor cred_b(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 2),
      {device::FidoTransportProtocol::kUsbHumanInterfaceDevice});
  // Same ID as `cred_a` and `cred_b` but with different transports. Transport
  // hints from descriptors with equal IDs should be merged.
  device::PublicKeyCredentialDescriptor cred_c(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 1),
      {device::FidoTransportProtocol::kBluetoothLowEnergy});
  device::PublicKeyCredentialDescriptor cred_d(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 2),
      {device::FidoTransportProtocol::kBluetoothLowEnergy});

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials.clear();
  options->exclude_credentials.insert(options->exclude_credentials.end(), 5,
                                      cred_a);
  options->exclude_credentials.push_back(cred_b);
  options->exclude_credentials.insert(options->exclude_credentials.end(), 3,
                                      cred_c);
  options->exclude_credentials.insert(options->exclude_credentials.end(), 2,
                                      cred_d);

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(
      virtual_device_factory_->mutable_state()->exclude_list_history.size(),
      1u);
  device::PublicKeyCredentialDescriptor cred_a_and_c(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 1));
  device::PublicKeyCredentialDescriptor cred_b_and_d(
      device::CredentialType::kPublicKey,
      std::vector<uint8_t>(kTestCredentialIdLength, 2));
  EXPECT_THAT(
      virtual_device_factory_->mutable_state()->exclude_list_history.at(0),
      testing::UnorderedElementsAre(cred_a_and_c, cred_b_and_d));
}

// Test that allow lists over 64 entries are verboten.
TEST_F(AuthenticatorImplTest, OversizedAllowList) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.max_credential_id_length = kTestCredentialIdLength;
  config.max_credential_count_in_list = 100;
  virtual_device_factory_->SetCtap2Config(config);

  auto test_credentials = GetTestCredentials(
      /*num_credentials=*/blink::mojom::
          kPublicKeyCredentialDescriptorListMaxSize +
      1);
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      test_credentials.at(0).id, kTestRelyingPartyId));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials = test_credentials;

  bool has_mojo_error = false;
  SetMojoErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& error) { has_mojo_error = true; }));

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_TRUE(has_mojo_error);
}

// Test that exclude lists over 64 entries are verboten.
TEST_F(AuthenticatorImplTest, OversizedExcludeList) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.max_credential_id_length = kTestCredentialIdLength;
  config.max_credential_count_in_list = 100;
  virtual_device_factory_->SetCtap2Config(config);

  auto test_credentials = GetTestCredentials(
      /*num_credentials=*/blink::mojom::
          kPublicKeyCredentialDescriptorListMaxSize +
      1);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = test_credentials;

  bool has_mojo_error = false;
  SetMojoErrorHandler(base::BindLambdaForTesting(
      [&](const std::string& error) { has_mojo_error = true; }));

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_TRUE(has_mojo_error);
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
      assertion_options->allow_credentials.back().id, kTestRelyingPartyId));
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
      assertion_options->allow_credentials.back().id, kTestRelyingPartyId));
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(assertion_options)).status,
            AuthenticatorStatus::SUCCESS);
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
          test_credentials.back().id, kTestRelyingPartyId));
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
  NavigateAndCommit(GURL(kTestOrigin1));

  static constexpr struct {
    device::CoseAlgorithmIdentifier algo;
    std::optional<int> evp_id;
  } kTests[] = {
      {device::CoseAlgorithmIdentifier::kEs256, EVP_PKEY_EC},
      {device::CoseAlgorithmIdentifier::kRs256, EVP_PKEY_RSA},
      {device::CoseAlgorithmIdentifier::kEdDSA, EVP_PKEY_ED25519},
      {device::CoseAlgorithmIdentifier::kMlDsa44, EVP_PKEY_ML_DSA_44},
      {device::CoseAlgorithmIdentifier::kMlDsa65, EVP_PKEY_ML_DSA_65},
      {device::CoseAlgorithmIdentifier::kMlDsa87, EVP_PKEY_ML_DSA_87},
      {device::CoseAlgorithmIdentifier::kInvalidForTesting, std::nullopt},
  };

  std::vector<device::CoseAlgorithmIdentifier> advertised_algorithms;
  for (const auto& test : kTests) {
    advertised_algorithms.push_back(test.algo);
  }

  device::VirtualCtap2Device::Config config;
  config.advertised_algorithms = std::move(advertised_algorithms);
  virtual_device_factory_->SetCtap2Config(config);

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

    // The value of the parsed authenticator data should match what's in
    // the attestation object.
    std::optional<Value> attestation_value =
        Reader::Read(response->attestation_object);
    CHECK(attestation_value);
    const auto& attestation = attestation_value->GetMap();
    const auto auth_data_it = attestation.find(Value(device::kAuthDataKey));
    CHECK(auth_data_it != attestation.end());
    const std::vector<uint8_t>& auth_data =
        auth_data_it->second.GetBytestring();
    EXPECT_EQ(auth_data, response->info->authenticator_data);

    ASSERT_EQ(test.evp_id.has_value(), response->public_key_der.has_value());
    if (!test.evp_id) {
      continue;
    }

    bssl::UniquePtr<EVP_PKEY> pkey =
        crypto::evp::PublicKeyFromBytes(response->public_key_der.value());
    ASSERT_TRUE(pkey.get());

    EXPECT_EQ(test.evp_id.value(), EVP_PKEY_id(pkey.get()));
  }
}

TEST_F(AuthenticatorImplTest, AlgorithmsOmitted) {
  // Some CTAP 2.0 security keys shipped support for algorithms other than
  // ECDSA P-256 but the algorithms field didn't exist then. makeCredential
  // requests should get routed to them anyway.

  device::VirtualCtap2Device::Config config;
  // Remove the algorithms field from the getInfo.
  config.advertised_algorithms.clear();
  virtual_device_factory_->SetCtap2Config(config);
  NavigateAndCommit(GURL(kTestOrigin1));

  // Test that an Ed25519 credential can still be created. (The virtual
  // authenticator supports that algorithm.)
  {
    const int32_t algo =
        static_cast<int32_t>(device::CoseAlgorithmIdentifier::kEdDSA);
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->public_key_parameters = GetTestPublicKeyCredentialParameters(algo);
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    const auto& response = result.response;
    EXPECT_EQ(response->public_key_algo, algo);
  }

  // Test that requesting an unsupported algorithm still collects a touch.
  {
    bool touched = false;
    virtual_device_factory_->mutable_state()->simulate_press_callback =
        base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
          touched = true;
          return true;
        });

    const int32_t algo = static_cast<int32_t>(
        device::CoseAlgorithmIdentifier::kInvalidForTesting);
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->public_key_parameters = GetTestPublicKeyCredentialParameters(algo);
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
    VerifyMakeCredentialOutcomeUkm(
        1, MakeCredentialOutcome::kAlgorithmNotSupported,
        AuthenticationRequestMode::kModalWebAuthn);
    EXPECT_TRUE(touched);
  }
}

TEST_F(AuthenticatorImplTest, VirtualAuthenticatorPublicKeyAlgos) {
  // Exercise all the public key types in the virtual authenticator for create()
  // and get().
  NavigateAndCommit(GURL(kTestOrigin1));

  static const struct {
    STACK_ALLOCATED();

   public:
    device::CoseAlgorithmIdentifier algo;
    const EVP_MD* digest;
  } kTests[] = {
      {device::CoseAlgorithmIdentifier::kEs256, EVP_sha256()},
      {device::CoseAlgorithmIdentifier::kRs256, EVP_sha256()},
      {device::CoseAlgorithmIdentifier::kEdDSA, nullptr},
      {device::CoseAlgorithmIdentifier::kMlDsa44, nullptr},
      {device::CoseAlgorithmIdentifier::kMlDsa65, nullptr},
      {device::CoseAlgorithmIdentifier::kMlDsa87, nullptr},
  };

  std::vector<device::CoseAlgorithmIdentifier> advertised_algorithms;
  for (const auto& test : kTests) {
    advertised_algorithms.push_back(test.algo);
  }

  device::VirtualCtap2Device::Config config;
  config.advertised_algorithms = std::move(advertised_algorithms);
  virtual_device_factory_->SetCtap2Config(config);

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

    bssl::UniquePtr<EVP_PKEY> pkey = crypto::evp::PublicKeyFromBytes(
        create_result.response->public_key_der.value());
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
    std::array<uint8_t, crypto::hash::kSha256Size> client_data_json_hash =
        crypto::hash::Sha256(get_result.response->info->client_data_json);
    signed_data.insert(signed_data.end(), client_data_json_hash.begin(),
                       client_data_json_hash.end());

    bssl::ScopedEVP_MD_CTX md_ctx;
    ASSERT_EQ(EVP_DigestVerifyInit(md_ctx.get(), /*pctx=*/nullptr, test.digest,
                                   /*e=*/nullptr, pkey.get()),
              1);
    EXPECT_EQ(EVP_DigestVerify(md_ctx.get(), signature.data(), signature.size(),
                               signed_data.data(), signed_data.size()),
              1);
  }
}

TEST_F(AuthenticatorImplTest, TestAuthenticationTransport) {
  // TODO(crbug.com/40197472): handle case where the transport is unknown.
  NavigateAndCommit(GURL(kTestOrigin1));
  // Verify transport used during authentication is correctly being returned
  // to the renderer layer.
  for (const device::FidoTransportProtocol transport :
       {device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
        device::FidoTransportProtocol::kBluetoothLowEnergy,
        device::FidoTransportProtocol::kNearFieldCommunication,
        device::FidoTransportProtocol::kInternal}) {
    device::AuthenticatorAttachment attachment =
        (transport == device::FidoTransportProtocol::kInternal
             ? device::AuthenticatorAttachment::kPlatform
             : device::AuthenticatorAttachment::kCrossPlatform);
    ResetVirtualDevice();
    virtual_device_factory_->SetSupportedProtocol(
        device::ProtocolVersion::kCtap2);
    virtual_device_factory_->SetTransport(transport);
    virtual_device_factory_->mutable_state()->transport = transport;

    PublicKeyCredentialCreationOptionsPtr create_options =
        GetTestPublicKeyCredentialCreationOptions();
    MakeCredentialResult create_result =
        AuthenticatorMakeCredential(std::move(create_options));
    ASSERT_EQ(create_result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(create_result.response->authenticator_attachment, attachment);

    PublicKeyCredentialRequestOptionsPtr get_options =
        GetTestPublicKeyCredentialRequestOptions();
    device::PublicKeyCredentialDescriptor public_key(
        device::CredentialType::kPublicKey,
        create_result.response->info->raw_id, {transport});
    get_options->allow_credentials = {std::move(public_key)};
    GetAssertionResult get_result =
        AuthenticatorGetAssertion(std::move(get_options));
    ASSERT_EQ(get_result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(get_result.response->authenticator_attachment, attachment);
  }
}

TEST_F(AuthenticatorImplTest, ResetDiscoveryFactoryOverride) {
  // This is a regression test for crbug.com/1087158.
  NavigateAndCommit(GURL(kTestOrigin1));

  // Make the entire discovery factory disappear mid-request.
  bool IsReady = false;
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        IsReady = true;
        ResetVirtualDevice();
        return false;
      });

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, InvalidU2FPublicKey) {
  NavigateAndCommit(GURL(kTestOrigin1));
  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);
  virtual_device_factory_->mutable_state()->u2f_invalid_public_key = true;

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, InvalidU2FSignature) {
  NavigateAndCommit(GURL(kTestOrigin1));
  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);
  virtual_device_factory_->mutable_state()->u2f_invalid_signature = true;

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, kTestOrigin1));
  options->extensions->appid = kTestOrigin1;

  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, CredBlob) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.cred_blob_support = true;
  // credProtect is required for credBlob per CTAP 2.1.
  config.cred_protect_support = true;
  virtual_device_factory_->SetCtap2Config(config);

  const std::vector<uint8_t> cred_blob = {1, 2, 3, 4};

  std::vector<uint8_t> credential_id;
  // Create a credential with a credBlob set.
  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->cred_blob = cred_blob;
    auto result = AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    credential_id = std::move(result.response->info->raw_id);
    EXPECT_TRUE(result.response->echo_cred_blob);
    EXPECT_TRUE(result.response->cred_blob);
  }

  // Expect to be able to fetch the credBlob with an assertion.
  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials[0] = device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, std::move(credential_id));
    options->extensions->get_cred_blob = true;

    auto result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->extensions->get_cred_blob, cred_blob);
  }
}

TEST_F(AuthenticatorImplTest, MinPINLength) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (const bool min_pin_length_supported : {false, true}) {
    device::VirtualCtap2Device::Config config;
    config.min_pin_length_extension_support = min_pin_length_supported;
    virtual_device_factory_->SetCtap2Config(config);

    for (const bool min_pin_length_requested : {false, true}) {
      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->min_pin_length_requested = min_pin_length_requested;
      auto result = AuthenticatorMakeCredential(std::move(options));
      ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);

      const device::AuthenticatorData auth_data =
          AuthDataFromMakeCredentialResponse(result.response);
      bool has_min_pin_length = false;
      if (auth_data.extensions().has_value()) {
        const cbor::Value::MapValue& extensions =
            auth_data.extensions()->GetMap();
        const auto it =
            extensions.find(cbor::Value(device::kExtensionMinPINLength));
        has_min_pin_length = it != extensions.end() && it->second.is_unsigned();
      }
      ASSERT_EQ(has_min_pin_length,
                min_pin_length_supported && min_pin_length_requested);
    }
  }
}

// Regression test for crbug.com/1257281.
// Tests that a request is not cancelled when an authenticator returns
// CTAP2_ERR_KEEPALIVE_CANCEL after selecting another authenticator for a
// request.
TEST_F(AuthenticatorImplTest, CancellingAuthenticatorDoesNotTerminateRequest) {
  NavigateAndCommit(GURL(kTestOrigin1));
  for (auto request_type : {device::FidoRequestType::kMakeCredential,
                            device::FidoRequestType::kGetAssertion}) {
    SCOPED_TRACE(::testing::Message()
                 << "request_type="
                 << (request_type == device::FidoRequestType::kMakeCredential
                         ? "make_credential"
                         : "get_assertion"));
    // Make a device that supports getting a PUAT with UV.
    auto discovery =
        std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
    device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
    device_1.config.internal_uv_support = true;
    device_1.config.pin_uv_auth_token_support = true;
    device_1.config.user_verification_succeeds = true;
    device_1.config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
    device_1.state->fingerprints_enrolled = true;
    PublicKeyCredentialRequestOptionsPtr dummy_options =
        GetTestPublicKeyCredentialRequestOptions();
    ASSERT_TRUE(device_1.state->InjectRegistration(
        dummy_options->allow_credentials[0].id, kTestRelyingPartyId));
    discovery->AddDevice(std::move(device_1));

    // Make a device that does not support PUATs but can still handle the
    // request. This device will not respond to the request.
    device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_2;
    device_2.config.internal_uv_support = false;
    device_2.config.pin_uv_auth_token_support = false;
    device_2.config.ctap2_versions = {device::Ctap2Version::kCtap2_0};
    device_2.state->simulate_press_callback =
        base::BindRepeating([](VirtualFidoDevice* ignore) { return false; });
    discovery->AddDevice(std::move(device_2));
    ReplaceDiscoveryFactory(std::move(discovery));

    if (request_type == device::FidoRequestType::kMakeCredential) {
      MakeCredentialResult result = AuthenticatorMakeCredential();
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    } else {
      GetAssertionResult result = AuthenticatorGetAssertion();
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    }
  }
}

TEST_F(AuthenticatorImplTest, PRFWithoutSupport) {
  // This tests that the PRF extension doesn't trigger any DCHECKs or crashes
  // when used with an authenticator doesn't doesn't support hmac-secret.
  NavigateAndCommit(GURL(kTestOrigin1));

  auto prf_value = blink::mojom::PRFValues::New();
  const std::vector<uint8_t> salt1(32, 1);
  prf_value->first = salt1;
  std::vector<blink::mojom::PRFValuesPtr> prf_inputs;
  prf_inputs.emplace_back(std::move(prf_value));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->extensions->prf = true;
  options->extensions->prf_inputs = std::move(prf_inputs);

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

  EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// These test verify that the virtual authenticator supports the Signal API.
class VirtualAuthenticatorSignalTest : public AuthenticatorImplTest {
 public:
  static constexpr char kUsername[] = "reimu";
  static constexpr char kDisplayName[] = "Reimu Hakurei";
  const std::vector<uint8_t> kUserId = {2};

  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    NavigateAndCommit(GURL(kTestOrigin1));

    // These tests need an AuthenticatorEnvironment set up.
    virtual_device_factory_ = nullptr;
    content::AuthenticatorEnvironment* authenticator_environment =
        content::AuthenticatorEnvironment::GetInstance();
    authenticator_environment->Reset();
    FrameTreeNode* frame_tree_node =
        static_cast<content::RenderFrameHostImpl*>(main_rfh())
            ->frame_tree_node();
    authenticator_environment->EnableVirtualAuthenticatorFor(
        frame_tree_node,
        /*enable_ui=*/false);
    VirtualAuthenticatorManagerImpl* virtual_authenticator_manager =
        authenticator_environment->MaybeGetVirtualAuthenticatorManager(
            frame_tree_node);
    VirtualAuthenticator::Options virt_auth_options;
    virt_auth_options.protocol = device::ProtocolVersion::kCtap2;
    virt_auth_options.transport = device::FidoTransportProtocol::kInternal;
    virt_auth_options.has_resident_key = true;
    authenticator_ =
        virtual_authenticator_manager
            ->AddAuthenticatorAndReturnNonOwningPointer(virt_auth_options);

    // Make a credential.
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->user.id = kUserId;
    options->user.name = kUsername;
    options->user.display_name = kDisplayName;
    options->authenticator_selection->resident_key =
        device::ResidentKeyRequirement::kRequired;
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    credential_id_ = result.response->info->raw_id;
  }

  void TearDown() override {
    authenticator_ = nullptr;
    AuthenticatorImplTest::TearDown();
  }

 protected:
  // The id of the credential created during test setup.
  std::vector<uint8_t> credential_id_;

  raw_ptr<VirtualAuthenticator> authenticator_;
};

TEST_F(VirtualAuthenticatorSignalTest, SignalUnknownCredentialId) {
  {
    // Verify that we do not remove passkeys that don't match the rp id.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kDifferentTestRelyingPartyId;
    options->unknown_credential_id = credential_id_;
    AuthenticatorReport(std::move(options));
    EXPECT_TRUE(authenticator_->registrations().contains(credential_id_));
  }
  {
    // Verify that we do not remove passkeys that don't match the cred id.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kTestRelyingPartyId;
    options->unknown_credential_id = std::vector<uint8_t>{4, 3, 2, 1};
    AuthenticatorReport(std::move(options));
    EXPECT_TRUE(authenticator_->registrations().contains(credential_id_));
  }
  {
    // Remove the passkey when the rp id and credential id match.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kTestRelyingPartyId;
    options->unknown_credential_id = credential_id_;
    AuthenticatorReport(std::move(options));
    EXPECT_FALSE(authenticator_->registrations().contains(credential_id_));
  }
}

TEST_F(VirtualAuthenticatorSignalTest, SignalAllAcceptableCredentials) {
  {
    // Verify that we do not remove passkeys that don't match the rp id.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kDifferentTestRelyingPartyId;
    options->all_accepted_credentials =
        blink::mojom::AllAcceptedCredentialsOptions::New(
            kUserId, std::vector<std::vector<uint8_t>>{});
    AuthenticatorReport(std::move(options));
    EXPECT_TRUE(authenticator_->registrations().contains(credential_id_));
  }
  {
    // Verify that we do not remove passkeys that don't match the user id.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kTestRelyingPartyId;
    options->all_accepted_credentials =
        blink::mojom::AllAcceptedCredentialsOptions::New(
            std::vector<uint8_t>{99}, std::vector<std::vector<uint8_t>>{});
    AuthenticatorReport(std::move(options));
    EXPECT_TRUE(authenticator_->registrations().contains(credential_id_));
  }
  {
    // Verify that we do not remove passkeys that are present on the list.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kTestRelyingPartyId;
    options->all_accepted_credentials =
        blink::mojom::AllAcceptedCredentialsOptions::New(
            kUserId, std::vector<std::vector<uint8_t>>{credential_id_});
    AuthenticatorReport(std::move(options));
    EXPECT_TRUE(authenticator_->registrations().contains(credential_id_));
  }
  {
    // Verify that we remove passkeys that are not present on the list.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kTestRelyingPartyId;
    options->all_accepted_credentials =
        blink::mojom::AllAcceptedCredentialsOptions::New(
            kUserId, std::vector<std::vector<uint8_t>>{});
    AuthenticatorReport(std::move(options));
    EXPECT_FALSE(authenticator_->registrations().contains(credential_id_));
  }
}

TEST_F(VirtualAuthenticatorSignalTest, SignalCurrentUserDetails) {
  constexpr char kNewUsername[] = "marisa";
  constexpr char kNewDisplayName[] = "Marisa Kirisame";
  {
    // Verify that we do not update passkeys that don't match the rp id.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kDifferentTestRelyingPartyId;
    options->current_user_details =
        blink::mojom::CurrentUserDetailsOptions::New(kUserId, kNewUsername,
                                                     kNewDisplayName);
    AuthenticatorReport(std::move(options));
    const auto& cred =
        authenticator_->registrations().find(credential_id_)->second;
    EXPECT_EQ(cred.user->name, kUsername);
    EXPECT_EQ(cred.user->display_name, kDisplayName);
  }
  {
    // Verify that we do not update passkeys that don't match the user id.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kTestRelyingPartyId;
    options->current_user_details =
        blink::mojom::CurrentUserDetailsOptions::New(
            std::vector<uint8_t>{9}, kNewUsername, kNewDisplayName);
    AuthenticatorReport(std::move(options));
    const auto& cred =
        authenticator_->registrations().find(credential_id_)->second;
    EXPECT_EQ(cred.user->name, kUsername);
    EXPECT_EQ(cred.user->display_name, kDisplayName);
  }
  {
    // Verify that we do update passkeys that match.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kTestRelyingPartyId;
    options->current_user_details =
        blink::mojom::CurrentUserDetailsOptions::New(kUserId, kNewUsername,
                                                     kNewDisplayName);
    AuthenticatorReport(std::move(options));
    const auto& cred =
        authenticator_->registrations().find(credential_id_)->second;
    EXPECT_EQ(cred.user->name, kNewUsername);
    EXPECT_EQ(cred.user->display_name, kNewDisplayName);
  }
}

class BlockingAuthenticatorRequestDelegate
    : public DefaultAuthenticatorRequestClientDelegate {
 public:
  BlockingAuthenticatorRequestDelegate() = default;

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
    cancel_callback_ = std::move(cancel_callback);
  }

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override {
    // Post a task to cancel the request to give the second authenticator a
    // chance to return a status from the cancelled request.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(cancel_callback_));
    return true;
  }

 private:
  base::OnceClosure cancel_callback_;
};

class BlockingDelegateContentBrowserClient : public ContentBrowserClient {
 public:
  BlockingDelegateContentBrowserClient() = default;

  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate_;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<BlockingAuthenticatorRequestDelegate>();
  }

 private:
  TestWebAuthenticationDelegate web_authentication_delegate_;
};

class BlockingDelegateAuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  BlockingDelegateAuthenticatorImplTest() = default;

  BlockingDelegateAuthenticatorImplTest(
      const BlockingDelegateAuthenticatorImplTest&) = delete;
  BlockingDelegateAuthenticatorImplTest& operator=(
      const BlockingDelegateAuthenticatorImplTest&) = delete;

  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
    NavigateAndCommit(GURL(kTestOrigin1));
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

 protected:
  BlockingDelegateContentBrowserClient test_client_;

 private:
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

TEST_F(BlockingDelegateAuthenticatorImplTest, PostCancelMessage) {
  // Create a fingerprint-reading device and a UP-only device. Advance the
  // first till it's waiting for a fingerprint then simulate a touch on the
  // UP device that claims that it failed due to an excluded credential.
  // This will cancel the request on the fingerprint device, which will resolve
  // the UV with an error. Don't crash (crbug.com/1225899).
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = GetTestCredentials();

  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
  scoped_refptr<VirtualFidoDevice::State> state_1 = device_1.state;
  device_1.state->simulate_press_callback =
      base::BindLambdaForTesting([&](VirtualFidoDevice* ignore) -> bool {
        // Drop all makeCredential requests. The reply will be sent when
        // the second authenticator is asked for a fingerprint.
        return false;
      });

  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_2;
  scoped_refptr<VirtualFidoDevice::State> state_2 = device_2.state;
  device_2.config.internal_uv_support = true;
  device_2.config.pin_support = true;
  device_2.config.pin_uv_auth_token_support = true;
  device_2.config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  device_2.state->pin = kTestPIN;
  device_2.state->fingerprints_enrolled = true;
  device_2.state->uv_retries = 8;
  device_2.state->cancel_response_code =
      device::CtapDeviceResponseCode::kCtap2ErrOperationDenied;
  device_2.state->simulate_press_callback =
      base::BindLambdaForTesting([&](VirtualFidoDevice* ignore) -> bool {
        // If asked for a fingerprint, fail the makeCredential request by
        // simulating a matched excluded credential by the other authenticator.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(state_1->transact_callback),
                                      std::vector<uint8_t>{static_cast<uint8_t>(
                                          device::CtapDeviceResponseCode::
                                              kCtap2ErrCredentialExcluded)}));
        return false;
      });

  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  discovery->AddDevice(std::move(device_1));
  discovery->AddDevice(std::move(device_2));
  ReplaceDiscoveryFactory(std::move(discovery));

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::CREDENTIAL_EXCLUDED);
}

// AuthenticatorImplWithRequestProxyTest tests behavior with an installed
// TestWebAuthenticationRequestProxy that takes over WebAuthn request handling.
class AuthenticatorImplWithRequestProxyTest : public AuthenticatorImplTest {
 protected:
  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
    test_client_.GetTestWebAuthenticationDelegate()->request_proxy =
        std::make_unique<TestWebAuthenticationRequestProxy>();
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

  TestWebAuthenticationRequestProxy& request_proxy() {
    return static_cast<TestWebAuthenticationRequestProxy&>(
        *test_client_.GetTestWebAuthenticationDelegate()->request_proxy);
  }

  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
  TestAuthenticatorContentBrowserClient test_client_;
};

TEST_F(AuthenticatorImplWithRequestProxyTest, Inactive) {
  request_proxy().config().is_active = false;
  NavigateAndCommit(GURL(kTestOrigin1));
  AuthenticatorIsUvpaa();
  EXPECT_EQ(request_proxy().observations().num_isuvpaa, 0u);
}

TEST_F(AuthenticatorImplWithRequestProxyTest, IsUVPAA) {
  size_t i = 0;
  for (const bool is_uvpaa : {false, true}) {
    SCOPED_TRACE(testing::Message() << "is_uvpaa=" << is_uvpaa);
    request_proxy().config().is_uvpaa = is_uvpaa;
    NavigateAndCommit(GURL(kTestOrigin1));
    EXPECT_EQ(AuthenticatorIsUvpaa(), is_uvpaa);
    EXPECT_EQ(request_proxy().observations().num_isuvpaa, ++i);
  }
}

TEST_F(AuthenticatorImplWithRequestProxyTest, IsConditionalMediationAvailable) {
  // We can't autofill credentials over the request proxy. Hence, conditional
  // mediation is unavailable, even if IsUVPAA returns true.
  NavigateAndCommit(GURL(kTestOrigin1));

  // Ensure there is no test override set and we're testing the real
  // implementation.
  ASSERT_EQ(test_client_.GetTestWebAuthenticationDelegate()->is_uvpaa_override,
            std::nullopt);

  // Proxy says `IsUVPAA()` is true.
  request_proxy().config().is_uvpaa = true;
  EXPECT_TRUE(AuthenticatorIsUvpaa());
  EXPECT_EQ(request_proxy().observations().num_isuvpaa, 1u);

  // But `IsConditionalMediationAvailable()` still returns false, bypassing the
  // proxy.
  EXPECT_FALSE(AuthenticatorIsConditionalMediationAvailable());
  EXPECT_EQ(request_proxy().observations().num_isuvpaa, 1u);
}

TEST_F(AuthenticatorImplWithRequestProxyTest,
       GetClientCapabilities_ConditionalGet_ReturnsFalse) {
  // We can't autofill credentials over the request proxy. Hence, conditional
  // mediation is unavailable, even if IsUVPAA returns true.
  NavigateAndCommit(GURL(kTestOrigin1));
  ASSERT_EQ(test_client_.GetTestWebAuthenticationDelegate()->is_uvpaa_override,
            std::nullopt);
  request_proxy().config().is_uvpaa = true;

  // Internally, `IsConditionalMediationAvailable()` should returns `false`,
  // bypassing the proxy.
  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
  ExpectCapability(capabilities, client_capabilities::kConditionalGet, false);
}

TEST_F(AuthenticatorImplWithRequestProxyTest, MakeCredential) {
  request_proxy().config().request_success = true;
  request_proxy().config().make_credential_response =
      MakeCredentialAuthenticatorResponse::New();
  request_proxy().config().make_credential_response->info =
      CommonCredentialInfo::New();

  NavigateAndCommit(GURL(kTestOrigin1));
  auto request = GetTestPublicKeyCredentialCreationOptions();
  MakeCredentialResult result = AuthenticatorMakeCredential(request->Clone());

  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(request_proxy().observations().num_cancel, 0u);
  EXPECT_EQ(request_proxy().observations().create_requests.size(), 1u);

  auto expected = request->Clone();
  expected->remote_desktop_client_override = RemoteDesktopClientOverride::New();
  expected->remote_desktop_client_override->origin =
      url::Origin::Create(GURL(kTestOrigin1));
  expected->remote_desktop_client_override->same_origin_with_ancestors = true;
  EXPECT_EQ(request_proxy().observations().create_requests.at(0), expected);
}

// remoteClientDataJSON is mutually exclusive with an active request proxy: an
// already-proxied request must not be re-proxied, so a request carrying the
// extension is rejected with NOT_ALLOWED_ERROR and never forwarded to the
// proxy.
TEST_F(AuthenticatorImplWithRequestProxyTest, RemoteClientDataJsonNotProxied) {
  // remoteClientDataJSON is gated by a browser-side feature check; enable it so
  // the request reaches the proxy mutual-exclusion logic under test.
  base::test::ScopedFeatureList scoped_feature_list(
      device::kWebAuthnRemoteClientDataJson);
  request_proxy().config().is_active = true;
  // Authorize the caller to use the override extensions so the request passes
  // RP ID validation and reaches the proxy mutual-exclusion check.
  test_client_.GetTestWebAuthenticationDelegate()
      ->remote_desktop_client_override_origin =
      url::Origin::Create(GURL(kTestOrigin1));

  NavigateAndCommit(GURL(kTestOrigin1));
  auto request = GetTestPublicKeyCredentialCreationOptions();
  request->remote_client_data_json =
      R"({"type":"webauthn.create","challenge":"dGVzdA",)"
      R"("origin":"https://example.test","crossOrigin":false})";
  MakeCredentialResult result = AuthenticatorMakeCredential(std::move(request));

  EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
  // The proxy must never have been asked to handle the request.
  EXPECT_EQ(request_proxy().observations().create_requests.size(), 0u);
}

// Verify requests with an attached proxy run RP ID checks.
TEST_F(AuthenticatorImplWithRequestProxyTest, MakeCredentialOriginAndRpIds) {
  request_proxy().config().request_success = true;
  request_proxy().config().make_credential_response =
      MakeCredentialAuthenticatorResponse::New();
  request_proxy().config().make_credential_response->info =
      CommonCredentialInfo::New();

  for (const OriginClaimedAuthorityPair& test_case :
       webauthn::test::GetInvalidRpTestCases()) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    NavigateAndCommit(GURL(test_case.origin));
    BrowserContext* context = main_rfh()->GetBrowserContext();
    ASSERT_TRUE(
        test_client_.GetWebAuthenticationDelegate()->MaybeGetRequestProxy(
            context, url::Origin::Create(GURL(test_case.origin))));

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              test_case.expected_status);
    EXPECT_EQ(request_proxy().observations().create_requests.size(), 0u);
  }
}

// Tests that attempting to make a credential when a request is already proxied
// fails with NotAllowedError.
TEST_F(AuthenticatorImplWithRequestProxyTest, MakeCredentialAlreadyProxied) {
  GURL origin(kCorpCrdOrigin);
  test_client_.GetTestWebAuthenticationDelegate()
      ->remote_desktop_client_override_origin = url::Origin::Create(origin);
  NavigateAndCommit(origin);
  auto request = GetTestPublicKeyCredentialCreationOptions();
  request->remote_desktop_client_override =
      RemoteDesktopClientOverride::New(url::Origin::Create(origin), true);
  MakeCredentialResult result = AuthenticatorMakeCredential(std::move(request));

  EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(request_proxy().observations().create_requests.size(), 0u);
}

TEST_F(AuthenticatorImplWithRequestProxyTest, AppId) {
  request_proxy().config().request_success = true;
  request_proxy().config().make_credential_response =
      MakeCredentialAuthenticatorResponse::New();
  request_proxy().config().make_credential_response->info =
      CommonCredentialInfo::New();

  for (const auto& test_case : kValidAppIdCases) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    BrowserContext* context = main_rfh()->GetBrowserContext();
    ASSERT_TRUE(
        test_client_.GetWebAuthenticationDelegate()->MaybeGetRequestProxy(
            context, url::Origin::Create(GURL(test_case.origin))));

    EXPECT_EQ(TryAuthenticationWithAppId(test_case.origin,
                                         test_case.claimed_authority),
              AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(request_proxy().observations().get_requests.size(), 1u);
    request_proxy().observations().get_requests.clear();

    EXPECT_EQ(TryRegistrationWithAppIdExclude(test_case.origin,
                                              test_case.claimed_authority),
              AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(request_proxy().observations().create_requests.size(), 1u);
    request_proxy().observations().create_requests.clear();
  }

  // Test invalid cases that should be rejected. `kInvalidRpTestCases`
  // contains a mix of RP ID an App ID cases, but they should all be rejected.
  for (const OriginClaimedAuthorityPair& test_case :
       webauthn::test::GetInvalidRpTestCases()) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    if (test_case.claimed_authority.empty()) {
      // In this case, no AppID is actually being tested.
      continue;
    }

    BrowserContext* context = main_rfh()->GetBrowserContext();
    ASSERT_TRUE(
        test_client_.GetWebAuthenticationDelegate()->MaybeGetRequestProxy(
            context, url::Origin::Create(GURL(test_case.origin))));

    AuthenticatorStatus test_status = TryAuthenticationWithAppId(
        test_case.origin, test_case.claimed_authority);
    EXPECT_TRUE(test_status == AuthenticatorStatus::INVALID_DOMAIN ||
                test_status == test_case.expected_status);
    EXPECT_EQ(request_proxy().observations().get_requests.size(), 0u);

    test_status = TryRegistrationWithAppIdExclude(test_case.origin,
                                                  test_case.claimed_authority);
    EXPECT_TRUE(test_status == AuthenticatorStatus::INVALID_DOMAIN ||
                test_status == test_case.expected_status);
    EXPECT_EQ(request_proxy().observations().create_requests.size(), 0u);
  }
}

TEST_F(AuthenticatorImplWithRequestProxyTest, MakeCredential_Timeout) {
  request_proxy().config().resolve_callbacks = false;
  request_proxy().config().request_success = true;
  request_proxy().config().make_credential_response =
      MakeCredentialAuthenticatorResponse::New();
  request_proxy().config().make_credential_response->info =
      CommonCredentialInfo::New();

  NavigateAndCommit(GURL(kTestOrigin1));
  MakeCredentialResult result = AuthenticatorMakeCredentialAndWaitForTimeout(
      GetTestPublicKeyCredentialCreationOptions());

  EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(request_proxy().observations().create_requests.size(), 1u);
  EXPECT_EQ(request_proxy().observations().num_cancel, 1u);

  // Proxy should not hold a pending request after cancellation.
  EXPECT_FALSE(request_proxy().HasPendingRequest());
}

TEST_F(AuthenticatorImplWithRequestProxyTest, GetAssertion) {
  request_proxy().config().request_success = true;
  request_proxy().config().get_assertion_response =
      GetAssertionAuthenticatorResponse::New();
  request_proxy().config().get_assertion_response->info =
      CommonCredentialInfo::New();
  request_proxy().config().get_assertion_response->extensions =
      AuthenticationExtensionsClientOutputs::New();

  NavigateAndCommit(GURL(kTestOrigin1));
  auto request = GetTestPublicKeyCredentialRequestOptions();
  GetAssertionResult result = AuthenticatorGetAssertion(request->Clone());

  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(request_proxy().observations().num_cancel, 0u);
  EXPECT_EQ(request_proxy().observations().get_requests.size(), 1u);

  auto expected = request->Clone();
  expected->extensions->remote_desktop_client_override =
      RemoteDesktopClientOverride::New();
  expected->extensions->remote_desktop_client_override->origin =
      url::Origin::Create(GURL(kTestOrigin1));
  expected->extensions->remote_desktop_client_override
      ->same_origin_with_ancestors = true;
  EXPECT_EQ(request_proxy().observations().get_requests.at(0), expected);
}

// Tests that attempting to get an assertion when a request is already proxied
// fails with NotAllowedError.
TEST_F(AuthenticatorImplWithRequestProxyTest, GetAssertionAlreadyProxied) {
  GURL origin(kCorpCrdOrigin);
  test_client_.GetTestWebAuthenticationDelegate()
      ->remote_desktop_client_override_origin = url::Origin::Create(origin);
  NavigateAndCommit(origin);
  auto request = GetTestPublicKeyCredentialRequestOptions();
  request->extensions->remote_desktop_client_override =
      RemoteDesktopClientOverride::New(url::Origin::Create(origin), true);
  GetAssertionResult result = AuthenticatorGetAssertion(std::move(request));

  EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(request_proxy().observations().get_requests.size(), 0u);
}

// Verify that Conditional UI requests are not proxied.
TEST_F(AuthenticatorImplWithRequestProxyTest, GetAssertionConditionalUI) {
  NavigateAndCommit(GURL(kTestOrigin1));
  auto request = GetTestPublicKeyCredentialRequestOptions();
  auto options = GetCredentialOptions::New();
  options->mediation = blink::mojom::Mediation::CONDITIONAL;
  options->public_key = std::move(request);
  GetAssertionResult result = AuthenticatorGetCredential(std::move(options));

  EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(request_proxy().observations().get_requests.size(), 0u);
}

// Verify requests with an attached proxy run RP ID checks.
TEST_F(AuthenticatorImplWithRequestProxyTest, GetAssertionOriginAndRpIds) {
  request_proxy().config().request_success = true;
  request_proxy().config().get_assertion_response =
      GetAssertionAuthenticatorResponse::New();
  request_proxy().config().get_assertion_response->info =
      CommonCredentialInfo::New();

  for (const OriginClaimedAuthorityPair& test_case :
       webauthn::test::GetInvalidRpTestCases()) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    NavigateAndCommit(GURL(test_case.origin));
    BrowserContext* context = main_rfh()->GetBrowserContext();
    ASSERT_TRUE(
        test_client_.GetWebAuthenticationDelegate()->MaybeGetRequestProxy(
            context, url::Origin::Create(GURL(test_case.origin))));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              test_case.expected_status);
    EXPECT_EQ(request_proxy().observations().get_requests.size(), 0u);
  }
}

TEST_F(AuthenticatorImplWithRequestProxyTest, GetAssertion_Timeout) {
  request_proxy().config().resolve_callbacks = false;
  request_proxy().config().request_success = true;
  request_proxy().config().get_assertion_response =
      GetAssertionAuthenticatorResponse::New();
  request_proxy().config().get_assertion_response->info =
      CommonCredentialInfo::New();

  NavigateAndCommit(GURL(kTestOrigin1));
  GetAssertionResult result = AuthenticatorGetAssertionAndWaitForTimeout(
      GetTestPublicKeyCredentialRequestOptions());

  EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(request_proxy().observations().get_requests.size(), 1u);
  EXPECT_EQ(request_proxy().observations().num_cancel, 1u);

  // Proxy should not hold a pending request after cancellation.
  EXPECT_FALSE(request_proxy().HasPendingRequest());
}

TEST_F(AuthenticatorImplWithRequestProxyTest,
       VirtualAuthenticatorTakesPrecedence) {
  // With the virtual authenticator enabled, no requests should hit the proxy.
  content::AuthenticatorEnvironment::GetInstance()
      ->EnableVirtualAuthenticatorFor(
          static_cast<content::RenderFrameHostImpl*>(main_rfh())
              ->frame_tree_node(),
          /*enable_ui=*/false);
  test_client_.GetTestWebAuthenticationDelegate()->is_uvpaa_override = true;

  NavigateAndCommit(GURL(kTestOrigin1));
  ASSERT_TRUE(
      request_proxy().IsActive(url::Origin::Create(GURL(kTestOrigin1))));

  {
    MakeCredentialResult result = AuthenticatorMakeCredential(
        GetTestPublicKeyCredentialCreationOptions());
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(request_proxy().observations().create_requests.size(), 0u);
  }

  {
    GetAssertionResult result =
        AuthenticatorGetAssertion(GetTestPublicKeyCredentialRequestOptions());
    EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
    EXPECT_EQ(request_proxy().observations().get_requests.size(), 0u);
  }

  EXPECT_TRUE(AuthenticatorIsUvpaa());
  EXPECT_EQ(request_proxy().observations().num_isuvpaa, 0u);
  EXPECT_TRUE(AuthenticatorIsConditionalMediationAvailable());
  EXPECT_EQ(request_proxy().observations().num_isuvpaa, 0u);
}

TEST_F(AuthenticatorImplTest, CrossDeviceFallbackUrl_Valid) {
  base::test::ScopedFeatureList feature_list(
      device::kWebAuthnCrossDeviceFallbackUrl);
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->transport =
      device::FidoTransportProtocol::kHybrid;

  auto options = GetTestGetCredentialOptions();
  options->public_key->extensions->cross_device_fallback_url =
      GURL("https://a.google.com/fallback");

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->public_key->allow_credentials[0].id, kTestRelyingPartyId));

  GetAssertionResult result = AuthenticatorGetCredential(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  ASSERT_TRUE(result.response);
  ASSERT_TRUE(result.response->extensions);
  ASSERT_TRUE(
      result.response->extensions->cross_device_fallback_url.has_value());
  EXPECT_TRUE(*result.response->extensions->cross_device_fallback_url);

  auto last_request =
      virtual_device_factory_->mutable_state()->last_get_assertion_request;
  ASSERT_TRUE(last_request.has_value());
  ASSERT_TRUE(last_request->cross_device_fallback_url.has_value());
  EXPECT_EQ(*last_request->cross_device_fallback_url,
            "https://a.google.com/fallback");
}

TEST_F(AuthenticatorImplTest, CrossDeviceFallbackUrl_InvalidOrigin) {
  base::test::ScopedFeatureList feature_list(
      device::kWebAuthnCrossDeviceFallbackUrl);
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->transport =
      device::FidoTransportProtocol::kHybrid;

  auto options = GetTestGetCredentialOptions();
  options->public_key->extensions->cross_device_fallback_url =
      GURL("https://other.com/fallback");

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->public_key->allow_credentials[0].id, kTestRelyingPartyId));

  AuthenticatorGetCredential(std::move(options));

  auto last_request =
      virtual_device_factory_->mutable_state()->last_get_assertion_request;
  ASSERT_TRUE(last_request.has_value());
  EXPECT_FALSE(last_request->cross_device_fallback_url.has_value());
}

TEST_F(AuthenticatorImplTest, CrossDeviceFallbackUrl_InvalidScheme) {
  base::test::ScopedFeatureList feature_list(
      device::kWebAuthnCrossDeviceFallbackUrl);
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->transport =
      device::FidoTransportProtocol::kHybrid;

  auto options = GetTestGetCredentialOptions();
  options->public_key->extensions->cross_device_fallback_url =
      GURL("http://a.google.com/fallback");

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->public_key->allow_credentials[0].id, kTestRelyingPartyId));

  AuthenticatorGetCredential(std::move(options));

  auto last_request =
      virtual_device_factory_->mutable_state()->last_get_assertion_request;
  ASSERT_TRUE(last_request.has_value());
  EXPECT_FALSE(last_request->cross_device_fallback_url.has_value());
}

TEST_F(AuthenticatorImplTest, CrossDeviceFallbackUrl_BlockedByCSP) {
  base::test::ScopedFeatureList feature_list(
      device::kWebAuthnCrossDeviceFallbackUrl);
  NavigateAndCommit(GURL(kTestOrigin1));

  // Set CSP to block the fallback URL.
  auto policies = network::ParseContentSecurityPolicies(
      "connect-src https://allowed.com",
      network::mojom::ContentSecurityPolicyType::kEnforce,
      network::mojom::ContentSecurityPolicySource::kHTTP, GURL(kTestOrigin1));
  static_cast<RenderFrameHostImpl*>(main_rfh())
      ->policy_container_host()
      ->AddContentSecurityPoliciesForTesting(std::move(policies));

  device::VirtualCtap2Device::Config config;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->transport =
      device::FidoTransportProtocol::kHybrid;

  auto options = GetTestGetCredentialOptions();
  options->public_key->extensions->cross_device_fallback_url =
      GURL("https://a.google.com/fallback");

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->public_key->allow_credentials[0].id, kTestRelyingPartyId));

  AuthenticatorGetCredential(std::move(options));

  auto last_request =
      virtual_device_factory_->mutable_state()->last_get_assertion_request;
  ASSERT_TRUE(last_request.has_value());
  EXPECT_FALSE(last_request->cross_device_fallback_url.has_value());
}

TEST_F(AuthenticatorImplTest, CrossDeviceFallbackUrl_FlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(device::kWebAuthnCrossDeviceFallbackUrl);
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->transport =
      device::FidoTransportProtocol::kHybrid;

  auto options = GetTestGetCredentialOptions();
  options->public_key->extensions->cross_device_fallback_url =
      GURL("https://a.google.com/fallback");

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->public_key->allow_credentials[0].id, kTestRelyingPartyId));

  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  mojo::test::BadMessageObserver bad_message_observer;
  authenticator->GetCredential(std::move(options), base::DoNothing());
  EXPECT_EQ(bad_message_observer.WaitForBadMessage(),
            "crossDeviceFallbackUrl extension sent but feature disabled");
}

TEST_F(AuthenticatorImplTest, CrossDeviceFallbackUrl_Processed) {
  base::test::ScopedFeatureList feature_list(
      device::kWebAuthnCrossDeviceFallbackUrl);
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.override_response_map
      [device::CtapRequestCommand::kAuthenticatorGetAssertion] = std::make_pair(
      device::CtapDeviceResponseCode::kCtap2ErrFallbackUrlProcessed,
      std::nullopt);
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->transport =
      device::FidoTransportProtocol::kHybrid;

  auto options = GetTestGetCredentialOptions();
  options->public_key->extensions->cross_device_fallback_url =
      GURL("https://a.google.com/fallback");

  GetAssertionResult result = AuthenticatorGetCredential(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::CROSS_DEVICE_FALLBACK);
}

TEST_F(AuthenticatorImplTest, InactiveRenderFrameHost) {
  NavigateAndCommit(GURL(kTestOrigin1));

  // Set the lifecycle state to `kInBackForwardCache` so the RenderFrameHost is
  // inactive.
  static_cast<RenderFrameHostImpl*>(main_rfh())
      ->SetLifecycleState(
          RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  ASSERT_FALSE(main_rfh()->IsActive());

  // Try to connect to the authenticator service.
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // The receiver should be dropped immediately, causing the remote to be
  // disconnected.
  base::RunLoop run_loop;
  authenticator.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(authenticator.is_connected());
}

TEST_F(AuthenticatorImplTest, CmtgKeyEndToEnd) {
  NavigateAndCommit(GURL(kTestOrigin1));

  // Configure device with CMTG support.
  device::VirtualCtap2Device::Config config;
  config.cmtg_key_support = true;
  virtual_device_factory_->SetCtap2Config(config);

  // Create credential with CMTG extension.
  PublicKeyCredentialCreationOptionsPtr create_options =
      GetTestPublicKeyCredentialCreationOptions();
  create_options->cmtg_key = true;
  MakeCredentialResult create_result =
      AuthenticatorMakeCredential(std::move(create_options));
  ASSERT_EQ(create_result.status, AuthenticatorStatus::SUCCESS);
  ASSERT_TRUE(create_result.response->cmtg_key);
  const std::vector<uint8_t> initial_cmtg_key =
      create_result.response->cmtg_key->cmtg_key;
  const std::vector<uint8_t> credential_id =
      create_result.response->info->raw_id;

  // Get an assertion with the same CMTG key.
  PublicKeyCredentialRequestOptionsPtr get_options1 =
      GetTestPublicKeyCredentialRequestOptions();
  get_options1->allow_credentials[0].id = credential_id;
  get_options1->extensions->cmtg_key = true;
  GetAssertionResult get_result1 =
      AuthenticatorGetAssertion(std::move(get_options1));
  ASSERT_EQ(get_result1.status, AuthenticatorStatus::SUCCESS);
  ASSERT_TRUE(get_result1.response->extensions->cmtg_key);
  EXPECT_EQ(get_result1.response->extensions->cmtg_key->cmtg_key,
            initial_cmtg_key);

  // Trigger generation of a new CMTG key on next assertion.
  virtual_device_factory_->mutable_state()
      ->registrations.at(credential_id)
      .generate_cmtg_key_on_next_operation = true;
  PublicKeyCredentialRequestOptionsPtr get_options2 =
      GetTestPublicKeyCredentialRequestOptions();
  get_options2->allow_credentials[0].id = credential_id;
  get_options2->extensions->cmtg_key = true;
  GetAssertionResult get_result2 =
      AuthenticatorGetAssertion(std::move(get_options2));
  ASSERT_EQ(get_result2.status, AuthenticatorStatus::SUCCESS);
  ASSERT_TRUE(get_result2.response->extensions->cmtg_key);
  EXPECT_NE(get_result2.response->extensions->cmtg_key->cmtg_key,
            initial_cmtg_key);
}

class AuthenticatorImplVirtualAuthenticatorTransportTest
    : public AuthenticatorImplTest,
      public ::testing::WithParamInterface<device::FidoTransportProtocol> {
 protected:
  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    NavigateAndCommit(GURL(kTestOrigin1));
    virtual_device_factory_ = nullptr;
    content::AuthenticatorEnvironment::GetInstance()->Reset();
  }
};

TEST_P(AuthenticatorImplVirtualAuthenticatorTransportTest,
       MakeCredentialAndGetAssertion) {
  device::FidoTransportProtocol transport = GetParam();

  content::AuthenticatorEnvironment* authenticator_environment =
      content::AuthenticatorEnvironment::GetInstance();
  FrameTreeNode* frame_tree_node =
      static_cast<content::RenderFrameHostImpl*>(main_rfh())->frame_tree_node();
  authenticator_environment->EnableVirtualAuthenticatorFor(frame_tree_node,
                                                           /*enable_ui=*/false);
  VirtualAuthenticatorManagerImpl* virtual_authenticator_manager =
      authenticator_environment->MaybeGetVirtualAuthenticatorManager(
          frame_tree_node);

  VirtualAuthenticator::Options virt_auth_options;
  virt_auth_options.protocol = device::ProtocolVersion::kCtap2;
  virt_auth_options.transport = transport;

  if (transport == device::FidoTransportProtocol::kInternal) {
    virt_auth_options.has_user_verification = true;
    virt_auth_options.attachment = device::AuthenticatorAttachment::kPlatform;
  }

  VirtualAuthenticator* authenticator =
      virtual_authenticator_manager->AddAuthenticatorAndReturnNonOwningPointer(
          std::move(virt_auth_options));
  ASSERT_TRUE(authenticator);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  if (transport == device::FidoTransportProtocol::kInternal) {
    options->authenticator_selection = device::AuthenticatorSelectionCriteria(
        device::AuthenticatorAttachment::kPlatform,
        device::ResidentKeyRequirement::kDiscouraged,
        device::UserVerificationRequirement::kPreferred);
  }

  MakeCredentialResult result = AuthenticatorMakeCredential(std::move(options));
  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(std::ranges::contains(result.response->transports, transport));

  const std::vector<uint8_t> credential_id = result.response->info->raw_id;

  PublicKeyCredentialRequestOptionsPtr get_options =
      GetTestPublicKeyCredentialRequestOptions();
  get_options->allow_credentials[0].id = credential_id;
  get_options->allow_credentials[0].transports = {transport};

  GetAssertionResult get_result =
      AuthenticatorGetAssertion(std::move(get_options));
  EXPECT_EQ(get_result.status, AuthenticatorStatus::SUCCESS);
}

// LINT.IfChange(VirtualAuthenticatorTransports)
INSTANTIATE_TEST_SUITE_P(
    All,
    AuthenticatorImplVirtualAuthenticatorTransportTest,
    ::testing::Values(device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
                      device::FidoTransportProtocol::kNearFieldCommunication,
                      device::FidoTransportProtocol::kBluetoothLowEnergy,
                      device::FidoTransportProtocol::kHybrid,
                      device::FidoTransportProtocol::kInternal,
                      device::FidoTransportProtocol::kSmartCard));
// LINT.ThenChange(//device/fido/public/fido_transport_protocol.h:FidoTransportProtocol)

}  // namespace content
