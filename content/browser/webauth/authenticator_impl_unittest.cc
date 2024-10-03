// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/browser/webauth/authenticator_impl.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/stack_allocated.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/ranges/algorithm.h"
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
#include "build/chromeos_buildflags.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/webauthn/content/browser/internal_authenticator_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/webauth/authenticator_common_impl.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/client_data_json.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_authenticator_manager_impl.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "crypto/sha2.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/fido_tunnel_device.h"
#include "device/fido/cable/v2_authenticator.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_discovery.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/v2_test_util.h"
#include "device/fido/discoverable_credential_metadata.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/filter.h"
#include "device/fido/large_blob.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/multiple_virtual_fido_device_factory.h"
#include "device/fido/pin.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/functions.h"
#include "services/data_decoder/gzipper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "url/origin.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/credential_store.h"
#include "device/fido/mac/icloud_keychain.h"
#include "device/fido/mac/scoped_icloud_keychain_test_environment.h"
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_scale_factor.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "content/public/test/test_browser_context.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/util.h"
#include "third_party/microsoft_webauthn/webauthn.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#endif

namespace content {

using ::testing::_;

using GetAssertionOutcome = AuthenticatorCommonImpl::GetAssertionOutcome;
using MakeCredentialOutcome = AuthenticatorCommonImpl::MakeCredentialOutcome;
using RequestMode = AuthenticatorCommonImpl::RequestMode;

using blink::mojom::AttestationConveyancePreference;
using blink::mojom::AuthenticationExtensionsClientInputs;
using blink::mojom::AuthenticationExtensionsClientOutputs;
using blink::mojom::AuthenticatorSelectionCriteria;
using blink::mojom::AuthenticatorSelectionCriteriaPtr;
using blink::mojom::AuthenticatorStatus;
using blink::mojom::AuthenticatorTransport;
using blink::mojom::CableAuthentication;
using blink::mojom::CableAuthenticationPtr;
using blink::mojom::CommonCredentialInfo;
using blink::mojom::GetAssertionAuthenticatorResponse;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
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
using device::cablev2::Event;

namespace {

using InterestingFailureReason =
    AuthenticatorRequestClientDelegate::InterestingFailureReason;
using FailureReasonFuture = base::test::TestFuture<InterestingFailureReason>;

constexpr base::TimeDelta kTestTimeout = base::Minutes(1);

// The size of credential IDs returned by GetTestCredentials().
constexpr size_t kTestCredentialIdLength = 32u;

constexpr char kTestOrigin1[] = "https://a.google.com";
constexpr char kTestOrigin2[] = "https://acme.org";
constexpr char kTestRelyingPartyId[] = "google.com";
constexpr char kDifferentTestRelyingPartyId[] = "different-rp.com";
constexpr char kExtensionScheme[] = "chrome-extension";
static constexpr char kCorpCrdOrigin[] =
    "https://remotedesktop.corp.google.com";

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
    {"ws://google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},
    {"gopher://google.com", "google.com", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ftp://google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},
    {"file:///google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},
    // Use of webauthn from a WSS origin may be technically valid, but we
    // prohibit use on non-HTTPS origins. (At least for now.)
    {"wss://google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},

    {"data:,", "", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"https://google.com", "", AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"ws:///google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},
    {"wss:///google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},
    {"gopher://google.com", "", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ftp://google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},
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

using TestGetClientCapabilityFuture = base::test::TestFuture<
    std::vector<blink::mojom::WebAuthnClientCapabilityPtr>>;
using TestIsUvpaaFuture = base::test::TestFuture<bool>;
using TestMakeCredentialFuture =
    base::test::TestFuture<AuthenticatorStatus,
                           MakeCredentialAuthenticatorResponsePtr,
                           WebAuthnDOMExceptionDetailsPtr>;
using TestGetAssertionFuture =
    base::test::TestFuture<AuthenticatorStatus,
                           GetAssertionAuthenticatorResponsePtr,
                           WebAuthnDOMExceptionDetailsPtr>;
using TestRequestStartedFuture = base::test::TestFuture<void>;
using TestReportFuture =
    base::test::TestFuture<AuthenticatorStatus, WebAuthnDOMExceptionDetailsPtr>;

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
  options->timeout = base::Minutes(1);
  options->authenticator_selection = GetTestAuthenticatorSelectionCriteria();
  return options;
}

PublicKeyCredentialRequestOptionsPtr
GetTestPublicKeyCredentialRequestOptions() {
  auto options = PublicKeyCredentialRequestOptions::New();
  options->extensions = AuthenticationExtensionsClientInputs::New();
  options->relying_party_id = std::string(kTestRelyingPartyId);
  options->challenge.assign(32, 0x0A);
  options->timeout = base::Minutes(1);
  options->user_verification = device::UserVerificationRequirement::kPreferred;
  options->allow_credentials = GetTestCredentials();
  return options;
}

PublicKeyCredentialReportOptionsPtr GetTestPublicKeyCredentialReportOptions() {
  auto options = PublicKeyCredentialReportOptions::New();
  options->relying_party_id = std::string(kTestRelyingPartyId);
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
  std::optional<Value> attestation_value =
      Reader::Read(response->attestation_object);
  CHECK(attestation_value);
  const auto& attestation = attestation_value->GetMap();

  const auto auth_data_it = attestation.find(Value(device::kAuthDataKey));
  CHECK(auth_data_it != attestation.end());
  const std::vector<uint8_t>& auth_data = auth_data_it->second.GetBytestring();
  std::optional<device::AuthenticatorData> parsed_auth_data =
      device::AuthenticatorData::DecodeAuthenticatorData(auth_data);
  return std::move(parsed_auth_data.value());
}

bool HasUV(const MakeCredentialAuthenticatorResponsePtr& response) {
  return AuthDataFromMakeCredentialResponse(response)
      .obtained_user_verification();
}

bool HasUV(const GetAssertionAuthenticatorResponsePtr& response) {
  std::optional<device::AuthenticatorData> auth_data =
      device::AuthenticatorData::DecodeAuthenticatorData(
          response->info->authenticator_data);
  return auth_data->obtained_user_verification();
}

url::Origin GetTestOrigin() {
  const GURL test_relying_party_url(kTestOrigin1);
  CHECK(test_relying_party_url.is_valid());
  return url::Origin::Create(test_relying_party_url);
}

std::string GetTestClientDataJSON(ClientDataRequestType type) {
  return BuildClientDataJson({std::move(type), GetTestOrigin(), GetTestOrigin(),
                              GetTestChallengeBytes(),
                              /*is_cross_origin_iframe=*/false});
}

device::LargeBlob CompressLargeBlob(base::span<const uint8_t> blob) {
  data_decoder::Gzipper gzipper;
  std::vector<uint8_t> compressed;
  base::RunLoop run_loop;
  gzipper.Deflate(
      blob, base::BindLambdaForTesting(
                [&](std::optional<mojo_base::BigBuffer> result) {
                  compressed = device::fido_parsing_utils::Materialize(*result);
                  run_loop.Quit();
                }));
  run_loop.Run();
  return device::LargeBlob(std::move(compressed), blob.size());
}

std::vector<uint8_t> UncompressLargeBlob(device::LargeBlob blob) {
  data_decoder::Gzipper gzipper;
  std::vector<uint8_t> uncompressed;
  base::RunLoop run_loop;
  gzipper.Inflate(
      blob.compressed_data, blob.original_size,
      base::BindLambdaForTesting(
          [&](std::optional<mojo_base::BigBuffer> result) {
            if (result) {
              uncompressed = device::fido_parsing_utils::Materialize(*result);
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

std::array<uint8_t, crypto::kSHA256Length> EvaluateHMAC(
    base::span<const uint8_t> key,
    base::span<const uint8_t> salt) {
  std::array<uint8_t, crypto::kSHA256Length> ret;
  unsigned hmac_out_length;
  HMAC(EVP_sha256(), key.data(), key.size(), salt.data(), salt.size(),
       ret.data(), &hmac_out_length);
  CHECK_EQ(hmac_out_length, ret.size());
  return ret;
}

}  // namespace

class AuthenticatorTestBase : public RenderViewHostTestHarness {
 protected:
  AuthenticatorTestBase()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AuthenticatorTestBase() override = default;

  static void SetUpTestSuite() {
#if BUILDFLAG(IS_MAC)
    // Load fido_strings, which can be required for exercising the Touch ID
    // authenticator.
    base::FilePath path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &path));
    base::FilePath fido_test_strings =
        path.Append(FILE_PATH_LITERAL("fido_test_strings.pak"));
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        fido_test_strings, ui::kScaleFactorNone);
#endif
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

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

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();

    mojo::SetDefaultProcessErrorHandler(base::NullCallback());

    virtual_device_factory_ = nullptr;
    AuthenticatorEnvironment::GetInstance()->Reset();
#if BUILDFLAG(IS_CHROMEOS)
    chromeos::U2FClient::Shutdown();
    chromeos::TpmManagerClient::Shutdown();
#endif
  }

  virtual void ResetVirtualDevice() {
    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory_ = virtual_device_factory.get();
    AuthenticatorEnvironment::GetInstance()
        ->ReplaceDefaultDiscoveryFactoryForTesting(
            std::move(virtual_device_factory));
  }

  virtual void ReplaceDiscoveryFactory(
      std::unique_ptr<device::FidoDiscoveryFactory> device_factory) {
    virtual_device_factory_ = nullptr;
    AuthenticatorEnvironment::GetInstance()
        ->ReplaceDefaultDiscoveryFactoryForTesting(std::move(device_factory));
  }

  void SetMojoErrorHandler(
      base::RepeatingCallback<void(const std::string&)> callback) {
    mojo_error_handler_ = callback;
  }

  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_ =
      nullptr;
#if BUILDFLAG(IS_WIN)
  device::FakeWinWebAuthnApi fake_win_webauthn_api_;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override_{
      &fake_win_webauthn_api_};
  std::unique_ptr<device::fido::win::ScopedBiometricsOverride>
      biometrics_override_;
#endif

 private:
  void OnMojoError(const std::string& error) {
    if (mojo_error_handler_) {
      mojo_error_handler_.Run(error);
      return;
    }
    FAIL() << "Unhandled mojo error: " << error;
  }

  base::RepeatingCallback<void(const std::string&)> mojo_error_handler_;
};

class AuthenticatorImplTest : public AuthenticatorTestBase {
 protected:
  AuthenticatorImplTest() {
    url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  }
  ~AuthenticatorImplTest() override = default;

  void SetUp() override {
    AuthenticatorTestBase::SetUp();
    SetBluetoothLESupported(true);
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  void SetBluetoothLESupported(bool supported) {
    bluetooth_global_values_->SetLESupported(supported);
  }

  void NavigateAndCommit(const GURL& url) {
    RenderViewHostTestHarness::NavigateAndCommit(url);
  }

  mojo::Remote<blink::mojom::Authenticator> ConnectToAuthenticator() {
    mojo::Remote<blink::mojom::Authenticator> authenticator;
    static_cast<RenderFrameHostImpl*>(main_rfh())
        ->GetWebAuthenticationService(
            authenticator.BindNewPipeAndPassReceiver());
    return authenticator;
  }

  bool AuthenticatorIsUvpaa() {
    TestIsUvpaaFuture future;
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(
        future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
  }

  using ClientCapabilitiesList =
      std::vector<blink::mojom::WebAuthnClientCapabilityPtr>;

  ClientCapabilitiesList AuthenticatorGetClientCapabilities() {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestGetClientCapabilityFuture future;
    authenticator->GetClientCapabilities(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Take();
  }

  void ExpectCapability(
      const std::vector<blink::mojom::WebAuthnClientCapabilityPtr>&
          capabilities,
      std::string_view capability_name,
      bool supported) {
    auto capability_it =
        std::find_if(capabilities.begin(), capabilities.end(),
                     [&capability_name](const auto& capability) {
                       return capability->name == capability_name;
                     });

    ASSERT_NE(capability_it, capabilities.end());
    EXPECT_EQ(supported, (*capability_it)->supported);
  }

  bool AuthenticatorIsConditionalMediationAvailable() {
    TestIsUvpaaFuture future;
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    authenticator->IsConditionalMediationAvailable(future.GetCallback());
    EXPECT_TRUE(future.Wait());
    return future.Get();
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
    TestMakeCredentialFuture future;
    authenticator->MakeCredential(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    auto [status, response, dom_exception] = future.Take();
    return {status, std::move(response)};
  }

  MakeCredentialResult AuthenticatorMakeCredentialAndWaitForTimeout(
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
    TestGetAssertionFuture future;
    authenticator->GetAssertion(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    auto [status, response, dom_exception] = future.Take();
    return {status, std::move(response)};
  }

  GetAssertionResult AuthenticatorGetAssertionAndWaitForTimeout(
      PublicKeyCredentialRequestOptionsPtr options) {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestGetAssertionFuture future;
    authenticator->GetAssertion(std::move(options), future.GetCallback());
    task_environment()->FastForwardBy(kTestTimeout);
    auto [status, response, dom_exception] = future.Take();
    return {status, std::move(response)};
  }

  AuthenticatorStatus AuthenticatorReport(
      PublicKeyCredentialReportOptionsPtr options) {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestReportFuture future;
    authenticator->Report(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    auto [status, dom_exception] = future.Take();
    return status;
  }

  AuthenticatorStatus TryAuthenticationWithAppId(const std::string& origin,
                                                 const std::string& appid) {
    const GURL origin_url(origin);
    NavigateAndCommit(origin_url);

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = origin_url.host();
    options->extensions->appid = appid;

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

  ukm::TestUkmRecorder* GetTestUkmRecorder() { return &test_ukm_recorder_; }

  void VerifyGetAssertionOutcomeUkm(uint32_t index,
                                    GetAssertionOutcome outcome,
                                    RequestMode mode) {
    auto entries = GetTestUkmRecorder()->GetEntriesByName(
        ukm::builders::WebAuthn_SignCompletion::kEntryName);
    ASSERT_GT(entries.size(), index);
    GetTestUkmRecorder()->ExpectEntryMetric(
        entries[index], "SignCompletionResult", static_cast<int64_t>(outcome));
    GetTestUkmRecorder()->ExpectEntryMetric(entries[index], "RequestMode",
                                            static_cast<int64_t>(mode));
  }

  void VerifyMakeCredentialOutcomeUkm(uint32_t index,
                                      MakeCredentialOutcome outcome,
                                      RequestMode mode) {
    auto entries = GetTestUkmRecorder()->GetEntriesByName(
        ukm::builders::WebAuthn_RegisterCompletion::kEntryName);
    ASSERT_GT(entries.size(), index);
    GetTestUkmRecorder()->ExpectEntryMetric(entries[index],
                                            "RegisterCompletionResult",
                                            static_cast<int64_t>(outcome));
    GetTestUkmRecorder()->ExpectEntryMetric(entries[index], "RequestMode",
                                            static_cast<int64_t>(mode));
  }

  scoped_refptr<::testing::NiceMock<device::MockBluetoothAdapter>>
      mock_adapter_ = base::MakeRefCounted<
          ::testing::NiceMock<device::MockBluetoothAdapter>>();

 private:
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalOverrideValues>
      bluetooth_global_values_ =
          device::BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  data_decoder::test::InProcessDataDecoder data_decoder_service_;
  url::ScopedSchemeRegistryForTests scoped_registry_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

TEST_F(AuthenticatorImplTest, ClientDataJSONSerialization) {
  // First test that the output is in the expected form. Some verifiers may be
  // depending on the exact JSON serialisation. Since the serialisation may add
  // extra elements, this can only test that the expected value is a prefix of
  // the returned value.
  std::vector<uint8_t> challenge_bytes = {1, 2, 3};
  EXPECT_EQ(
      BuildClientDataJson({ClientDataRequestType::kWebAuthnCreate,
                           GetTestOrigin(), GetTestOrigin(), challenge_bytes,
                           false})
          .find(
              "{\"type\":\"webauthn.create\",\"challenge\":\"AQID\",\"origin\":"
              "\"https://a.google.com\",\"crossOrigin\":false"),
      0u);

  // Second, check that a generic JSON parser correctly parses the result.
  static const struct {
    const ClientDataRequestType type;
    url::Origin origin;
    url::Origin top_origin;
    std::vector<uint8_t> challenge;
    bool is_cross_origin;
  } kTestCases[] = {
      {
          ClientDataRequestType::kWebAuthnGet,
          GetTestOrigin(),
          GetTestOrigin(),
          {1, 2, 3},
          false,
      },
      {
          ClientDataRequestType::kPaymentGet,
          GetTestOrigin(),
          GetTestOrigin(),
          {1, 2, 3},
          false,
      },
      {
          ClientDataRequestType::kWebAuthnCreate,
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

    const auto parsed = base::JSONReader::Read(json);
    ASSERT_TRUE(parsed.has_value());
    std::string type_key;
    std::string expected_type;
    switch (test.type) {
      case ClientDataRequestType::kWebAuthnCreate:
        type_key = "type";
        expected_type = "webauthn.create";
        break;
      case ClientDataRequestType::kWebAuthnGet:
        type_key = "type";
        expected_type = "webauthn.get";
        break;
      case ClientDataRequestType::kPaymentGet:
        type_key = "type";
        expected_type = "payment.get";
        break;
    }
    ASSERT_TRUE(parsed->is_dict());
    EXPECT_EQ(*parsed->GetDict().FindString(type_key), expected_type);
    EXPECT_EQ(*parsed->GetDict().FindString("origin"), test.origin.Serialize());
    std::string expected_challenge;
    base::Base64UrlEncode(
        std::string_view(reinterpret_cast<const char*>(test.challenge.data()),
                         test.challenge.size()),
        base::Base64UrlEncodePolicy::OMIT_PADDING, &expected_challenge);
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
  std::vector<OriginClaimedAuthorityPair> tests(
      &kValidRelyingPartyTestCases[0],
      &kValidRelyingPartyTestCases[std::size(kValidRelyingPartyTestCases)]);
  tests.insert(
      tests.end(), &kInvalidRelyingPartyTestCases[0],
      &kInvalidRelyingPartyTestCases[std::size(kInvalidRelyingPartyTestCases)]);

  int test_case_count = 0;
  for (const auto& test_case : tests) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

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
        RequestMode::kModalWebAuthn);
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
                                 RequestMode::kModalWebAuthn);
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
                                 RequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorImplTest, GetClientCapabilities) {
  NavigateAndCommit(GURL(kTestOrigin1));

  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();

  std::vector<std::string> capability_names;
  base::ranges::transform(
      capabilities, std::back_inserter(capability_names),
      [](const auto& capability) { return capability->name; });

  const std::vector<std::string_view> kRequiredCapabilities = {
      client_capabilities::kConditionalGet,
      client_capabilities::kHybridTransport,
      client_capabilities::kPasskeyPlatformAuthenticator,
      client_capabilities::kUserVerifyingPlatformAuthenticator,
      client_capabilities::kRelatedOrigins,
  };

  // Ensure no extra capabilities
  EXPECT_EQ(kRequiredCapabilities.size(), capabilities.size());

  // Check that each required capability is present exactly once.
  for (const auto& capability : kRequiredCapabilities) {
    EXPECT_EQ(1u, static_cast<size_t>(
                      base::ranges::count(capability_names, capability)));
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
       GetClientCapabilities_HybridTransport_BluetoothDisabled) {
  blink::ParsedPermissionsPolicy permissions_policy(1);
  permissions_policy[0].feature =
      blink::mojom::PermissionsPolicyFeature::kBluetooth;
  // Simulate navigating to a page with this Permissions Policy.
  auto navigation_simulator = NavigationSimulator::CreateRendererInitiated(
      GURL(kTestOrigin1), main_rfh());
  navigation_simulator->SetPermissionsPolicyHeader(permissions_policy);
  navigation_simulator->Commit();

  EXPECT_CALL(*mock_adapter_, IsPresent()).Times(0);
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

// Parses its arguments as JSON and expects that all the keys in the first are
// also in the second, and with the same value.
static void CheckJSONIsSubsetOfJSON(std::string_view subset_str,
                                    std::string_view test_str) {
  std::optional<base::Value> subset = base::JSONReader::Read(subset_str);
  ASSERT_TRUE(subset);
  ASSERT_TRUE(subset->is_dict());
  const base::Value::Dict& subset_dict = subset->GetDict();
  std::optional<base::Value> test = base::JSONReader::Read(test_str);
  ASSERT_TRUE(test);
  ASSERT_TRUE(test->is_dict());
  const base::Value::Dict& test_dict = test->GetDict();

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
      GetTestClientDataJSON(ClientDataRequestType::kWebAuthnCreate));
}

TEST(ClientDataSerializationTest, Sign) {
  CheckJSONIsSubsetOfJSON(
      kTestSignClientDataJsonString,
      GetTestClientDataJSON(ClientDataRequestType::kWebAuthnGet));
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
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.MakeCredential.Result",
      AuthenticatorCommonImpl::CredentialRequestResult::kTimeout, 1);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kUiTimeout,
                                 RequestMode::kModalWebAuthn);
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

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, ReportOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (const OriginClaimedAuthorityPair& test_case :
       kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = test_case.claimed_authority;
    options->unknown_credential_id = std::vector<uint8_t>(32, 0x0A);

    EXPECT_EQ(AuthenticatorReport(std::move(options)),
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
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.GetAssertion.Result",
      AuthenticatorCommonImpl::CredentialRequestResult::kTimeout, 1);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kUiTimeout,
                               RequestMode::kModalWebAuthn);
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
    options->extensions->cable_authentication_data = GetTestCableExtension();

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
                               RequestMode::kModalWebAuthn);
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
                                 RequestMode::kModalWebAuthn);
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
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionFuture future;
  authenticator->GetAssertion(std::move(options), future.GetCallback());

  // Make second request.
  // TODO(crbug.com/41355992): Rework to ensure there are potential race
  // conditions once we have VirtualAuthenticatorEnvironment.
  PublicKeyCredentialRequestOptionsPtr options2 =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionFuture future2;
  authenticator->GetAssertion(std::move(options2), future2.GetCallback());
  EXPECT_TRUE(future2.Wait());

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, std::get<0>(future2.Get()));

  EXPECT_TRUE(future.Wait());
}

TEST_F(AuthenticatorImplTest, ReportPendingRequest) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // Make first request.
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionFuture future;
  authenticator->GetAssertion(std::move(options), future.GetCallback());

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
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionFuture future;
  authenticator->GetAssertion(std::move(options), future.GetCallback());

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
                                 RequestMode::kModalWebAuthn);
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

// TestWebAuthenticationRequestProxy is a test fake implementation of the
// WebAuthenticationRequestProxy embedder interface.
class TestWebAuthenticationRequestProxy : public WebAuthenticationRequestProxy {
 public:
  struct Config {
    // If true, resolves all request event callbacks instantly.
    bool resolve_callbacks = true;

    // The return value of IsActive().
    bool is_active = true;

    // The fake response to SignalIsUVPAARequest().
    bool is_uvpaa = true;

    // Whether the request to SignalCreateRequest() should succeed.
    bool request_success = true;

    // If `request_success` is false, the name of the DOMError to be
    // returned.
    std::string request_error_name = "NotAllowedError";

    // If `request_success` is true, the fake response to be returned for an
    // onCreateRequest event.
    blink::mojom::MakeCredentialAuthenticatorResponsePtr
        make_credential_response = nullptr;

    // If `request_success` is true, the fake response to be returned for an
    // onGetRequest event.
    blink::mojom::GetAssertionAuthenticatorResponsePtr get_assertion_response =
        nullptr;
  };

  struct Observations {
    std::vector<PublicKeyCredentialCreationOptionsPtr> create_requests;
    std::vector<PublicKeyCredentialRequestOptionsPtr> get_requests;
    size_t num_isuvpaa;
    size_t num_cancel;
  };

  ~TestWebAuthenticationRequestProxy() override {
    DCHECK(!HasPendingRequest());
  }

  Config& config() { return config_; }

  Observations& observations() { return observations_; }

  bool IsActive(const url::Origin& caller_origin) override {
    return config_.is_active;
  }

  RequestId SignalCreateRequest(
      const PublicKeyCredentialCreationOptionsPtr& options,
      CreateCallback callback) override {
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

  RequestId SignalGetRequest(
      const PublicKeyCredentialRequestOptionsPtr& options,
      GetCallback callback) override {
    current_request_id_++;
    observations_.get_requests.push_back(options->Clone());
    pending_get_callback_ = std::move(callback);
    if (config_.resolve_callbacks) {
      RunPendingGetCallback();
      return current_request_id_;
    }
    return current_request_id_;
  }

  RequestId SignalIsUvpaaRequest(IsUvpaaCallback callback) override {
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

  void CancelRequest(RequestId request_id) override {
    DCHECK_EQ(request_id, current_request_id_);
    observations_.num_cancel++;
    if (pending_create_callback_) {
      pending_create_callback_.Reset();
    }
    if (pending_get_callback_) {
      pending_get_callback_.Reset();
    }
  }

  void RunPendingCreateCallback() {
    DCHECK(pending_create_callback_);
    auto callback =
        config_.request_success
            ? base::BindOnce(std::move(pending_create_callback_),
                             current_request_id_, nullptr,
                             config_.make_credential_response.Clone())
            : base::BindOnce(std::move(pending_create_callback_),
                             current_request_id_,
                             WebAuthnDOMExceptionDetails::New(
                                 config_.request_error_name, "message"),
                             nullptr);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

  void RunPendingGetCallback() {
    DCHECK(pending_get_callback_);
    auto callback =
        config_.request_success
            ? base::BindOnce(std::move(pending_get_callback_),
                             current_request_id_, nullptr,
                             config_.get_assertion_response.Clone())
            : base::BindOnce(std::move(pending_create_callback_),
                             current_request_id_,
                             WebAuthnDOMExceptionDetails::New(
                                 config_.request_error_name, "message"),
                             nullptr);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

  void RunPendingIsUvpaaCallback() {
    DCHECK(pending_is_uvpaa_callback_);
    std::move(pending_is_uvpaa_callback_).Run(config_.is_uvpaa);
  }

  bool HasPendingRequest() {
    return pending_create_callback_ || pending_get_callback_ ||
           pending_is_uvpaa_callback_;
  }

 private:
  Config config_;
  Observations observations_;

  RequestId current_request_id_ = 0;
  CreateCallback pending_create_callback_;
  GetCallback pending_get_callback_;
  IsUvpaaCallback pending_is_uvpaa_callback_;
};

// TestWebAuthenticationDelegate is a test fake implementation of the
// WebAuthenticationDelegate embedder interface.
class TestWebAuthenticationDelegate : public WebAuthenticationDelegate {
 public:
  void IsUserVerifyingPlatformAuthenticatorAvailableOverride(
      RenderFrameHost*,
      base::OnceCallback<void(std::optional<bool>)> callback) override {
    std::move(callback).Run(is_uvpaa_override);
  }

  bool OverrideCallerOriginAndRelyingPartyIdValidation(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      const std::string& rp_id) override {
    return permit_extensions && origin.scheme() == kExtensionScheme &&
           origin.host() == rp_id;
  }

  std::optional<std::string> MaybeGetRelyingPartyIdOverride(
      const std::string& claimed_rp_id,
      const url::Origin& caller_origin) override {
    if (permit_extensions && caller_origin.scheme() == kExtensionScheme) {
      return caller_origin.Serialize();
    }
    return std::nullopt;
  }

  bool ShouldPermitIndividualAttestation(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id) override {
    return permit_individual_attestation ||
           (permit_individual_attestation_for_rp_id.has_value() &&
            relying_party_id == *permit_individual_attestation_for_rp_id);
  }

  bool SupportsResidentKeys(RenderFrameHost*) override {
    return supports_resident_keys;
  }

  bool SupportsPasskeyMetadataSyncing() override {
    return supports_passkey_metadata_syncing;
  }

  bool IsFocused(WebContents* web_contents) override { return is_focused; }

#if BUILDFLAG(IS_MAC)
  std::optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig(
      BrowserContext* browser_context) override {
    return touch_id_authenticator_config;
  }
#endif

  WebAuthenticationRequestProxy* MaybeGetRequestProxy(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) override {
    return request_proxy && request_proxy->IsActive(caller_origin)
               ? request_proxy.get()
               : nullptr;
  }

  bool OriginMayUseRemoteDesktopClientOverride(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) override {
    return caller_origin == remote_desktop_client_override_origin;
  }

  // If set, the return value of IsUVPAA() will be overridden with this value.
  // Platform-specific implementations will not be invoked.
  std::optional<bool> is_uvpaa_override;

  // If set, the delegate will permit WebAuthn requests from chrome-extension
  // origins.
  bool permit_extensions = false;

  // Indicates whether individual attestation should be permitted by the
  // delegate.
  bool permit_individual_attestation = false;

  // A specific RP ID for which individual attestation will be permitted.
  std::optional<std::string> permit_individual_attestation_for_rp_id;

  // Indicates whether resident key operations should be permitted by the
  // delegate.
  bool supports_resident_keys = false;

  // Indicates whether metadata syncing should be assumed to be supported.
  bool supports_passkey_metadata_syncing = false;

  // The return value of the focus check issued at the end of a request.
  bool is_focused = true;

#if BUILDFLAG(IS_MAC)
  // Configuration data for the macOS platform authenticator.
  std::optional<TouchIdAuthenticatorConfig> touch_id_authenticator_config;
#endif

  // The WebAuthenticationRequestProxy returned by |MaybeGetRequestProxy|.
  std::unique_ptr<TestWebAuthenticationRequestProxy> request_proxy = nullptr;

  // The origin permitted to use the RemoteDesktopClientOverride extension.
  std::optional<url::Origin> remote_desktop_client_override_origin;
};

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
      NOTREACHED_IN_MIGRATION();
      return "";
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

// TestAuthenticatorRequestDelegate is a test fake implementation of the
// AuthenticatorRequestClientDelegate embedder interface.
class TestAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  TestAuthenticatorRequestDelegate(
      RenderFrameHost* render_frame_host,
      base::OnceClosure action_callbacks_registered_callback,
      base::OnceClosure started_over_callback,
      bool simulate_user_cancelled)
      : action_callbacks_registered_callback_(
            std::move(action_callbacks_registered_callback)),
        started_over_callback_(std::move(started_over_callback)),
        does_block_request_on_failure_(!started_over_callback_.is_null()),
        simulate_user_cancelled_(simulate_user_cancelled) {}

  TestAuthenticatorRequestDelegate(const TestAuthenticatorRequestDelegate&) =
      delete;
  TestAuthenticatorRequestDelegate& operator=(
      const TestAuthenticatorRequestDelegate&) = delete;

  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback,
      base::RepeatingCallback<
          void(device::FidoRequestHandlerBase::BlePermissionCallback)>
          ble_status_callback) override {
    ASSERT_TRUE(action_callbacks_registered_callback_)
        << "RegisterActionCallbacks called twice.";
    cancel_callback_ = std::move(cancel_callback);
    std::move(action_callbacks_registered_callback_).Run();
    if (started_over_callback_) {
      action_callbacks_registered_callback_ = std::move(started_over_callback_);
      start_over_callback_ = start_over_callback;
    }
  }

  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo transport_info)
      override {
    // Simulate the behaviour of Chrome's |AuthenticatorRequestDialogModel|
    // which shows a specific error when no transports are available and lets
    // the user cancel the request.
    if (transport_info.available_transports.empty() ||
        simulate_user_cancelled_) {
      std::move(cancel_callback_).Run();
    }
  }

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override {
    if (!does_block_request_on_failure_) {
      return false;
    }

    std::move(start_over_callback_).Run();
    does_block_request_on_failure_ = false;
    return true;
  }

  base::OnceClosure action_callbacks_registered_callback_;
  base::OnceClosure cancel_callback_;
  base::OnceClosure started_over_callback_;
  base::OnceClosure start_over_callback_;
  bool does_block_request_on_failure_ = false;
  bool simulate_user_cancelled_ = false;
};

// TestAuthenticatorContentBrowserClient is a test fake implementation of the
// ContentBrowserClient interface that injects |TestWebAuthenticationDelegate|
// and |TestAuthenticatorRequestDelegate| instances into |AuthenticatorImpl|.
class TestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  TestWebAuthenticationDelegate* GetTestWebAuthenticationDelegate() {
    return &web_authentication_delegate;
  }

  // ContentBrowserClient:
  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate;
  }

  bool IsSecurityLevelAcceptableForWebAuthn(
      content::RenderFrameHost* rfh,
      const url::Origin& origin) override {
    return is_webauthn_security_level_acceptable;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    if (return_null_delegate) {
      return nullptr;
    }
    return std::make_unique<TestAuthenticatorRequestDelegate>(
        render_frame_host,
        action_callbacks_registered_callback
            ? std::move(action_callbacks_registered_callback)
            : base::DoNothing(),
        std::move(started_over_callback_), simulate_user_cancelled_);
  }

  TestWebAuthenticationDelegate web_authentication_delegate;

  // If set, this closure will be called when the subsequently constructed
  // delegate is informed that the request has started.
  base::OnceClosure action_callbacks_registered_callback;

  // This emulates scenarios where a nullptr RequestClientDelegate is returned
  // because a request is already in progress.
  bool return_null_delegate = false;

  // If started_over_callback_ is set to a non-null callback, the request will
  // be restarted after action callbacks are registered, and
  // |started_over_callback| will replace
  // |action_callbacks_registered_callback|. This should then be called the
  // second time action callbacks are registered. It also causes
  // DoesBlockRequestOnFailure to return true, once.
  base::OnceClosure started_over_callback_;

  // This simulates the user immediately cancelling the request after transport
  // availability info is enumerated.
  bool simulate_user_cancelled_ = false;

  // The return value of IsSecurityLevelAcceptableForWebAuthn.
  bool is_webauthn_security_level_acceptable = true;
};

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
    std::string_view cert = x5c[0].GetBytestringAsString();
    EXPECT_TRUE(cert.find(substring) != cert.npos);
  }

  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

TEST_F(AuthenticatorContentBrowserClientTest, MakeCredentialTLSError) {
  NavigateAndCommit(GURL(kTestOrigin1));
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::CERTIFICATE_ERROR);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kOtherFailure,
                                 RequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorContentBrowserClientTest, GetAssertionTLSError) {
  NavigateAndCommit(GURL(kTestOrigin1));
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::CERTIFICATE_ERROR);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kOtherFailure,
                               RequestMode::kModalWebAuthn);
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

TEST_F(AuthenticatorContentBrowserClientTest, TestGetAssertionCancel) {
  NavigateAndCommit(GURL(kTestOrigin1));
  test_client_.simulate_user_cancelled_ = true;
  base::HistogramTester histogram_tester;

  EXPECT_EQ(AuthenticatorGetAssertion().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.GetAssertion.Result",
      AuthenticatorCommonImpl::CredentialRequestResult::kUserCancelled, 1);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kUserCancellation,
                               RequestMode::kModalWebAuthn);
}

TEST_F(AuthenticatorContentBrowserClientTest, TestMakeCredentialCancel) {
  NavigateAndCommit(GURL(kTestOrigin1));
  test_client_.simulate_user_cancelled_ = true;
  base::HistogramTester histogram_tester;

  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.MakeCredential.Result",
      AuthenticatorCommonImpl::CredentialRequestResult::kUserCancelled, 1);
  VerifyMakeCredentialOutcomeUkm(0, MakeCredentialOutcome::kUserCancellation,
                                 RequestMode::kModalWebAuthn);
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

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();

  TestRequestStartedFuture request_started_future;
  test_client_.action_callbacks_registered_callback =
      request_started_future.GetCallback();
  authenticator->GetAssertion(std::move(options), base::DoNothing());
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

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();

  TestRequestStartedFuture request_started_future;
  test_client_.action_callbacks_registered_callback =
      request_started_future.GetCallback();
  TestRequestStartedFuture request_restarted_future;
  test_client_.started_over_callback_ = request_restarted_future.GetCallback();

  authenticator->GetAssertion(std::move(options), base::DoNothing());
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
  // Conditional mediation should always be available if gpm passkeys are
  // enabled.
  test_client_.GetTestWebAuthenticationDelegate()
      ->supports_passkey_metadata_syncing = true;
  NavigateAndCommit(GURL(kTestOrigin1));

  ClientCapabilitiesList capabilities = AuthenticatorGetClientCapabilities();
  ExpectCapability(capabilities, client_capabilities::kConditionalGet, true);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       GPMPasskeys_IsConditionalMediationAvailable) {
  // Conditional mediation should always be available if gpm passkeys are
  // enabled.
  test_client_.GetTestWebAuthenticationDelegate()
      ->supports_passkey_metadata_syncing = true;
  NavigateAndCommit(GURL(kTestOrigin1));
  ASSERT_TRUE(AuthenticatorIsConditionalMediationAvailable());
}

// AuthenticatorImplRemoteDesktopClientOverrideTest exercises the
// RemoteDesktopClientOverride extension, which is used by remote desktop
// applications exercising requests on behalf of other origins.
class AuthenticatorImplRemoteDesktopClientOverrideTest
    : public AuthenticatorContentBrowserClientTest {
 protected:
  static constexpr char kOtherRdpOrigin[] = "https://myrdp.test";
  static constexpr char kExampleOrigin[] = "https://example.test";
  static constexpr char kExampleRpId[] = "example.test";
  static constexpr char kExampleAppid[] = "https://example.test/appid.json";
  static constexpr char kOtherRpId[] = "other.test";
  static constexpr char kOtherAppid[] = "https://other.test/appid.json";

  void SetUp() override {
    AuthenticatorContentBrowserClientTest::SetUp();
    // Authorize `kCorpCrdOrigin` to exercise the extension. In //chrome, this
    // is controlled by the `WebAuthenticationRemoteProxiedRequestsAllowed`
    // enterprise policy.
    test_client_.GetTestWebAuthenticationDelegate()
        ->remote_desktop_client_override_origin =
        url::Origin::Create(GURL(kCorpCrdOrigin));
    // Controls the Blink feature gating the extension.
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kWebAuthRemoteDesktopSupport);
  }

  base::test::ScopedCommandLine scoped_command_line_;
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnGoogleCorpRemoteDesktopClientPrivilege};
};

TEST_F(AuthenticatorImplRemoteDesktopClientOverrideTest, MakeCredential) {
  // Verify that an authorized origin may use the extension. Regular RP ID
  // processing applies, i.e. the origin override must be authorized to claim
  // the specified RP ID.
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
    options->remote_desktop_client_override = RemoteDesktopClientOverride::New(
        url::Origin::Create(GURL(test.remote_origin)), true);

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              test.success ? AuthenticatorStatus::SUCCESS
                           : AuthenticatorStatus::
                                 REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED);
  }
}

TEST_F(AuthenticatorImplRemoteDesktopClientOverrideTest, GetAssertion) {
  // Verify that an authorized origin may use the extension. Regular RP ID
  // processing applies, i.e. the origin override must be authorized to claim
  // the specified RP ID.
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
    options->extensions->remote_desktop_client_override =
        RemoteDesktopClientOverride::New(
            url::Origin::Create(GURL(test.remote_origin)), true);

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id, test.rp_id));

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              test.success ? AuthenticatorStatus::SUCCESS
                           : AuthenticatorStatus::
                                 REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED);
  }
}

TEST_F(AuthenticatorImplRemoteDesktopClientOverrideTest, MakeCredentialAppid) {
  // Verify that origin overriding extends to the appidExclude extension. If the
  // caller origin is authorized to use the extension, App ID processing is
  // applied to the overridden origin.
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
    options->remote_desktop_client_override = RemoteDesktopClientOverride::New(
        url::Origin::Create(GURL(test.remote_origin)), true);

    auto result = AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, test.expected);
  }
}

TEST_F(AuthenticatorImplRemoteDesktopClientOverrideTest, GetAssertionAppid) {
  // Verify that origin overriding extends to the appid extension. If the
  // caller origin is authorized to use the extension, App ID processing is
  // applied to the overridden origin.
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
    options->extensions->remote_desktop_client_override =
        RemoteDesktopClientOverride::New(
            url::Origin::Create(GURL(test.remote_origin)), true);

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id, test.rp_id));

    auto result = AuthenticatorGetAssertion(std::move(options));
    EXPECT_EQ(result.status, test.expected);
  }
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
            /*started_over_callback=*/base::OnceClosure(),
            /*simulate_user_cancelled=*/false),
        failure_reasons_callback_(std::move(failure_reasons_callback)) {}

  MockAuthenticatorRequestDelegateObserver(
      const MockAuthenticatorRequestDelegateObserver&) = delete;
  MockAuthenticatorRequestDelegateObserver& operator=(
      const MockAuthenticatorRequestDelegateObserver&) = delete;

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
  MOCK_METHOD1(FidoAuthenticatorRemoved, void(std::string_view));

 private:
  InterestingFailureReasonCallback failure_reasons_callback_;
};

// Fake test construct that shares all other behavior with
// AuthenticatorCommonImpl except that:
//  - FakeAuthenticatorCommonImpl does not trigger UI activity.
//  - MockAuthenticatorRequestDelegateObserver is injected to
//  |request_delegate_|
//    instead of ChromeAuthenticatorRequestDelegate.
class FakeAuthenticatorCommonImpl : public AuthenticatorCommonImpl {
 public:
  explicit FakeAuthenticatorCommonImpl(
      RenderFrameHost* render_frame_host,
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate)
      : AuthenticatorCommonImpl(render_frame_host,
                                ServingRequestsFor::kWebContents),
        mock_delegate_(std::move(mock_delegate)) {}
  ~FakeAuthenticatorCommonImpl() override = default;

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  MaybeCreateRequestDelegate() override {
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

  mojo::Remote<blink::mojom::Authenticator> ConnectToFakeAuthenticator(
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> delegate) {
    mojo::Remote<blink::mojom::Authenticator> authenticator;
    // AuthenticatorImpl owns itself. It self-destructs when the RenderFrameHost
    // navigates or is deleted.
    AuthenticatorImpl::CreateForTesting(
        *main_rfh(), authenticator.BindNewPipeAndPassReceiver(),
        std::make_unique<FakeAuthenticatorCommonImpl>(main_rfh(),
                                                      std::move(delegate)));
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
  ReplaceDiscoveryFactory(std::move(discovery_factory));

  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionFuture future;

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

  authenticator->GetAssertion(std::move(options), future.GetCallback());
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

  TestGetAssertionFuture future;
  authenticator->GetAssertion(GetTestPublicKeyCredentialRequestOptions(),
                              future.GetCallback());

  task_environment()->FastForwardBy(kTestTimeout);

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, std::get<0>(future.Get()));

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

  TestGetAssertionFuture future;
  authenticator->GetAssertion(GetTestPublicKeyCredentialRequestOptions(),
                              future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, std::get<0>(future.Get()));

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
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.GetAssertion.Result",
      AuthenticatorCommonImpl::CredentialRequestResult::kOtherError, 1);
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
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.GetAssertion.Result",
      AuthenticatorCommonImpl::CredentialRequestResult::kOtherSuccess, 1);
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
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.MakeCredential.Result",
      AuthenticatorCommonImpl::CredentialRequestResult::kOtherError, 1);
}

TEST_F(AuthenticatorImplTest, MakeCredentialResultMetricSuccess) {
  NavigateAndCommit(GURL(kTestOrigin1));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  histogram_tester.ExpectUniqueSample(
      "WebAuthentication.MakeCredential.Result",
      AuthenticatorCommonImpl::CredentialRequestResult::kOtherSuccess, 1);
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
    EXPECT_TRUE(base::ranges::none_of(
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
        RequestMode::kModalWebAuthn);
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
    auto virt_auth_options =
        blink::test::mojom::VirtualAuthenticatorOptions::New();
    virt_auth_options->protocol = device::ProtocolVersion::kCtap2;
    virt_auth_options->transport = device::FidoTransportProtocol::kInternal;
    virt_auth_options->has_resident_key = true;
    authenticator_ =
        virtual_authenticator_manager
            ->AddAuthenticatorAndReturnNonOwningPointer(*virt_auth_options);

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
    EXPECT_TRUE(
        base::Contains(authenticator_->registrations(), credential_id_));
  }
  {
    // Verify that we do not remove passkeys that don't match the cred id.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kTestRelyingPartyId;
    options->unknown_credential_id = std::vector<uint8_t>{4, 3, 2, 1};
    AuthenticatorReport(std::move(options));
    EXPECT_TRUE(
        base::Contains(authenticator_->registrations(), credential_id_));
  }
  {
    // Remove the passkey when the rp id and credential id match.
    PublicKeyCredentialReportOptionsPtr options =
        GetTestPublicKeyCredentialReportOptions();
    options->relying_party_id = kTestRelyingPartyId;
    options->unknown_credential_id = credential_id_;
    AuthenticatorReport(std::move(options));
    EXPECT_FALSE(
        base::Contains(authenticator_->registrations(), credential_id_));
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
    EXPECT_TRUE(
        base::Contains(authenticator_->registrations(), credential_id_));
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
    EXPECT_TRUE(
        base::Contains(authenticator_->registrations(), credential_id_));
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
    EXPECT_TRUE(
        base::Contains(authenticator_->registrations(), credential_id_));
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
    EXPECT_FALSE(
        base::Contains(authenticator_->registrations(), credential_id_));
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

static constexpr char kTestPIN[] = "1234";
static constexpr char16_t kTestPIN16[] = u"1234";

class UVTestAuthenticatorClientDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  explicit UVTestAuthenticatorClientDelegate(bool* collected_pin,
                                             uint32_t* min_pin_length,
                                             bool* did_bio_enrollment,
                                             bool cancel_bio_enrollment)
      : collected_pin_(collected_pin),
        min_pin_length_(min_pin_length),
        did_bio_enrollment_(did_bio_enrollment),
        cancel_bio_enrollment_(cancel_bio_enrollment) {
    *collected_pin_ = false;
    *did_bio_enrollment_ = false;
  }

  bool SupportsPIN() const override { return true; }

  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override {
    *collected_pin_ = true;
    *min_pin_length_ = options.min_pin_length;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(provide_pin_cb), kTestPIN16));
  }

  void StartBioEnrollment(base::OnceClosure next_callback) override {
    *did_bio_enrollment_ = true;
    if (cancel_bio_enrollment_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(next_callback));
      return;
    }
    bio_callback_ = std::move(next_callback);
  }

  void OnSampleCollected(int remaining_samples) override {
    if (remaining_samples <= 0) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(bio_callback_));
    }
  }

  void FinishCollectToken() override {}

 private:
  raw_ptr<bool> collected_pin_;
  raw_ptr<uint32_t> min_pin_length_;
  base::OnceClosure bio_callback_;
  raw_ptr<bool> did_bio_enrollment_;
  bool cancel_bio_enrollment_;
};

class UVTestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  // ContentBrowserClient:
  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<UVTestAuthenticatorClientDelegate>(
        &collected_pin, &min_pin_length, &did_bio_enrollment,
        cancel_bio_enrollment);
  }

  TestWebAuthenticationDelegate web_authentication_delegate;

  bool collected_pin;
  uint32_t min_pin_length = 0;
  bool did_bio_enrollment;
  bool cancel_bio_enrollment = false;
};

class UVAuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  UVAuthenticatorImplTest() = default;

  UVAuthenticatorImplTest(const UVAuthenticatorImplTest&) = delete;
  UVAuthenticatorImplTest& operator=(const UVAuthenticatorImplTest&) = delete;

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
          device::UserVerificationRequirement::kRequired,
      bool exclude_credentials = false,
      bool appid_exclude = false) {
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

  UVTestAuthenticatorContentBrowserClient test_client_;

 private:
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

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
    : public AuthenticatorRequestClientDelegate {
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
        NOTREACHED_IN_MIGRATION();
    }

    virtual_device_factory_->SetCtap2Config(config);
  }

 private:
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
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
                      NOTREACHED_IN_MIGRATION();
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
                      NOTREACHED_IN_MIGRATION();
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
                                 RequestMode::kModalWebAuthn);
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
                                 RequestMode::kModalWebAuthn);
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
                NOTREACHED_IN_MIGRATION();
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
                NOTREACHED_IN_MIGRATION();
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
                               RequestMode::kModalWebAuthn);
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
                               RequestMode::kModalWebAuthn);
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
    SCOPED_TRACE(UVToString(test_case.uv));
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
                               RequestMode::kModalWebAuthn);
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

class BlockingAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  BlockingAuthenticatorRequestDelegate() = default;

  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
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
    auto ret = std::make_unique<BlockingAuthenticatorRequestDelegate>();
    delegate_ = ret.get();
    return ret;
  }

 private:
  TestWebAuthenticationDelegate web_authentication_delegate_;
  raw_ptr<BlockingAuthenticatorRequestDelegate, AcrossTasksDanglingUntriaged>
      delegate_ = nullptr;
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

// ResidentKeyTestAuthenticatorRequestDelegate is a delegate that:
//   a) always returns |kTestPIN| when asked for a PIN.
//   b) sorts potential resident-key accounts by user ID, maps them to a string
//      form ("<hex user ID>:<user name>:<display name>"), joins the strings
//      with "/", and compares the result against |expected_accounts|.
//   c) auto-selects the account with the user ID matching |selected_user_id|.
class ResidentKeyTestAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  struct Config {
    // A string representation of the accounts expected to be passed to
    // `SelectAccount()`.
    std::string expected_accounts;

    // The user ID of the account that should be selected by `SelectAccount()`.
    std::vector<uint8_t> selected_user_id;

    // Indicates whether `SetConditional(true)` is expected to be called.
    bool expect_conditional = false;

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
    DCHECK(!config_.expect_conditional || expect_conditional_satisfied_)
        << "SetConditionalRequest() expected but not called";
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
        FROM_HERE, base::BindOnce(std::move(provide_pin_cb), kTestPIN16));
  }

  void FinishCollectToken() override {}

  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback,
      base::RepeatingCallback<
          void(device::FidoRequestHandlerBase::BlePermissionCallback)>
          ble_status_callback) override {
    account_preselected_callback_ = account_preselected_callback;
    request_callback_ = request_callback;
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
    base::ranges::transform(
        responses, std::back_inserter(string_reps),
        [](const device::AuthenticatorGetAssertionResponse& response) {
          const device::PublicKeyCredentialUserEntity& user =
              response.user_entity.value();
          return base::HexEncode(user.id) + ":" + user.name.value_or("") + ":" +
                 user.display_name.value_or("");
        });

    EXPECT_EQ(config_.expected_accounts, base::JoinString(string_reps, "/"));

    const auto selected = base::ranges::find(
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

  void SetConditionalRequest(bool is_conditional) override {
    EXPECT_EQ(config_.expect_conditional, is_conditional);
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
                                 RequestMode::kModalWebAuthn);
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

    SCOPED_TRACE(::testing::Message() << "uv=" << UVToString(test.uv));
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
            NOTREACHED_IN_MIGRATION();
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
        NOTREACHED_IN_MIGRATION();
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

// Tests that the incognito flag is plumbed through conditional UI requests.
TEST_F(ResidentKeyAuthenticatorImplTest, ConditionalUI_Incognito) {
  virtual_device_factory_->set_discover_win_webauthn_api_authenticator(true);
  fake_win_webauthn_api_.set_available(true);
  fake_win_webauthn_api_.set_version(WEBAUTHN_API_VERSION_4);
  fake_win_webauthn_api_.set_supports_silent_discovery(true);
  device::PublicKeyCredentialRpEntity rp(kTestRelyingPartyId);
  device::PublicKeyCredentialUserEntity user({1, 2, 3, 4});
  fake_win_webauthn_api_.InjectDiscoverableCredential(
      /*credential_id=*/{{4, 3, 2, 1}}, std::move(rp), std::move(user));

  // |SelectAccount| should not be called for conditional UI requests.
  test_client_.delegate_config.expected_accounts = "<invalid>";
  test_client_.delegate_config.expect_conditional = true;

  for (bool is_off_the_record : {true, false}) {
    SCOPED_TRACE(is_off_the_record ? "off the record" : "on the record");
    static_cast<TestBrowserContext*>(GetBrowserContext())
        ->set_is_off_the_record(is_off_the_record);
    PublicKeyCredentialRequestOptionsPtr options(get_credential_options());
    options->is_conditional = true;
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
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
  PublicKeyCredentialRequestOptionsPtr options(get_credential_options());
  options->is_conditional = true;
  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  VerifyGetAssertionOutcomeUkm(0, GetAssertionOutcome::kSuccess,
                               RequestMode::kConditional);
}

// Tests that the AuthenticatorRequestDelegate can choose a known platform
// authentictor credential as "preselected", which causes the request to be
// specialized to the chosen credential ID and post-request account selection UI
// to be skipped.
TEST_F(ResidentKeyAuthenticatorImplTest, PreselectDiscoverableCredential) {
  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  virtual_device_factory_->SetCtap2Config(config);
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

class InternalAuthenticatorImplTest : public AuthenticatorTestBase {
 protected:
  InternalAuthenticatorImplTest() = default;

  void SetUp() override {
    AuthenticatorTestBase::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    internal_authenticator_impl_.reset();
    SetBrowserClientForTesting(old_client_);
    AuthenticatorTestBase::TearDown();
  }

  void NavigateAndCommit(const GURL& url) {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    internal_authenticator_impl_.reset();
    RenderViewHostTestHarness::NavigateAndCommit(url);
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
  TestAuthenticatorContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

// Regression test for crbug.com/1433416.
TEST_F(InternalAuthenticatorImplTest, MakeCredentialSkipTLSCheck) {
  NavigateAndCommit(GURL(kTestOrigin1));
  InternalAuthenticatorImpl* authenticator =
      GetAuthenticator(url::Origin::Create(GURL(kTestOrigin1)));
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  TestMakeCredentialFuture future;
  authenticator->MakeCredential(std::move(options), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(std::get<0>(future.Get()),
            blink::mojom::AuthenticatorStatus::SUCCESS);
}

// Regression test for crbug.com/1433416.
TEST_F(InternalAuthenticatorImplTest, GetAssertionSkipTLSCheck) {
  NavigateAndCommit(GURL(kTestOrigin1));
  InternalAuthenticatorImpl* authenticator =
      GetAuthenticator(url::Origin::Create(GURL(kTestOrigin1)));
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, options->relying_party_id));
  TestGetAssertionFuture future;
  authenticator->GetAssertion(std::move(options), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(std::get<0>(future.Get()),
            blink::mojom::AuthenticatorStatus::SUCCESS);
}

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
    TestMakeCredentialFuture future;
    authenticator->MakeCredential(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(test_case.expected_status, std::get<0>(future.Get()));
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
    TestMakeCredentialFuture future;
    authenticator->MakeCredential(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(test_case.expected_status, std::get<0>(future.Get()));
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

    TestGetAssertionFuture future;
    authenticator->GetAssertion(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(test_case.expected_status, std::get<0>(future.Get()));
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
        options->allow_credentials[0].id, test_case.claimed_authority));
    TestGetAssertionFuture future;
    authenticator->GetAssertion(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(test_case.expected_status, std::get<0>(future.Get()));
  }
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
      crypto::ScopedFakeAppleKeychainV2::UVMethod::kPasswordOnly);
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
      : public AuthenticatorRequestClientDelegate {
   public:
    using Callback = base::RepeatingCallback<void(
        const device::FidoRequestHandlerBase::TransportAvailabilityInfo&)>;
    explicit InspectTAIAuthenticatorRequestDelegate(Callback callback)
        : callback_(std::move(callback)) {}

    void ConfigureDiscoveries(
        const url::Origin& origin,
        const std::string& rp_id,
        RequestSource request_source,
        device::FidoRequestType request_type,
        std::optional<device::ResidentKeyRequirement> resident_key_requirement,
        device::UserVerificationRequirement user_verification_requirement,
        std::optional<std::string_view> user_name,
        base::span<const device::CableDiscoveryData> pairings_from_extension,
        bool is_enclave_authenticator_available,
        device::FidoDiscoveryFactory* fido_discovery_factory) override {
      // nswindow must be set for the iCloud Keychain authenticator to be
      // discovered.
      fido_discovery_factory->set_nswindow(
          device::fido::icloud_keychain::kFakeNSWindowForTesting);
    }

    void OnTransportAvailabilityEnumerated(
        device::FidoRequestHandlerBase::TransportAvailabilityInfo tai)
        override {
      callback_.Run(tai);
    }

   private:
    Callback callback_;
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
      const device::FidoRequestHandlerBase::TransportAvailabilityInfo& tai) {
    if (tai_callback_) {
      std::move(tai_callback_).Run(tai);
    }
  }

  static std::vector<device::DiscoverableCredentialMetadata> GetCredentials() {
    device::DiscoverableCredentialMetadata metadata(
        device::AuthenticatorType::kICloudKeychain, kTestRelyingPartyId,
        {1, 2, 3, 4}, {{5, 6, 7, 8}, "name", "displayName"});
    return {std::move(metadata)};
  }

  InspectTAIContentBrowserClient test_client_{base::BindRepeating(
      &ICloudKeychainAuthenticatorImplTest::OnTransportAvailabilityEnumerated,
      base::Unretained(this))};
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
  InspectTAIAuthenticatorRequestDelegate::Callback tai_callback_;
};

// Gardener 2024-06-18: Disabled due to asan failures (crbug.com/347287026).
TEST_F(ICloudKeychainAuthenticatorImplTest, DISABLED_Discovery) {
  if (__builtin_available(macOS 13.5, *)) {
    NavigateAndCommit(GURL(kTestOrigin1));
    device::fido::icloud_keychain::ScopedTestEnvironment test_environment(
        GetCredentials());
    bool tai_seen = false;
    tai_callback_ = base::BindLambdaForTesting(
        [&tai_seen](
            const device::FidoRequestHandlerBase::TransportAvailabilityInfo&
                tai) {
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
    GTEST_SKIP() << "Need macOS 13.3 for this test";
  }
}

#endif  // BUILDFLAG(IS_MAC)

// AuthenticatorCableV2Test tests features of the caBLEv2 transport and
// protocol.
class AuthenticatorCableV2Test : public AuthenticatorImplRequestDelegateTest {
 public:
  void SetUp() override {
    AuthenticatorImplTest::SetUp();

    NavigateAndCommit(GURL(kTestOrigin1));
    ResetNetworkService();

    old_client_ = SetBrowserClientForTesting(&browser_client_);

    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    bssl::UniquePtr<EC_KEY> peer_identity(EC_KEY_derive_from_secret(
        p256.get(), zero_seed_.data(), zero_seed_.size()));
    CHECK_EQ(sizeof(peer_identity_x962_),
             EC_POINT_point2oct(
                 p256.get(), EC_KEY_get0_public_key(peer_identity.get()),
                 POINT_CONVERSION_UNCOMPRESSED, peer_identity_x962_,
                 sizeof(peer_identity_x962_), /*ctx=*/nullptr));

    std::tie(ble_advert_callback_, ble_advert_events_) =
        device::cablev2::Discovery::AdvertEventStream::New();
  }

  void TearDown() override {
    // Ensure that all pending caBLE connections have timed out and closed.
    task_environment()->FastForwardBy(base::Minutes(10));

    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();

    // All `EstablishedConnection` instances should have been destroyed.
    CHECK_EQ(device::cablev2::FidoTunnelDevice::
                 GetNumEstablishedConnectionInstancesForTesting(),
             0);
  }

  base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
  GetPairingCallback() {
    return base::BindRepeating(&AuthenticatorCableV2Test::OnNewPairing,
                               base::Unretained(this));
  }

  base::RepeatingCallback<void(std::unique_ptr<device::cablev2::Pairing>)>
  GetInvalidatedPairingCallback() {
    return base::BindRepeating(&AuthenticatorCableV2Test::OnInvalidatedPairing,
                               base::Unretained(this));
  }

  base::RepeatingCallback<void(Event)> GetEventCallback() {
    return base::BindRepeating(&AuthenticatorCableV2Test::OnCableEvent,
                               base::Unretained(this));
  }

  void EnableConnectionSignalAtTunnelServer() {
    // Recreate the tunnel server so that it supports the connection signal.
    network_context_ = device::cablev2::NewMockTunnelServer(
        base::BindRepeating(&AuthenticatorCableV2Test::OnContact,
                            base::Unretained(this)),
        /*supports_connect_signal=*/true);
  }

 protected:
  class DiscoveryFactory : public device::FidoDiscoveryFactory {
   public:
    explicit DiscoveryFactory(
        std::unique_ptr<device::cablev2::Discovery> discovery)
        : discovery_(std::move(discovery)) {}

    std::vector<std::unique_ptr<device::FidoDiscoveryBase>> Create(
        device::FidoTransportProtocol transport) override {
      if (transport != device::FidoTransportProtocol::kHybrid || !discovery_) {
        return {};
      }

      return SingleDiscovery(std::move(discovery_));
    }

   private:
    std::unique_ptr<device::cablev2::Discovery> discovery_;
  };

  class TestAuthenticationDelegate : public WebAuthenticationDelegate {
   public:
    bool SupportsResidentKeys(RenderFrameHost*) override { return true; }

    bool IsFocused(WebContents* web_contents) override { return true; }
  };

  class ContactWhenReadyAuthenticatorRequestDelegate
      : public AuthenticatorRequestClientDelegate {
   public:
    explicit ContactWhenReadyAuthenticatorRequestDelegate(
        base::RepeatingClosure callback)
        : callback_(callback) {}
    ~ContactWhenReadyAuthenticatorRequestDelegate() override = default;

    void OnTransportAvailabilityEnumerated(
        device::FidoRequestHandlerBase::TransportAvailabilityInfo) override {
      callback_.Run();
    }

   private:
    base::RepeatingClosure callback_;
  };

  class ContactWhenReadyContentBrowserClient : public ContentBrowserClient {
   public:
    explicit ContactWhenReadyContentBrowserClient(
        base::RepeatingClosure callback)
        : callback_(callback) {}

    std::unique_ptr<AuthenticatorRequestClientDelegate>
    GetWebAuthenticationRequestDelegate(
        RenderFrameHost* render_frame_host) override {
      return std::make_unique<ContactWhenReadyAuthenticatorRequestDelegate>(
          callback_);
    }

    WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
      return &authentication_delegate_;
    }

   private:
    base::RepeatingClosure callback_;
    TestAuthenticationDelegate authentication_delegate_;
  };

  // MaybeContactPhones is called when OnTransportAvailabilityEnumerated is
  // called by the request handler.
  void MaybeContactPhones() {
    if (maybe_contact_phones_callback_) {
      std::move(maybe_contact_phones_callback_).Run();
    }
  }

  void OnContact(
      base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
      base::span<const uint8_t, device::cablev2::kPairingIDSize> pairing_id,
      base::span<const uint8_t, device::cablev2::kClientNonceSize> client_nonce,
      const std::string& request_type_hint) {
    std::move(contact_callback_)
        .Run(tunnel_id, pairing_id, client_nonce, request_type_hint);
  }

  void OnNewPairing(std::unique_ptr<device::cablev2::Pairing> pairing) {
    pairings_.emplace_back(std::move(pairing));
  }

  void OnInvalidatedPairing(
      std::unique_ptr<device::cablev2::Pairing> disabled_pairing) {
    pairings_.erase(base::ranges::find_if(
        pairings_, [&disabled_pairing](const auto& pairing) {
          return device::cablev2::Pairing::EqualPublicKeys(pairing,
                                                           disabled_pairing);
        }));
  }

  void OnCableEvent(Event event) { events_.push_back(event); }

  void DoPairingConnection() {
    // First do unpaired exchange to get pairing data.
    auto discovery = std::make_unique<device::cablev2::Discovery>(
        device::FidoRequestType::kGetAssertion,
        base::BindLambdaForTesting([&]() { return network_context_.get(); }),
        qr_generator_key_, std::move(ble_advert_events_),
        /*contact_device_stream=*/nullptr,
        /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
        GetPairingCallback(), GetInvalidatedPairingCallback(),
        GetEventCallback(), /*must_support_ctap=*/true);

    ReplaceDiscoveryFactory(
        std::make_unique<DiscoveryFactory>(std::move(discovery)));

    const std::vector<uint8_t> contact_id(/*count=*/200, /*value=*/1);
    std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
        device::cablev2::authenticator::TransactFromQRCode(
            device::cablev2::authenticator::NewMockPlatform(
                std::move(ble_advert_callback_), &virtual_device_,
                /*observer=*/nullptr),
            base::BindLambdaForTesting(
                [&]() { return network_context_.get(); }),
            root_secret_, "Test Authenticator", zero_qr_secret_,
            peer_identity_x962_, contact_id);

    EXPECT_EQ(AuthenticatorMakeCredential().status,
              AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(pairings_.size(), 1u);

    // Now do a pairing-based exchange. Generate a random request type hint to
    // ensure that all values work.
    device::FidoRequestType request_type =
        device::FidoRequestType::kMakeCredential;
    std::string expected_request_type_string = "mc";
    if (base::RandDouble() < 0.5) {
      request_type = device::FidoRequestType::kGetAssertion;
      expected_request_type_string = "ga";
    }

    std::tie(ble_advert_callback_, ble_advert_events_) =
        device::cablev2::Discovery::EventStream<
            base::span<const uint8_t, device::cablev2::kAdvertSize>>::New();
    auto callback_and_event_stream = device::cablev2::Discovery::EventStream<
        std::unique_ptr<device::cablev2::Pairing>>::New();
    discovery = std::make_unique<device::cablev2::Discovery>(
        request_type,
        base::BindLambdaForTesting([&]() { return network_context_.get(); }),
        qr_generator_key_, std::move(ble_advert_events_),
        std::move(callback_and_event_stream.second),
        /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
        GetPairingCallback(), GetInvalidatedPairingCallback(),
        GetEventCallback(), /*must_support_ctap=*/true);

    maybe_contact_phones_callback_ = base::BindLambdaForTesting([&]() {
      callback_and_event_stream.first.Run(
          std::make_unique<device::cablev2::Pairing>(*pairings_[0]));
    });

    const std::array<uint8_t, device::cablev2::kRoutingIdSize> routing_id = {0};
    bool contact_callback_IsReady = false;
    // When the |cablev2::Discovery| starts it'll make a connection to the
    // tunnel service with the contact ID from the pairing data. This will be
    // handled by the |TestNetworkContext| and turned into a call to
    // |contact_callback_|. This simulates the tunnel server sending a cloud
    // message to a phone. Given the information from the connection, a
    // transaction can be created.
    contact_callback_ = base::BindLambdaForTesting(
        [this, &transaction, routing_id, contact_id, &contact_callback_IsReady,
         &expected_request_type_string](
            base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
            base::span<const uint8_t, device::cablev2::kPairingIDSize>
                pairing_id,
            base::span<const uint8_t, device::cablev2::kClientNonceSize>
                client_nonce,
            const std::string& request_type_hint) -> void {
          contact_callback_IsReady = true;
          CHECK_EQ(request_type_hint, expected_request_type_string);
          transaction = device::cablev2::authenticator::TransactFromFCM(
              device::cablev2::authenticator::NewMockPlatform(
                  std::move(ble_advert_callback_), &virtual_device_,
                  /*observer=*/nullptr),
              base::BindLambdaForTesting(
                  [&]() { return network_context_.get(); }),
              root_secret_, routing_id, tunnel_id, pairing_id, client_nonce,
              contact_id);
        });

    ReplaceDiscoveryFactory(
        std::make_unique<DiscoveryFactory>(std::move(discovery)));

    EXPECT_EQ(AuthenticatorMakeCredential().status,
              AuthenticatorStatus::SUCCESS);
    EXPECT_TRUE(contact_callback_IsReady);
  }

  void ResetNetworkService() {
    network_context_ = device::cablev2::NewMockTunnelServer(base::BindRepeating(
        &AuthenticatorCableV2Test::OnContact, base::Unretained(this)));
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
  device::VirtualCtap2Device virtual_device_{DeviceState(), DeviceConfig()};
  std::vector<std::unique_ptr<device::cablev2::Pairing>> pairings_;
  base::OnceCallback<void(
      base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
      base::span<const uint8_t, device::cablev2::kPairingIDSize> pairing_id,
      base::span<const uint8_t, device::cablev2::kClientNonceSize> client_nonce,
      const std::string& request_type_hint)>
      contact_callback_;
  std::unique_ptr<device::cablev2::Discovery::AdvertEventStream>
      ble_advert_events_;
  device::cablev2::Discovery::AdvertEventStream::Callback ble_advert_callback_;
  ContactWhenReadyContentBrowserClient browser_client_{
      base::BindRepeating(&AuthenticatorCableV2Test::MaybeContactPhones,
                          base::Unretained(this))};
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
  base::OnceClosure maybe_contact_phones_callback_;
  std::vector<Event> events_;

 private:
  static VirtualCtap2Device::State* DeviceState() {
    VirtualCtap2Device::State* state = new VirtualCtap2Device::State;
    state->fingerprints_enrolled = true;
    state->default_backup_eligibility = true;
    return state;
  }

  static VirtualCtap2Device::Config DeviceConfig() {
    // `MockPlatform` uses a virtual device to answer requests, but it can't
    // handle the credential ID being omitted in responses.
    VirtualCtap2Device::Config ret;
    ret.include_credential_in_assertion_response =
        VirtualCtap2Device::Config::IncludeCredential::ALWAYS;
    ret.prf_support = true;
    ret.internal_account_chooser = true;
    ret.internal_uv_support = true;
    ret.always_uv = true;
    return ret;
  }
};

TEST_F(AuthenticatorCableV2Test, QRBasedWithNoPairing) {
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion,
      base::BindLambdaForTesting([&]() { return network_context_.get(); }),
      qr_generator_key_, std::move(ble_advert_events_),
      /*contact_device_stream=*/nullptr,
      /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
      GetPairingCallback(), GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          device::cablev2::authenticator::NewMockPlatform(
              std::move(ble_advert_callback_), &virtual_device_,
              /*observer=*/nullptr),
          base::BindLambdaForTesting([&]() { return network_context_.get(); }),
          root_secret_, "Test Authenticator", zero_qr_secret_,
          peer_identity_x962_,
          /*contact_id=*/std::nullopt);

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(pairings_.size(), 0u);
}

TEST_F(AuthenticatorCableV2Test, HandshakeError) {
  // A handshake error should be fatal to the request with
  // `kHybridTransportError`.
  auto network_context_factory =
      base::BindLambdaForTesting([&]() { return network_context_.get(); });
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion, network_context_factory,
      qr_generator_key_, std::move(ble_advert_events_),
      /*contact_device_stream=*/nullptr,
      /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
      GetPairingCallback(), GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::NewHandshakeErrorDevice(
          device::cablev2::authenticator::NewMockPlatform(
              std::move(ble_advert_callback_), &virtual_device_,
              /*observer=*/nullptr),
          network_context_factory, zero_qr_secret_);

  FailureReasonFuture failure_reason_future;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_future.GetCallback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  TestMakeCredentialFuture future;
  authenticator->MakeCredential(GetTestPublicKeyCredentialCreationOptions(),
                                future.GetCallback());

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, std::get<0>(future.Get()));

  ASSERT_TRUE(failure_reason_future.IsReady());
  EXPECT_EQ(AuthenticatorRequestClientDelegate::InterestingFailureReason::
                kHybridTransportError,
            failure_reason_future.Get());
}

// Test having the network service crash between creating a discovery and
// performing a cable transaction. Regression test for crbug.com/332724843.
TEST_F(AuthenticatorCableV2Test, NetworkServiceCrash) {
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion,
      base::BindLambdaForTesting([&]() { return network_context_.get(); }),
      qr_generator_key_, std::move(ble_advert_events_),
      /*contact_device_stream=*/nullptr,
      /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
      GetPairingCallback(), GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));

  // Simulate the network service restarting.
  ResetNetworkService();

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          device::cablev2::authenticator::NewMockPlatform(
              std::move(ble_advert_callback_), &virtual_device_,
              /*observer=*/nullptr),
          base::BindLambdaForTesting([&]() { return network_context_.get(); }),
          root_secret_, "Test Authenticator", zero_qr_secret_,
          peer_identity_x962_,
          /*contact_id=*/std::nullopt);

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(pairings_.size(), 0u);
}

TEST_F(AuthenticatorCableV2Test, PairingBased) {
  DoPairingConnection();

  const std::vector<Event> kExpectedEvents = {
      // From the QR connection
      Event::kBLEAdvertReceived,
      Event::kReady,
      // From the paired connection
      Event::kBLEAdvertReceived,
      Event::kReady,
  };
  EXPECT_EQ(events_, kExpectedEvents);
}

TEST_F(AuthenticatorCableV2Test, PairingBasedWithConnectionSignal) {
  EnableConnectionSignalAtTunnelServer();
  DoPairingConnection();

  const std::vector<Event> kExpectedEvents = {
      // From the QR connection
      Event::kBLEAdvertReceived,
      Event::kReady,
      // From the paired connection
      Event::kPhoneConnected,
      Event::kBLEAdvertReceived,
      Event::kReady,
  };
  EXPECT_EQ(events_, kExpectedEvents);
}

static std::unique_ptr<device::cablev2::Pairing> DummyPairing() {
  auto ret = std::make_unique<device::cablev2::Pairing>();
  ret->tunnel_server_domain = device::cablev2::kTunnelServer;
  ret->contact_id = {1, 2, 3, 4, 5};
  ret->id = {6, 7, 8, 9};
  ret->secret = {10, 11, 12, 13};
  std::fill(ret->peer_public_key_x962.begin(), ret->peer_public_key_x962.end(),
            22);
  ret->name = __func__;

  return ret;
}

TEST_F(AuthenticatorCableV2Test, ContactIDDisabled) {
  // Passing |nullopt| as the callback here causes all contact IDs to be
  // rejected.
  network_context_ = device::cablev2::NewMockTunnelServer(std::nullopt);
  auto callback_and_event_stream = device::cablev2::Discovery::EventStream<
      std::unique_ptr<device::cablev2::Pairing>>::New();
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion,
      base::BindLambdaForTesting([&]() { return network_context_.get(); }),
      qr_generator_key_, std::move(ble_advert_events_),
      std::move(callback_and_event_stream.second),
      /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
      GetPairingCallback(), GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));

  maybe_contact_phones_callback_ =
      base::BindLambdaForTesting([&callback_and_event_stream]() {
        callback_and_event_stream.first.Run(DummyPairing());
      });

  pairings_.emplace_back(DummyPairing());
  ASSERT_EQ(pairings_.size(), 1u);
  EXPECT_EQ(AuthenticatorMakeCredentialAndWaitForTimeout(
                GetTestPublicKeyCredentialCreationOptions())
                .status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  // The pairing should be been erased because of the signal from the tunnel
  // server.
  ASSERT_EQ(pairings_.size(), 0u);
}

// ServerLinkValues contains keys that mimic those created by a site doing
// caBLEv2 server-link.
struct ServerLinkValues {
  // This value would be provided by the site to the desktop, in a caBLE
  // extension in the get() call.
  device::CableDiscoveryData desktop_side;

  // These values would be provided to the phone via a custom mechanism.
  std::array<uint8_t, device::cablev2::kQRSecretSize> secret;
  std::array<uint8_t, device::kP256X962Length> peer_identity;
};

// CreateServerLink simulates a site doing caBLEv2 server-link and calculates
// server-link values that could be sent to the desktop and phone sides of a
// transaction.
static ServerLinkValues CreateServerLink() {
  std::vector<uint8_t> seed(device::cablev2::kQRSeedSize);
  base::RandBytes(seed);

  bssl::UniquePtr<EC_GROUP> p256(
      EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
  bssl::UniquePtr<EC_KEY> ec_key(
      EC_KEY_derive_from_secret(p256.get(), seed.data(), seed.size()));

  ServerLinkValues ret;
  base::RandBytes(ret.secret);
  CHECK_EQ(ret.peer_identity.size(),
           EC_POINT_point2oct(p256.get(), EC_KEY_get0_public_key(ec_key.get()),
                              POINT_CONVERSION_UNCOMPRESSED,
                              ret.peer_identity.data(),
                              ret.peer_identity.size(), /*ctx=*/nullptr));

  ret.desktop_side.version = device::CableDiscoveryData::Version::V2;
  ret.desktop_side.v2.emplace(seed, std::vector<uint8_t>());
  ret.desktop_side.v2->server_link_data.insert(
      ret.desktop_side.v2->server_link_data.end(), ret.secret.begin(),
      ret.secret.end());

  return ret;
}

TEST_F(AuthenticatorCableV2Test, ServerLink) {
  const ServerLinkValues server_link_1 = CreateServerLink();
  const ServerLinkValues server_link_2 = CreateServerLink();
  const std::vector<device::CableDiscoveryData> extension_values = {
      server_link_1.desktop_side, server_link_2.desktop_side};

  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion,
      base::BindLambdaForTesting([&]() { return network_context_.get(); }),
      qr_generator_key_, std::move(ble_advert_events_),
      /*contact_device_stream=*/nullptr, extension_values, GetPairingCallback(),
      GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));

  // Both extension values should work, but we can only do a single transaction
  // per test because a lot of state is setup for a test. Therefore pick one of
  // the two to check, at random.
  const auto& server_link =
      (base::RandUint64() & 1) ? server_link_1 : server_link_2;

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          device::cablev2::authenticator::NewMockPlatform(
              std::move(ble_advert_callback_), &virtual_device_,
              /*observer=*/nullptr),
          base::BindLambdaForTesting([&]() { return network_context_.get(); }),
          root_secret_, "Test Authenticator", server_link.secret,
          server_link.peer_identity,
          /*contact_id=*/std::nullopt);

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(pairings_.size(), 0u);
}

TEST_F(AuthenticatorCableV2Test, LateLinking) {
  auto network_context_factory =
      base::BindLambdaForTesting([&]() { return network_context_.get(); });
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion, network_context_factory,
      qr_generator_key_, std::move(ble_advert_events_),
      /*contact_device_stream=*/nullptr,
      /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
      GetPairingCallback(), GetInvalidatedPairingCallback(), GetEventCallback(),
      /*must_support_ctap=*/true);

  ReplaceDiscoveryFactory(
      std::make_unique<DiscoveryFactory>(std::move(discovery)));

  const std::vector<uint8_t> contact_id(/*count=*/200, /*value=*/1);
  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::NewLateLinkingDevice(
          device::CtapDeviceResponseCode::kCtap2ErrOperationDenied,
          device::cablev2::authenticator::NewMockPlatform(
              std::move(ble_advert_callback_), &virtual_device_,
              /*observer=*/nullptr),
          network_context_factory, zero_qr_secret_, peer_identity_x962_);

  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);

  // There should not be any pairing at this point because the device shouldn't
  // have sent the information yet.
  EXPECT_EQ(pairings_.size(), 0u);

  // After 30 seconds, a pairing should have been recorded even though the
  // WebAuthn request has completed.
  task_environment()->FastForwardBy(base::Seconds(30));
  EXPECT_EQ(pairings_.size(), 1u);
}

// AuthenticatorCableV2AuthenticatorTest tests aspects of the authenticator
// implementation, rather than of the underlying caBLEv2 transport.
class AuthenticatorCableV2AuthenticatorTest
    : public AuthenticatorCableV2Test,
      public device::cablev2::authenticator::Observer {
 public:
  void SetUp() override {
    AuthenticatorCableV2Test::SetUp();

    auto discovery = std::make_unique<device::cablev2::Discovery>(
        device::FidoRequestType::kGetAssertion,
        base::BindLambdaForTesting([&]() { return network_context_.get(); }),
        qr_generator_key_, std::move(ble_advert_events_),
        /*contact_device_stream=*/nullptr,
        /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
        GetPairingCallback(), GetInvalidatedPairingCallback(),
        GetEventCallback(), /*must_support_ctap=*/true);

    ReplaceDiscoveryFactory(
        std::make_unique<DiscoveryFactory>(std::move(discovery)));

    transaction_ = device::cablev2::authenticator::TransactFromQRCode(
        device::cablev2::authenticator::NewMockPlatform(
            std::move(ble_advert_callback_), &virtual_device_, this),
        base::BindLambdaForTesting([&]() { return network_context_.get(); }),
        root_secret_, "Test Authenticator", zero_qr_secret_,
        peer_identity_x962_,
        /*contact_id=*/std::nullopt);
  }

 protected:
  // device::cablev2::authenticator::Observer
  void OnStatus(device::cablev2::authenticator::Platform::Status) override {}
  void OnCompleted(
      std::optional<device::cablev2::authenticator::Platform::Error> error)
      override {
    CHECK(!did_complete_);
    did_complete_ = true;
    error_ = error;
  }

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction_;
  bool did_complete_ = false;
  std::optional<device::cablev2::authenticator::Platform::Error> error_;
};

TEST_F(AuthenticatorCableV2AuthenticatorTest, GetAssertion) {
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials[0].transports.insert(
      device::FidoTransportProtocol::kHybrid);
  ASSERT_TRUE(virtual_device_.mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, options->relying_party_id));

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorCableV2AuthenticatorTest, MakeDiscoverableCredential) {
  auto options = GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->resident_key =
      device::ResidentKeyRequirement::kRequired;
  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);

  ASSERT_TRUE(did_complete_);
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ(*error_, device::cablev2::authenticator::Platform::Error::
                         DISCOVERABLE_CREDENTIALS_REQUEST);
}

TEST_F(AuthenticatorCableV2AuthenticatorTest, EmptyAllowList) {
  auto options = GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials.clear();
  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);

  ASSERT_TRUE(did_complete_);
  ASSERT_TRUE(error_.has_value());
  EXPECT_EQ(*error_, device::cablev2::authenticator::Platform::Error::
                         DISCOVERABLE_CREDENTIALS_REQUEST);
}

TEST_F(AuthenticatorCableV2AuthenticatorTest, PRFMakeCredential) {
  auto options = GetTestPublicKeyCredentialCreationOptions();
  options->prf_enable = true;

  const auto result = AuthenticatorMakeCredential(std::move(options));

  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(result.response->echo_prf);
  EXPECT_TRUE(result.response->prf);
}

static std::vector<uint8_t> HashPRFInput(base::span<const uint8_t> input) {
  std::vector<uint8_t> hash_input;
  constexpr char kPrefix[] = "WebAuthn PRF";
  hash_input.insert(hash_input.end(), std::begin(kPrefix), std::end(kPrefix));
  hash_input.insert(hash_input.end(), std::begin(input), std::end(input));
  return device::fido_parsing_utils::Materialize(
      crypto::SHA256Hash(hash_input));
}

static std::tuple<PublicKeyCredentialRequestOptionsPtr,
                  std::vector<uint8_t>,
                  std::vector<uint8_t>>
BuildPRFGetAssertion(device::VirtualCtap2Device& virtual_device,
                     bool use_eval_by_credential) {
  const std::vector<uint8_t> input1(32, 1);
  const std::vector<uint8_t> input2(32, 2);
  const std::vector<uint8_t> salt1 = HashPRFInput(input1);
  const std::vector<uint8_t> salt2 = HashPRFInput(input2);
  const std::array<uint8_t, 32> key1 = {1};
  const std::array<uint8_t, 32> key2 = {2};
  const std::array<uint8_t, 32> output1 = EvaluateHMAC(key2, salt1);
  const std::array<uint8_t, 32> output2 = EvaluateHMAC(key2, salt2);
  auto options = GetTestPublicKeyCredentialRequestOptions();

  CHECK(virtual_device.mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, options->relying_party_id));
  virtual_device.mutable_state()
      ->registrations.begin()
      ->second.hmac_key.emplace(key1, key2);

  std::vector<blink::mojom::PRFValuesPtr> prf_inputs;
  auto prf_value = blink::mojom::PRFValues::New();
  prf_value->first = input1;
  prf_value->second = input2;
  if (use_eval_by_credential) {
    prf_value->id = options->allow_credentials[0].id;
  }
  prf_inputs.emplace_back(std::move(prf_value));

  options->allow_credentials[0].transports.insert(
      device::FidoTransportProtocol::kHybrid);
  options->extensions->prf = true;
  options->extensions->prf_inputs = std::move(prf_inputs);
  options->user_verification = device::UserVerificationRequirement::kRequired;

  return std::make_tuple(std::move(options),
                         device::fido_parsing_utils::Materialize(output1),
                         device::fido_parsing_utils::Materialize(output2));
}

TEST_F(AuthenticatorCableV2AuthenticatorTest, PRFGetAssertion) {
  PublicKeyCredentialRequestOptionsPtr options;
  std::vector<uint8_t> output1, output2;
  std::tie(options, output1, output2) = BuildPRFGetAssertion(
      virtual_device_, /* use_eval_by_credential= */ false);

  const auto result = AuthenticatorGetAssertion(std::move(options));

  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(result.response->extensions->echo_prf);
  EXPECT_TRUE(result.response->extensions->prf_results);
  EXPECT_EQ(result.response->extensions->prf_results->first, output1);
  ASSERT_TRUE(result.response->extensions->prf_results->second.has_value());
  EXPECT_EQ(*result.response->extensions->prf_results->second, output2);
}

TEST_F(AuthenticatorCableV2AuthenticatorTest, PRFGetAssertionByCredential) {
  PublicKeyCredentialRequestOptionsPtr options;
  std::vector<uint8_t> output1, output2;
  std::tie(options, output1, output2) =
      BuildPRFGetAssertion(virtual_device_, /* use_eval_by_credential= */ true);

  const auto result = AuthenticatorGetAssertion(std::move(options));

  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(result.response->extensions->echo_prf);
  EXPECT_TRUE(result.response->extensions->prf_results);
  EXPECT_EQ(result.response->extensions->prf_results->first, output1);
  ASSERT_TRUE(result.response->extensions->prf_results->second.has_value());
  EXPECT_EQ(*result.response->extensions->prf_results->second, output2);
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
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnGoogleCorpRemoteDesktopClientPrivilege};
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

// Temporary regression test for crbug.com/1489468.
// TODO(crbug.com/40284051): Remove after passkey metadata syncing is enabled by
// default.
TEST_F(AuthenticatorImplWithRequestProxyTest,
       IsConditionalMediationAvailable_MetadataSyncing) {
  test_client_.GetTestWebAuthenticationDelegate()
      ->supports_passkey_metadata_syncing = true;

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

// Verify requests with an attached proxy run RP ID checks.
TEST_F(AuthenticatorImplWithRequestProxyTest, MakeCredentialOriginAndRpIds) {
  request_proxy().config().request_success = true;
  request_proxy().config().make_credential_response =
      MakeCredentialAuthenticatorResponse::New();
  request_proxy().config().make_credential_response->info =
      CommonCredentialInfo::New();

  for (const OriginClaimedAuthorityPair& test_case :
       kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

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
    SCOPED_TRACE(std::string(test_case.origin) + " " +
                 std::string(test_case.claimed_authority));

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

  // Test invalid cases that should be rejected. `kInvalidRelyingPartyTestCases`
  // contains a mix of RP ID an App ID cases, but they should all be rejected.
  for (const OriginClaimedAuthorityPair& test_case :
       kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    if (strlen(test_case.claimed_authority) == 0) {
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
  request->is_conditional = true;
  GetAssertionResult result = AuthenticatorGetAssertion(std::move(request));

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
       kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

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

}  // namespace content
