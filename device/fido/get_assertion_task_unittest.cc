// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_task.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/containers/to_array.h"
#include "base/containers/to_vector.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_get_info_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_request_common.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_types.h"
#include "device/fido/public/public_key_credential_descriptor.h"
#include "device/fido/virtual_ctap2_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {
namespace {

using TestGetAssertionTaskFuture =
    base::test::TestFuture<CtapDeviceResponseCode,
                           std::vector<AuthenticatorGetAssertionResponse>>;

class FidoGetAssertionTaskTest : public testing::Test {
 public:
  FidoGetAssertionTaskTest() = default;

  TestGetAssertionTaskFuture& get_assertion_future() { return future_; }

 private:
  base::test::TaskEnvironment task_environment_;
  TestGetAssertionTaskFuture future_;
};

TEST_F(FidoGetAssertionTaskTest, TestGetAssertionSuccess) {
  auto device = MockFidoDevice::MakeCtap();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  CtapGetAssertionRequest request_param(test_data::kRelyingPartyId,
                                        test_data::kClientDataJson);
  request_param.allow_list.emplace_back(
      CredentialType::kPublicKey,
      base::ToVector(test_data::kTestGetAssertionCredentialId));

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request_param), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_EQ(std::get<1>(get_assertion_future().Get()).size(), 1u);
}

TEST_F(FidoGetAssertionTaskTest, RedactLog) {
  // This is a base 64 encoded CTAP get assertion response with:
  // * largeBlobKey
  // * largeBlob extension
  // * PRF extension
  // All the values intended to be redacted are set to the string "secret".
  // (The response is not valid, but that's okay here.)
  constexpr char kTestCtapGetAssertionResponse[] =
      "AKUBomJpZFBP92OYWX3ztmRWuLsC74ndZHR5cGVqcHVibGljLWtleQJYJcRs74KtG1Rkd1kd"
      "AIsIdZ7D5tLstPOUdL/"
      "qaWmSXQO3HQAAAAADZnNlY3JldAdmc2VjcmV0CKJjcHJmomdlbmFibGVk9GdyZXN1bHRzoWV"
      "maXJzdGZzZWNyZXRpbGFyZ2VCbG9iZnNlY3JldA==";
  auto device = MockFidoDevice::MakeCtap();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      *base::Base64Decode(kTestCtapGetAssertionResponse));

  device_event_log::Initialize(/*max_entries=*/0);
  CtapGetAssertionRequest request_param(test_data::kRelyingPartyId,
                                        test_data::kClientDataJson);
  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request_param), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());
  EXPECT_TRUE(get_assertion_future().Wait());

  // Signature.
  std::string device_log = device_event_log::GetAsString(
      device_event_log::NEWEST_FIRST, /*format=*/"level",
      /*types=*/"fido",
      /*max_level=*/device_event_log::LOG_LEVEL_EVENT, /*max_events=*/0);
  EXPECT_THAT(device_log, testing::HasSubstr("3: \"[redacted]\""));
  // Large blob key.
  EXPECT_THAT(device_log, testing::HasSubstr("7: \"[redacted]\""));
  // PRF extension.
  EXPECT_THAT(
      device_log,
      testing::HasSubstr(
          "8: {\"prf\": {\"enabled\": false, \"results\": \"[redacted]\""));
  // Large blob extension.
  EXPECT_THAT(device_log, testing::HasSubstr("\"largeBlob\": \"[redacted]\""));

  // Verify that the data hasn't escaped redaction somehow.
  EXPECT_THAT(device_log, testing::Not(testing::HasSubstr("secret")));
  device_event_log::Shutdown();
}

TEST_F(FidoGetAssertionTaskTest, TestU2fSignSuccess) {
  auto device = MockFidoDevice::MakeU2f();
  device->ExpectWinkedAtLeastOnce();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  CtapGetAssertionRequest request_param(test_data::kRelyingPartyId,
                                        test_data::kClientDataJson);
  request_param.allow_list.emplace_back(
      CredentialType::kPublicKey, base::ToVector(test_data::kU2fSignKeyHandle));

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request_param), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_EQ(std::get<1>(get_assertion_future().Get()).size(), 1u);
}

TEST_F(FidoGetAssertionTaskTest, TestSignSuccessWithFake) {
  static const uint8_t kCredentialId[] = {1, 2, 3, 4};
  CtapGetAssertionRequest request_param(test_data::kRelyingPartyId,
                                        test_data::kClientDataJson);
  request_param.allow_list.emplace_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey, base::ToVector(kCredentialId)));

  auto device = std::make_unique<VirtualCtap2Device>();
  ASSERT_TRUE(device->mutable_state()->InjectRegistration(
      kCredentialId, test_data::kRelyingPartyId));
  base::test::TestFuture<void> done;
  device->DiscoverSupportedProtocolAndDeviceInfo(done.GetCallback());
  EXPECT_TRUE(done.Wait());

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request_param), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(get_assertion_future().Get()));

  // Just a sanity check, we don't verify the actual signature.
  ASSERT_GE(32u + 1u + 4u + 8u,  // Minimal ECDSA signature is 8 bytes
            std::get<1>(get_assertion_future().Get())
                .at(0)
                .authenticator_data.SerializeToByteArray()
                .size());
  EXPECT_EQ(0x01,
            std::get<1>(get_assertion_future().Get())
                .at(0)
                .authenticator_data.SerializeToByteArray()[32]);  // UP flag
  // Counter starts at zero and is incremented for every sign request.
  EXPECT_EQ(1, std::get<1>(get_assertion_future().Get())
                   .at(0)
                   .authenticator_data.SerializeToByteArray()[36]);  // counter
}

TEST_F(FidoGetAssertionTaskTest, TestIncorrectGetAssertionResponse) {
  auto device = MockFidoDevice::MakeCtap();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion, std::nullopt);

  auto task = std::make_unique<GetAssertionTask>(
      device.get(),
      CtapGetAssertionRequest(test_data::kRelyingPartyId,
                              test_data::kClientDataJson),
      CtapGetAssertionOptions(), get_assertion_future().GetCallback());

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_TRUE(std::get<1>(get_assertion_future().Get()).empty());
}

TEST_F(FidoGetAssertionTaskTest, TestU2fSignRequestWithEmptyAllowedList) {
  auto request = CtapGetAssertionRequest(test_data::kRelyingPartyId,
                                         test_data::kClientDataJson);

  auto device = MockFidoDevice::MakeU2f();
  device->ExpectWinkedAtLeastOnce();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fFakeRegisterCommand,
      test_data::kApduEncodedNoErrorSignResponse);

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_TRUE(std::get<1>(get_assertion_future().Get()).empty());
}

// Checks that when device supports both CTAP2 and U2F protocol and when
// appId extension parameter is present, the browser first checks presence
// of valid credentials via silent authentication.
TEST_F(FidoGetAssertionTaskTest, TestSilentSignInWhenAppIdExtensionPresent) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);

  std::vector<PublicKeyCredentialDescriptor> allowed_list;
  allowed_list.push_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      base::ToVector(test_data::kU2fSignKeyHandle)));
  request.app_id = test_data::kAppId;
  request.alternative_application_parameter =
      base::ToArray(test_data::kAlternativeApplicationParameter);
  request.allow_list = std::move(allowed_list);

  auto device = MockFidoDevice::MakeCtap();
  device->ExpectRequestAndRespondWith(test_data::kCtapSilentGetAssertionRequest,
                                      test_data::kTestGetAssertionResponse);
  device->ExpectRequestAndRespondWith(test_data::kCtapGetAssertionRequest,
                                      test_data::kTestGetAssertionResponse);

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(get_assertion_future().Get()));
}

TEST_F(FidoGetAssertionTaskTest, TestU2fFallbackForAppIdExtension) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);

  std::vector<PublicKeyCredentialDescriptor> allowed_list;
  allowed_list.push_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      base::ToVector(test_data::kU2fSignKeyHandle)));
  request.app_id = test_data::kAppId;
  request.alternative_application_parameter =
      base::ToArray(test_data::kAlternativeApplicationParameter);
  request.allow_list = std::move(allowed_list);

  ::testing::InSequence s;
  auto device = MockFidoDevice::MakeCtap();
  std::array<uint8_t, 1> error{{base::strict_cast<uint8_t>(
      CtapDeviceResponseCode::kCtap2ErrNoCredentials)}};
  // First, as the device supports both CTAP2 and U2F, the browser will attempt
  // a CTAP2 GetAssertion.
  device->ExpectRequestAndRespondWith(test_data::kCtapSilentGetAssertionRequest,
                                      error);
  // After falling back to U2F the request will use the alternative app_param,
  // which will be rejected.
  device->ExpectWinkedAtLeastOnce();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithAlternativeApplicationParameter,
      test_data::kU2fWrongDataApduResponse);
  // After the rejection, the U2F sign request with the primary application
  // parameter should be tried.
  device->ExpectWinkedAtLeastOnce();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());
  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(get_assertion_future().Get()));
}

TEST_F(FidoGetAssertionTaskTest, TestAvoidSilentSignInForCtapOnlyDevice) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);

  std::vector<PublicKeyCredentialDescriptor> allowed_list;
  allowed_list.push_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      base::ToVector(test_data::kU2fSignKeyHandle)));

  request.app_id = test_data::kAppId;
  request.alternative_application_parameter =
      base::ToArray(test_data::kAlternativeApplicationParameter);
  request.allow_list = std::move(allowed_list);

  auto device = MockFidoDevice::MakeCtap(ReadCTAPGetInfoResponse(
      test_data::kTestCtap2OnlyAuthenticatorGetInfoResponse));
  std::array<uint8_t, 1> error{
      {base::strict_cast<uint8_t>(CtapDeviceResponseCode::kCtap2ErrOther)}};
  device->ExpectRequestAndRespondWith(test_data::kCtapGetAssertionRequest,
                                      error);

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());
  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            std::get<0>(get_assertion_future().Get()));
}

// Create a CTAP2 mock device that batches allow lists one credential at a time.
std::unique_ptr<MockFidoDevice> MakeSingleCredentialBatchCtapDevice() {
  AuthenticatorGetInfoResponse device_info =
      ReadCTAPGetInfoResponse(
          test_data::kTestCtap2OnlyAuthenticatorGetInfoResponse)
          .value();
  device_info.max_credential_count_in_list = 1;
  device_info.max_credential_id_length = 64;
  return MockFidoDevice::MakeCtap(std::move(device_info));
}

// Probing silently should omit extensions while preserving PIN auth.
TEST_F(FidoGetAssertionTaskTest,
       SilentPreFlightOmitsExtensionsAndPreservesPinAuth) {
  const PublicKeyCredentialDescriptor unrecognized_cred(
      CredentialType::kPublicKey, {1, 2, 3, 4});
  const PublicKeyCredentialDescriptor matching_cred(
      CredentialType::kPublicKey,
      base::ToVector(test_data::kTestGetAssertionCredentialId));

  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);
  request.allow_list = {unrecognized_cred, matching_cred};
  request.pin_auth = std::vector<uint8_t>{0xaa, 0xbb, 0xcc, 0xdd};
  request.pin_protocol = PINUVAuthProtocol::kV2;
  request.large_blob_key = true;
  request.get_cred_blob = true;
  std::array<uint8_t, kP256X962Length> x962{};
  x962[0] = 0x04;
  request.hmac_secret.emplace(x962, std::vector<uint8_t>{1, 2, 3, 4},
                              std::vector<uint8_t>{5, 6, 7, 8}, std::nullopt);

  CtapGetAssertionRequest expected_silent_1(test_data::kRelyingPartyId,
                                            test_data::kClientDataJson);
  expected_silent_1.allow_list = {unrecognized_cred};
  expected_silent_1.user_presence_required = false;
  expected_silent_1.user_verification =
      UserVerificationRequirement::kDiscouraged;
  expected_silent_1.pin_auth = request.pin_auth;
  expected_silent_1.pin_protocol = request.pin_protocol;

  CtapGetAssertionRequest expected_silent_2 = expected_silent_1;
  expected_silent_2.allow_list = {matching_cred};

  CtapGetAssertionRequest expected_final = request;
  expected_final.allow_list = {matching_cred};

  auto device = MakeSingleCredentialBatchCtapDevice();
  const std::array<uint8_t, 1> kNoCredentialsError{base::strict_cast<uint8_t>(
      CtapDeviceResponseCode::kCtap2ErrNoCredentials)};
  {
    testing::InSequence s;
    device->ExpectRequestAndRespondWith(
        MockFidoDevice::EncodeCBORRequest(
            AsCTAPRequestValuePair(expected_silent_1)),
        kNoCredentialsError);
    device->ExpectRequestAndRespondWith(
        MockFidoDevice::EncodeCBORRequest(
            AsCTAPRequestValuePair(expected_silent_2)),
        test_data::kTestGetAssertionResponse);
    device->ExpectRequestAndRespondWith(
        MockFidoDevice::EncodeCBORRequest(
            AsCTAPRequestValuePair(expected_final)),
        test_data::kTestGetAssertionResponse);
  }

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_EQ(std::get<1>(get_assertion_future().Get()).size(), 1u);
}

// Probing without PIN auth should omit extensions, PIN protocol, and UV.
TEST_F(FidoGetAssertionTaskTest,
       SilentPreFlightWithoutPinAuthOmitsPinProtocolUvAndExtensions) {
  const PublicKeyCredentialDescriptor matching_cred(
      CredentialType::kPublicKey,
      base::ToVector(test_data::kTestGetAssertionCredentialId));
  const PublicKeyCredentialDescriptor other_cred(CredentialType::kPublicKey,
                                                 {1, 2, 3, 4});

  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  /*client_data_json=*/"");
  request.client_data_hash.fill(0x42);
  request.allow_list = {matching_cred, other_cred};
  request.user_verification = UserVerificationRequirement::kRequired;
  // Set pin_protocol without pin_auth, matching PRF without UV.
  request.pin_protocol = PINUVAuthProtocol::kV2;
  request.large_blob_extension_read = true;

  CtapGetAssertionRequest expected_silent(test_data::kRelyingPartyId,
                                          /*client_data_json=*/"");
  expected_silent.client_data_hash = request.client_data_hash;
  expected_silent.allow_list = {matching_cred};
  expected_silent.user_presence_required = false;
  expected_silent.user_verification = UserVerificationRequirement::kDiscouraged;

  CtapGetAssertionRequest expected_final = request;
  expected_final.allow_list = {matching_cred};

  auto device = MakeSingleCredentialBatchCtapDevice();
  {
    testing::InSequence s;
    device->ExpectRequestAndRespondWith(
        MockFidoDevice::EncodeCBORRequest(
            AsCTAPRequestValuePair(expected_silent)),
        test_data::kTestGetAssertionResponse);
    device->ExpectRequestAndRespondWith(
        MockFidoDevice::EncodeCBORRequest(
            AsCTAPRequestValuePair(expected_final)),
        test_data::kTestGetAssertionResponse);
  }

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_EQ(std::get<1>(get_assertion_future().Get()).size(), 1u);
}

}  // namespace
}  // namespace device
