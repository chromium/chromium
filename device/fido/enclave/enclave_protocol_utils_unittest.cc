// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_protocol_utils.h"

#include <optional>
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
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/json_request.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace device {
namespace {

constexpr uint8_t kHandshakeHash[32] = {
    0xac, 0xf0, 0xdf, 0xe4, 0x51, 0xe4, 0x6d, 0x77, 0xfc, 0x64, 0xc1,
    0x1f, 0xe9, 0x40, 0xc5, 0x00, 0x66, 0xd9, 0x0c, 0x54, 0xaf, 0x27,
    0xe9, 0xaa, 0x91, 0xe4, 0x4a, 0xd2, 0x72, 0x55, 0xbe, 0xd1};
constexpr uint8_t kDeviceId[] = "device0";
constexpr uint8_t kSignature[] = "signature";
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
    "81A1626F6BA168726573706F6E7365A3697369676E6174757265445369676E6A7573657248"
    "616E646C654261627161757468656E74696361746F724461746158251194228DA8FDBDEEFD"
    "261BD7B6595CFD70A50D70C6407BCF013DE96D4EFB17DE010000003B";
constexpr char kMakeCredentialHexResponse[] =
    "81A1626F6BA3677075625F6B657944050607086776657273696F6E0169656E637279707465"
    "644401020304";
constexpr int32_t kWrappedSecretVersion = 952;

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

void FakeSigningCallback(
    enclave::SignedMessage to_be_signed,
    base::OnceCallback<void(std::optional<enclave::ClientSignature>)>
        callback) {
  base::span<const uint8_t> message_span = to_be_signed;
  EXPECT_EQ(fido_parsing_utils::Materialize(message_span.first(32u)),
            fido_parsing_utils::Materialize(kHandshakeHash));

  enclave::ClientSignature ret;
  ret.device_id = fido_parsing_utils::Materialize(kDeviceId);
  ret.signature = fido_parsing_utils::Materialize(kSignature);
  ret.key_type = enclave::ClientKeyType::kHardware;
  std::move(callback).Run(std::move(ret));
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

  void CompletionCallback(std::optional<std::vector<uint8_t>> result) {
    result_ = std::move(*result);
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
    user_id_ = fido_parsing_utils::Materialize(kUserId);
    encrypted_passkey_ = fido_parsing_utils::Materialize(kEncryptedPasskey);
  }

  // This checks the outer map values of a request, which are common to all
  // request types.
  std::optional<cbor::Value> ValidateRequestFormatAndReturnCommandList(
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
    std::optional<cbor::Value> decoded_command =
        cbor::Reader::Read(encoded_request->second.GetBytestring());
    EXPECT_TRUE(decoded_command);
    EXPECT_TRUE(decoded_command->is_array());
    EXPECT_EQ(decoded_command->GetArray().size(), 1u);
    return decoded_command;
  }

  std::vector<uint8_t>& device_id() { return device_id_; }

  base::span<const uint8_t, 32> handshake_hash() { return kHandshakeHash; }

  std::vector<uint8_t>& user_id() { return user_id_; }

  std::vector<uint8_t>& encrypted_passkey() { return encrypted_passkey_; }

  std::vector<uint8_t> wrapped_secret() { return wrapped_secret_; }

 private:
  const std::vector<uint8_t> wrapped_secret_ = {1, 2, 3, 4, 5};
  std::vector<uint8_t> device_id_;
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
  std::optional<base::Value> parsed_json =
      base::JSONReader::Read(kGetAssertionRequestJson);
  EXPECT_TRUE(parsed_json);
  auto json_request =
      base::MakeRefCounted<JSONRequest>(std::move(*parsed_json));
  BuildCommandRequestBody(
      BuildGetAssertionCommand(std::move(entity), json_request, kClientDataJson,
                               /*claimed_pin=*/nullptr, wrapped_secret(),
                               /*secret=*/std::nullopt),
      base::BindOnce(&FakeSigningCallback), handshake_hash(),
      base::BindOnce(&BuildCommandCompletionWaiter::CompletionCallback,
                     base::Unretained(&waiter)));

  waiter.Wait();

  std::optional<cbor::Value> request_cbor = cbor::Reader::Read(waiter.result());
  auto decoded_command =
      ValidateRequestFormatAndReturnCommandList(*request_cbor);
  auto& command_element = decoded_command->GetArray()[0];
  auto& command_map = command_element.GetMap();
  EXPECT_EQ(command_map.find(cbor::Value("cmd"))->second.GetString(),
            "passkeys/assert");
  EXPECT_TRUE(command_map.find(cbor::Value("claimed_pin")) ==
              command_map.end());
  EXPECT_TRUE(command_map.find(cbor::Value("wrapped_pin_data")) ==
              command_map.end());
  EXPECT_EQ(
      command_map.find(cbor::Value("client_data_json"))->second.GetString(),
      kClientDataJson);
  auto& request_value_map =
      command_map.find(cbor::Value("request"))->second.GetMap();
  EXPECT_EQ(request_value_map.find(cbor::Value("rpId"))->second.GetString(),
            "test.example");

  auto& serialized_passkey_entity =
      command_map.find(cbor::Value("protobuf"))->second.GetBytestring();
  sync_pb::WebauthnCredentialSpecifics out_entity;
  EXPECT_TRUE(out_entity.ParseFromArray(serialized_passkey_entity.data(),
                                        serialized_passkey_entity.size()));
  EXPECT_EQ(out_entity.rp_id(), std::string(kRpId));
}

TEST_F(EnclaveProtocolUtilsTest, BuildGetAssertionRequest_WithPIN) {
  BuildCommandCompletionWaiter waiter;
  auto entity = PasskeyEntity();
  entity.set_rp_id(kRpId);
  std::optional<base::Value> parsed_json =
      base::JSONReader::Read(kGetAssertionRequestJson);
  EXPECT_TRUE(parsed_json);
  auto json_request =
      base::MakeRefCounted<JSONRequest>(std::move(*parsed_json));
  const std::vector<uint8_t> pin_claim = {1, 2, 3};
  const std::vector<uint8_t> wrapped_pin = {4, 5, 6, 7};
  auto claimed_pin = std::make_unique<ClaimedPIN>(pin_claim, wrapped_pin);
  BuildCommandRequestBody(
      BuildGetAssertionCommand(std::move(entity), json_request, kClientDataJson,
                               std::move(claimed_pin), wrapped_secret(),
                               /*secret=*/std::nullopt),
      base::BindOnce(&FakeSigningCallback), handshake_hash(),
      base::BindOnce(&BuildCommandCompletionWaiter::CompletionCallback,
                     base::Unretained(&waiter)));

  waiter.Wait();

  std::optional<cbor::Value> request_cbor = cbor::Reader::Read(waiter.result());
  auto decoded_command =
      ValidateRequestFormatAndReturnCommandList(*request_cbor);
  auto& command_element = decoded_command->GetArray()[0];
  auto& command_map = command_element.GetMap();

  EXPECT_EQ(command_map.find(cbor::Value("claimed_pin"))
                ->second.GetBytestring()
                .size(),
            3u);
  EXPECT_EQ(command_map.find(cbor::Value("wrapped_pin_data"))
                ->second.GetBytestring()
                .size(),
            4u);
}

TEST_F(EnclaveProtocolUtilsTest, BuildMakeCredentialRequest_Success) {
  BuildCommandCompletionWaiter waiter;
  std::optional<base::Value> parsed_json =
      base::JSONReader::Read(kMakeCredentialRequestJson);
  EXPECT_TRUE(parsed_json);
  auto json_request =
      base::MakeRefCounted<JSONRequest>(std::move(*parsed_json));
  BuildCommandRequestBody(
      BuildMakeCredentialCommand(json_request, /*claimed_pin=*/nullptr,
                                 wrapped_secret(), /*secret=*/std::nullopt),
      base::BindOnce(&FakeSigningCallback), handshake_hash(),
      base::BindOnce(&BuildCommandCompletionWaiter::CompletionCallback,
                     base::Unretained(&waiter)));

  waiter.Wait();

  std::optional<cbor::Value> request_cbor = cbor::Reader::Read(waiter.result());
  auto decoded_command =
      ValidateRequestFormatAndReturnCommandList(*request_cbor);
  auto& command_element = decoded_command->GetArray()[0];
  auto& command_map = command_element.GetMap();
  EXPECT_EQ(command_map.find(cbor::Value("cmd"))->second.GetString(),
            "passkeys/create");
  EXPECT_TRUE(command_map.find(cbor::Value("claimed_pin")) ==
              command_map.end());
  EXPECT_TRUE(command_map.find(cbor::Value("wrapped_pin_data")) ==
              command_map.end());
  auto& request_value_map =
      command_map.find(cbor::Value("request"))->second.GetMap();
  EXPECT_EQ(request_value_map.find(cbor::Value("rp"))
                ->second.GetMap()
                .find(cbor::Value("id"))
                ->second.GetString(),
            "test.example");
}

TEST_F(EnclaveProtocolUtilsTest, BuildMakeCredentialRequest_WithPIN) {
  BuildCommandCompletionWaiter waiter;
  std::optional<base::Value> parsed_json =
      base::JSONReader::Read(kMakeCredentialRequestJson);
  EXPECT_TRUE(parsed_json);
  auto json_request =
      base::MakeRefCounted<JSONRequest>(std::move(*parsed_json));
  const std::vector<uint8_t> pin_claim = {1, 2, 3};
  const std::vector<uint8_t> wrapped_pin = {4, 5, 6, 7};
  auto claimed_pin = std::make_unique<ClaimedPIN>(pin_claim, wrapped_pin);
  BuildCommandRequestBody(
      BuildMakeCredentialCommand(json_request, std::move(claimed_pin),
                                 wrapped_secret(), /*secret=*/std::nullopt),
      base::BindOnce(&FakeSigningCallback), handshake_hash(),
      base::BindOnce(&BuildCommandCompletionWaiter::CompletionCallback,
                     base::Unretained(&waiter)));

  waiter.Wait();

  std::optional<cbor::Value> request_cbor = cbor::Reader::Read(waiter.result());
  auto decoded_command =
      ValidateRequestFormatAndReturnCommandList(*request_cbor);
  auto& command_element = decoded_command->GetArray()[0];
  auto& command_map = command_element.GetMap();

  EXPECT_EQ(command_map.find(cbor::Value("claimed_pin"))
                ->second.GetBytestring()
                .size(),
            3u);
  EXPECT_EQ(command_map.find(cbor::Value("wrapped_pin_data"))
                ->second.GetBytestring()
                .size(),
            4u);
}

TEST_F(EnclaveProtocolUtilsTest, ParseGetAssertionResponse_Success) {
  std::vector<uint8_t> response_serialized;
  CHECK(base::HexStringToBytes(kGetAssertionHexResponse, &response_serialized));
  cbor::Value response_cbor = cbor::Reader::Read(response_serialized).value();
  std::vector<uint8_t> cred_id = {0, 1, 2};

  std::optional<AuthenticatorGetAssertionResponse> response;
  auto parse_result =
      ParseGetAssertionResponse(std::move(response_cbor), cred_id);
  EXPECT_TRUE(
      absl::holds_alternative<AuthenticatorGetAssertionResponse>(parse_result));
  const auto& assertion_response =
      absl::get<AuthenticatorGetAssertionResponse>(parse_result);
  EXPECT_EQ(assertion_response.user_entity->id,
            std::vector<uint8_t>({'a', 'b'}));
  EXPECT_EQ(assertion_response.credential->id, std::vector<uint8_t>({0, 1, 2}));
}

TEST_F(EnclaveProtocolUtilsTest, ParseGetAssertionResponse_Failures) {
  for (auto& test_case : kFailingGetAssertionResponses) {
    std::optional<AuthenticatorGetAssertionResponse> response;
    std::vector<uint8_t> response_serialized;
    CHECK(base::HexStringToBytes(test_case.hex_cbor, &response_serialized));
    cbor::Value response_cbor = cbor::Reader::Read(response_serialized).value();
    std::vector<uint8_t> cred_id = {0, 1, 2};
    auto parse_result =
        ParseGetAssertionResponse(std::move(response_cbor), cred_id);
    EXPECT_TRUE(absl::holds_alternative<ErrorResponse>(parse_result) &&
                absl::get<ErrorResponse>(parse_result).error_string.has_value())
        << "Failed GetAssertion response parsing for: " << test_case.name;
  }
}

TEST_F(EnclaveProtocolUtilsTest, ParseMakeCredentialResponse_Success) {
  std::vector<uint8_t> response_serialized;
  CHECK(
      base::HexStringToBytes(kMakeCredentialHexResponse, &response_serialized));
  cbor::Value response_cbor = cbor::Reader::Read(response_serialized).value();
  CtapMakeCredentialRequest ctap_request(
      kClientDataJson, PublicKeyCredentialRpEntity(kRpId),
      PublicKeyCredentialUserEntity(user_id()),
      PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>()));

  auto parse_result = ParseMakeCredentialResponse(
      std::move(response_cbor), ctap_request, kWrappedSecretVersion,
      /*user_verified=*/true);
  EXPECT_TRUE(
      (absl::holds_alternative<std::pair<AuthenticatorMakeCredentialResponse,
                                         sync_pb::WebauthnCredentialSpecifics>>(
          parse_result)));
  const auto& entity =
      absl::get<std::pair<AuthenticatorMakeCredentialResponse,
                          sync_pb::WebauthnCredentialSpecifics>>(parse_result)
          .second;
  EXPECT_EQ(entity.rp_id(), std::string(kRpId));
  EXPECT_EQ(entity.user_id(), std::string(user_id().begin(), user_id().end()));
  EXPECT_EQ(entity.key_version(), kWrappedSecretVersion);
  EXPECT_EQ(entity.encrypted(), std::string(encrypted_passkey().begin(),
                                            encrypted_passkey().end()));

  const auto& register_response =
      absl::get<std::pair<AuthenticatorMakeCredentialResponse,
                          sync_pb::WebauthnCredentialSpecifics>>(parse_result)
          .first;
  auto response_cred_id =
      register_response.attestation_object.authenticator_data()
          .GetCredentialId();
  EXPECT_EQ(entity.credential_id(),
            std::string(response_cred_id.begin(), response_cred_id.end()));
  EXPECT_TRUE(
      register_response.transports->contains(FidoTransportProtocol::kInternal));
  EXPECT_TRUE(
      register_response.transports->contains(FidoTransportProtocol::kHybrid));
  EXPECT_TRUE(register_response.is_resident_key);
  EXPECT_TRUE(register_response.attestation_object.authenticator_data()
                  .obtained_user_verification());
}

TEST_F(EnclaveProtocolUtilsTest, ParseMakeCredentialResponse_StringFailures) {
  CtapMakeCredentialRequest ctap_request(
      kClientDataJson, PublicKeyCredentialRpEntity(kRpId),
      PublicKeyCredentialUserEntity(user_id()),
      PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>()));

  for (auto& test_case : kFailingMakeCredentialResponses) {
    std::vector<uint8_t> response_serialized;
    CHECK(base::HexStringToBytes(test_case.hex_cbor, &response_serialized));
    cbor::Value response_cbor = cbor::Reader::Read(response_serialized).value();
    auto parse_result = ParseMakeCredentialResponse(
        std::move(response_cbor), ctap_request, kWrappedSecretVersion,
        /*user_verified=*/false);
    EXPECT_TRUE(absl::holds_alternative<ErrorResponse>(parse_result) &&
                absl::get<ErrorResponse>(parse_result).error_string.has_value())
        << "Failed MakeCredential response parsing for: " << test_case.name;
  }
}

TEST_F(EnclaveProtocolUtilsTest, ParseGetAssertionResponse_IntegerFailure) {
  std::vector<uint8_t> response_serialized;
  CHECK(base::HexStringToBytes("81A16365727202", &response_serialized));
  cbor::Value response_cbor = cbor::Reader::Read(response_serialized).value();
  std::vector<uint8_t> cred_id = {0, 1, 2};
  auto parse_result =
      ParseGetAssertionResponse(std::move(response_cbor), cred_id);

  EXPECT_TRUE(absl::holds_alternative<ErrorResponse>(parse_result));
  EXPECT_TRUE(absl::get<ErrorResponse>(parse_result).error_code.has_value());
  EXPECT_EQ(*absl::get<ErrorResponse>(parse_result).error_code, 2);
}

TEST_F(EnclaveProtocolUtilsTest, ParseMakeCredentialResponse_IntegerFailure) {
  CtapMakeCredentialRequest ctap_request(
      kClientDataJson, PublicKeyCredentialRpEntity(kRpId),
      PublicKeyCredentialUserEntity(user_id()),
      PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>()));

  std::vector<uint8_t> response_serialized;
  CHECK(base::HexStringToBytes("81A16365727202", &response_serialized));
  cbor::Value response_cbor = cbor::Reader::Read(response_serialized).value();
  auto parse_result = ParseMakeCredentialResponse(
      std::move(response_cbor), ctap_request, kWrappedSecretVersion,
      /*user_verified=*/false);

  EXPECT_TRUE(absl::holds_alternative<ErrorResponse>(parse_result));
  EXPECT_TRUE(absl::get<ErrorResponse>(parse_result).error_code.has_value());
  EXPECT_EQ(*absl::get<ErrorResponse>(parse_result).error_code, 2);
}

}  // namespace enclave
}  // namespace device
