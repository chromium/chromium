// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/enclave/enclave_protocol_utils.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/json_request.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
namespace {

constexpr uint8_t kHandshakeHash[32] = {
    0xac, 0xf0, 0xdf, 0xe4, 0x51, 0xe4, 0x6d, 0x77, 0xfc, 0x64, 0xc1,
    0x1f, 0xe9, 0x40, 0xc5, 0x00, 0x66, 0xd9, 0x0c, 0x54, 0xaf, 0x27,
    0xe9, 0xaa, 0x91, 0xe4, 0x4a, 0xd2, 0x72, 0x55, 0xbe, 0xd1};
constexpr uint8_t kDeviceId[] = "device0";
constexpr uint8_t kUserId[] = "ab";
constexpr uint8_t kEncryptedPasskey[] = {1, 2, 3, 4};
constexpr char kClientDataJson[] = "client_data_json";
constexpr char kRpId[] = "test.example";
constexpr char kGetAssertionRequestJson[] = R"({
    "allowCredentials":[
        {
            "id":"FBUW",
            "transports":["usb"],
            "type":"public-key"
        }
    ],
    "challenge":"dGVzdCBjaGFsbGVuZ2U",
    "rpId":"test.example",
    "userVerification":"required"})";
constexpr char kMakeCredentialRequestJson[] = R"({
    "attestation":"direct",
    "authenticatorSelection":{
        "authenticatorAttachment":"platform",
        "residentKey":"required",
        "userVerification":"required"
    },
    "challenge":"dGVzdCBjaGFsbGVuZ2U",
    "excludeCredentials":[
        {
            "id":"FBUW",
            "transports":["usb"],
            "type":"public-key"
        }],
    "pubKeyCredParams":[
        {
            "alg":-7,
            "type":"public-key"
        },
        {
            "alg":-257,
            "type":"public-key"
        }],
    "rp":{
        "id":"test.example",
        "name":"Example LLC"},
    "user":{
        "displayName":"Example User",
        "id":"dGVzdCB1c2VyIGlk",
        "name":"user@test.example"}})";

// Hex outputs are encoded CBOR serializations of test responses.
constexpr char kGetAssertionHexResponse[] =
    "81A1626F6BA168726573706F6E7365A3697369676E61747572656655326C6E62676A757365"
    "7248616E646C65635957497161757468656E74696361746F72446174617832455A51696A61"
    "6A39766537394A687658746C6C635F58436C44584447514876504154337062553737463934"
    "42414141414F77";
constexpr char kMakeCredentialHexResponse[] =
    "81A1626F6BA3667075624B657944050607086776657273696F6E0169656E63727970746564"
    "4401020304";

struct BadResponseTestCase {
  std::string name;
  std::string hex_cbor;
};

BadResponseTestCase kFailingGetAssertionResponses[] = {
    {"Command response element is not a map", "81F5"},
    {"Error received from enclave",
     "81A16365727270536572766572206572726F7220313031"},
    {"Command response did not contain a successful response or an error",
     "81A167556E6B6E6F776E70536572766572206572726F7220313031"},
    {"Command response did not contain a response field",
     "81A1626F6BA167556E6B6E6F776E60"},
    {"Invalid AuthenticatorGetAssertionResponse",
     "81A1626F6BA168726573706F6E7365A2697369676E61747572656655326C6E62676A75736"
     "57248616E646C6563595749"}};

BadResponseTestCase kFailingMakeCredentialResponses[] = {
    {"Command response element is not a map", "81F5"},
    {"Error received from enclave",
     "81A16365727270536572766572206572726F7220313031"},
    {"Command response did not contain a successful response or an error",
     "81A167556E6B6E6F776E70536572766572206572726F7220313031"},
    {"MakeCredential response did not contain a version",
     "81A1626F6BA2667075624B6579440506070869656E637279707465644401020304"},
    {"MakeCredential response did not contain a public key",
     "81A1626F6BA26776657273696F6E0169656E637279707465644401020304"},
    {"MakeCredential response did not contain an encrypted passkey",
     "81A1626F6BA2667075624B657944050607086776657273696F6E01"},
};

sync_pb::WebauthnCredentialSpecifics PasskeyEntity() {
  sync_pb::WebauthnCredentialSpecifics entity =
      sync_pb::WebauthnCredentialSpecifics::default_instance();
  return entity;
}

std::vector<uint8_t> FakeSigningCallback(
    base::span<const uint8_t> handshake_hash,
    base::span<const uint8_t> data) {
  EXPECT_EQ(fido_parsing_utils::Materialize(handshake_hash),
            fido_parsing_utils::Materialize(kHandshakeHash));
  return std::vector<uint8_t>();
}

// Class to receive the result of a BuildCommandRequestBody call. Only usable
// once per instance.
class BuildCommandCompletionWaiter {
 public:
  BuildCommandCompletionWaiter() = default;

  BuildCommandCompletionWaiter(const BuildCommandCompletionWaiter&) = delete;
  BuildCommandCompletionWaiter& operator=(const BuildCommandCompletionWaiter&) =
      delete;

  ~BuildCommandCompletionWaiter() { loop_.Quit(); }

  void CompletionCallback(std::vector<uint8_t> result) {
    result_ = std::move(result);
    loop_.Quit();
  }

  const std::vector<uint8_t>& result() { return result_; }

  void Wait() { loop_.Run(); }

 private:
  std::vector<uint8_t> result_;
  base::RunLoop loop_;
};

class EnclaveProtocolUtilsTest : public testing::Test {
 public:
  EnclaveProtocolUtilsTest() = default;

  EnclaveProtocolUtilsTest(const EnclaveProtocolUtilsTest&) = delete;
  EnclaveProtocolUtilsTest& operator=(const EnclaveProtocolUtilsTest&) = delete;

  ~EnclaveProtocolUtilsTest() override = default;

  void SetUp() override {
    device_id_ = fido_parsing_utils::Materialize(kDeviceId);
    handshake_hash_ = fido_parsing_utils::Materialize(kHandshakeHash);
    user_id_ = fido_parsing_utils::Materialize(kUserId);
    encrypted_passkey_ = fido_parsing_utils::Materialize(kEncryptedPasskey);
  }

  // This checks the outer map values of a request, which are common to all
  // request types.
  absl::optional<cbor::Value> ValidateRequestFormatAndReturnCommandList(
      const cbor::Value& request_cbor) {
    EXPECT_TRUE(request_cbor.is_map());
    EXPECT_NE(request_cbor.GetMap().find(cbor::Value("sig")),
              request_cbor.GetMap().end());
    EXPECT_EQ(request_cbor.GetMap()
                  .find(cbor::Value("auth_level"))
                  ->second.GetString(),
              "hw");
    EXPECT_EQ(request_cbor.GetMap()
                  .find(cbor::Value("device_id"))
                  ->second.GetBytestring(),
              device_id());
    auto encoded_request =
        request_cbor.GetMap().find(cbor::Value("encoded_requests"));
    EXPECT_NE(encoded_request, request_cbor.GetMap().end());
    absl::optional<cbor::Value> decoded_command =
        cbor::Reader::Read(encoded_request->second.GetBytestring());
    EXPECT_TRUE(decoded_command);
    EXPECT_TRUE(decoded_command->is_array());
    EXPECT_EQ(decoded_command->GetArray().size(), 1u);
    return decoded_command;
  }

  std::vector<uint8_t>& device_id() { return device_id_; }

  std::vector<uint8_t>& handshake_hash() { return handshake_hash_; }

  std::vector<uint8_t>& user_id() { return user_id_; }

  std::vector<uint8_t>& encrypted_passkey() { return encrypted_passkey_; }

 private:
  std::vector<uint8_t> device_id_;
  std::vector<uint8_t> handshake_hash_;
  std::vector<uint8_t> user_id_;
  std::vector<uint8_t> encrypted_passkey_;
  base::test::TaskEnvironment task_environment_;
};

}  // namespace

namespace enclave {

TEST_F(EnclaveProtocolUtilsTest, BuildGetAssertionRequest_Success) {
  BuildCommandCompletionWaiter waiter;
  auto entity = PasskeyEntity();
  entity.set_rp_id(kRpId);
  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(kGetAssertionRequestJson);
  EXPECT_TRUE(parsed_json);
  auto json_request =
      base::MakeRefCounted<JSONRequest>(std::move(*parsed_json));
  BuildCommandRequestBody(
      base::BindOnce(&BuildGetAssertionCommand, std::move(entity), json_request,
                     kClientDataJson),
      base::BindRepeating(&FakeSigningCallback), handshake_hash(), device_id(),
      base::BindOnce(&BuildCommandCompletionWaiter::CompletionCallback,
                     base::Unretained(&waiter)));

  waiter.Wait();

  absl::optional<cbor::Value> request_cbor =
      cbor::Reader::Read(waiter.result());
  auto decoded_command =
      ValidateRequestFormatAndReturnCommandList(*request_cbor);
  auto& command_element = decoded_command->GetArray()[0];
  EXPECT_EQ(
      command_element.GetMap().find(cbor::Value("cmd"))->second.GetString(),
      "passkeys/assert");
  EXPECT_EQ(command_element.GetMap().find(cbor::Value("uv"))->second.GetBool(),
            true);
  EXPECT_EQ(command_element.GetMap()
                .find(cbor::Value("client_data_json"))
                ->second.GetString(),
            kClientDataJson);
  auto& request_value_map =
      command_element.GetMap().find(cbor::Value("request"))->second.GetMap();
  EXPECT_EQ(request_value_map.find(cbor::Value("rpId"))->second.GetString(),
            "test.example");

  auto& serialized_passkey_entity = command_element.GetMap()
                                        .find(cbor::Value("protobuf"))
                                        ->second.GetBytestring();
  sync_pb::WebauthnCredentialSpecifics out_entity;
  EXPECT_TRUE(out_entity.ParseFromArray(serialized_passkey_entity.data(),
                                        serialized_passkey_entity.size()));
  EXPECT_EQ(out_entity.rp_id(), std::string(kRpId));
}

TEST_F(EnclaveProtocolUtilsTest, BuildMakeCredentialRequest_Success) {
  BuildCommandCompletionWaiter waiter;
  absl::optional<base::Value> parsed_json =
      base::JSONReader::Read(kMakeCredentialRequestJson);
  EXPECT_TRUE(parsed_json);
  auto json_request =
      base::MakeRefCounted<JSONRequest>(std::move(*parsed_json));
  BuildCommandRequestBody(
      base::BindOnce(&BuildMakeCredentialCommand, json_request),
      base::BindRepeating(&FakeSigningCallback), handshake_hash(), device_id(),
      base::BindOnce(&BuildCommandCompletionWaiter::CompletionCallback,
                     base::Unretained(&waiter)));

  waiter.Wait();

  absl::optional<cbor::Value> request_cbor =
      cbor::Reader::Read(waiter.result());
  auto decoded_command =
      ValidateRequestFormatAndReturnCommandList(*request_cbor);
  auto& command_element = decoded_command->GetArray()[0];
  EXPECT_EQ(
      command_element.GetMap().find(cbor::Value("cmd"))->second.GetString(),
      "passkeys/create");
  auto& request_value_map =
      command_element.GetMap().find(cbor::Value("request"))->second.GetMap();
  EXPECT_EQ(request_value_map.find(cbor::Value("rp"))
                ->second.GetMap()
                .find(cbor::Value("id"))
                ->second.GetString(),
            "test.example");
}

TEST_F(EnclaveProtocolUtilsTest, ParseGetAssertionResponse_Success) {
  std::vector<uint8_t> response_serialized;
  CHECK(base::HexStringToBytes(kGetAssertionHexResponse, &response_serialized));
  std::vector<uint8_t> cred_id = {0, 1, 2};

  absl::optional<AuthenticatorGetAssertionResponse> response;
  std::string error_string;
  std::tie(response, error_string) =
      ParseGetAssertionResponse(response_serialized, cred_id);
  bool pass = (response != absl::nullopt) && (error_string.empty());
  EXPECT_TRUE(pass);
  EXPECT_EQ(response->user_entity->id, std::vector<uint8_t>({'a', 'b'}));
  EXPECT_EQ(response->credential->id, std::vector<uint8_t>({0, 1, 2}));
}

TEST_F(EnclaveProtocolUtilsTest, ParseGetAssertionResponse_Failures) {
  for (auto& test_case : kFailingGetAssertionResponses) {
    absl::optional<AuthenticatorGetAssertionResponse> response;
    std::string error_string;

    std::vector<uint8_t> response_serialized;
    CHECK(base::HexStringToBytes(test_case.hex_cbor, &response_serialized));
    std::vector<uint8_t> cred_id = {0, 1, 2};
    std::tie(response, error_string) =
        ParseGetAssertionResponse(response_serialized, cred_id);
    bool pass = (response == absl::nullopt) && (!error_string.empty());
    EXPECT_TRUE(pass) << "Failed GetAssertion response parsing for: "
                      << test_case.name;
  }
}

TEST_F(EnclaveProtocolUtilsTest, ParseMakeCredentialResponse_Success) {
  std::vector<uint8_t> response_serialized;
  CHECK(
      base::HexStringToBytes(kMakeCredentialHexResponse, &response_serialized));
  CtapMakeCredentialRequest ctap_request(
      kClientDataJson, PublicKeyCredentialRpEntity(kRpId),
      PublicKeyCredentialUserEntity(user_id()),
      PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>()));

  absl::optional<AuthenticatorMakeCredentialResponse> response;
  absl::optional<sync_pb::WebauthnCredentialSpecifics> entity;
  std::string error_string;
  std::tie(response, entity, error_string) =
      ParseMakeCredentialResponse(response_serialized, ctap_request);
  bool pass = (response != absl::nullopt) && (entity != absl::nullopt) &&
              (error_string.empty());
  EXPECT_TRUE(pass);
  EXPECT_EQ(entity->rp_id(), std::string(kRpId));
  EXPECT_EQ(entity->user_id(), std::string(user_id().begin(), user_id().end()));
  EXPECT_EQ(entity->key_version(), 1);
  EXPECT_EQ(entity->encrypted(), std::string(encrypted_passkey().begin(),
                                             encrypted_passkey().end()));
  auto response_cred_id =
      response->attestation_object.authenticator_data().GetCredentialId();
  EXPECT_EQ(entity->credential_id(),
            std::string(response_cred_id.begin(), response_cred_id.end()));
  EXPECT_TRUE(response->transports->contains(FidoTransportProtocol::kInternal));
  EXPECT_TRUE(response->transports->contains(FidoTransportProtocol::kHybrid));
  EXPECT_TRUE(response->is_resident_key);
}

TEST_F(EnclaveProtocolUtilsTest, ParseMakeCredentialResponseFailures) {
  CtapMakeCredentialRequest ctap_request(
      kClientDataJson, PublicKeyCredentialRpEntity(kRpId),
      PublicKeyCredentialUserEntity(user_id()),
      PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>()));

  for (auto& test_case : kFailingMakeCredentialResponses) {
    absl::optional<AuthenticatorMakeCredentialResponse> response;
    absl::optional<sync_pb::WebauthnCredentialSpecifics> entity;
    std::string error_string;

    std::vector<uint8_t> response_serialized;
    CHECK(base::HexStringToBytes(test_case.hex_cbor, &response_serialized));
    std::tie(response, entity, error_string) =
        ParseMakeCredentialResponse(response_serialized, ctap_request);
    bool pass = (response == absl::nullopt) && (entity == absl::nullopt) &&
                (!error_string.empty());
    EXPECT_TRUE(pass) << "Failed MakeCredential response parsing for: "
                      << test_case.name;
  }
}

}  // namespace enclave
}  // namespace device
