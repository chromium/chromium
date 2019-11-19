// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_sign_operation.h"

#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "crypto/ec_private_key.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_u2f_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;
using ::testing::InSequence;

namespace {

using TestSignCallback = ::device::test::StatusAndValueCallbackReceiver<
    CtapDeviceResponseCode,
    base::Optional<AuthenticatorGetAssertionResponse>>;

}  // namespace

class U2fSignOperationTest : public ::testing::Test {
 public:
  CtapGetAssertionRequest CreateSignRequest(
      std::vector<std::vector<uint8_t>> key_handles) {
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataJson);

    for (auto& key_handle : key_handles) {
      request.allow_list.emplace_back(CredentialType::kPublicKey,
                                      std::move(key_handle));
    }
    return request;
  }

  TestSignCallback& sign_callback_receiver() { return sign_callback_receiver_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestSignCallback sign_callback_receiver_;
};

TEST_F(U2fSignOperationTest, SignSuccess) {
  auto request = CreateSignRequest(
      {fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)});

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();
  InSequence s;
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            sign_callback_receiver().status());
  EXPECT_THAT(sign_callback_receiver().value()->signature(),
              ::testing::ElementsAreArray(test_data::kU2fSignature));
  EXPECT_THAT(sign_callback_receiver().value()->raw_credential_id(),
              ::testing::ElementsAreArray(test_data::kU2fSignKeyHandle));
}

TEST_F(U2fSignOperationTest, SignSuccessWithFakeDevice) {
  auto private_key = crypto::ECPrivateKey::Create();
  std::string public_key;
  private_key->ExportRawPublicKey(&public_key);
  auto hash = fido_parsing_utils::CreateSHA256Hash(public_key);
  std::vector<uint8_t> key_handle(hash.begin(), hash.end());
  auto request = CreateSignRequest({key_handle});

  auto device = std::make_unique<VirtualU2fDevice>();
  device->mutable_state()->registrations.emplace(
      key_handle, VirtualFidoDevice::RegistrationData(
                      std::move(private_key), test_data::kApplicationParameter,
                      42 /* counter */));

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            sign_callback_receiver().status());

  // Just a sanity check, we don't verify the actual signature.
  ASSERT_GE(32u + 1u + 4u + 8u,  // Minimal ECDSA signature is 8 bytes
            sign_callback_receiver()
                .value()
                ->auth_data()
                .SerializeToByteArray()
                .size());
  EXPECT_EQ(0x01,
            sign_callback_receiver()
                .value()
                ->auth_data()
                .SerializeToByteArray()[32]);  // UP flag
  // Counter is incremented for every sign request so this counter should have
  // been incremented once.
  EXPECT_EQ(43, sign_callback_receiver()
                    .value()
                    ->auth_data()
                    .SerializeToByteArray()[36]);  // counter
}

TEST_F(U2fSignOperationTest, DelayedSuccess) {
  auto request = CreateSignRequest(
      {fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)});

  // Simulates a device that times out waiting for user touch once before
  // responding successfully.
  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();

  InSequence s;
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kU2fConditionNotSatisfiedApduResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            sign_callback_receiver().status());
  EXPECT_THAT(sign_callback_receiver().value()->signature(),
              ::testing::ElementsAreArray(test_data::kU2fSignature));
  EXPECT_THAT(sign_callback_receiver().value()->raw_credential_id(),
              ::testing::ElementsAreArray(test_data::kU2fSignKeyHandle));
}

TEST_F(U2fSignOperationTest, MultipleHandles) {
  // Two wrong keys followed by a correct key ensuring the wrong keys will be
  // tested first.
  auto request = CreateSignRequest(
      {fido_parsing_utils::Materialize(test_data::kKeyHandleAlpha),
       fido_parsing_utils::Materialize(test_data::kKeyHandleBeta),
       fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)});

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();
  InSequence s;
  // Wrong key would respond with SW_WRONG_DATA.
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithKeyAlpha,
      test_data::kU2fWrongDataApduResponse);
  device->ExpectRequestAndRespondWith(test_data::kU2fSignCommandApduWithKeyBeta,
                                      test_data::kU2fWrongDataApduResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            sign_callback_receiver().status());
  EXPECT_THAT(sign_callback_receiver().value()->signature(),
              ::testing::ElementsAreArray(test_data::kU2fSignature));
  EXPECT_THAT(sign_callback_receiver().value()->raw_credential_id(),
              ::testing::ElementsAreArray(test_data::kU2fSignKeyHandle));
}

TEST_F(U2fSignOperationTest, MultipleHandlesLengthError) {
  // One wrong key that responds with key handle length followed by a correct
  // key.
  auto request = CreateSignRequest(
      {fido_parsing_utils::Materialize(test_data::kKeyHandleAlpha),
       fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)});

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();
  InSequence s;

  // Wrong key would respond with the key handle length.
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithKeyAlpha,
      test_data::kU2fKeyHandleSizeApduResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();

  sign_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            sign_callback_receiver().status());
  EXPECT_THAT(sign_callback_receiver().value()->signature(),
              ::testing::ElementsAreArray(test_data::kU2fSignature));
  EXPECT_THAT(sign_callback_receiver().value()->raw_credential_id(),
              ::testing::ElementsAreArray(test_data::kU2fSignKeyHandle));
}

// Test that Fake U2F registration is invoked when no credentials in the allowed
// list are recognized by the device.
TEST_F(U2fSignOperationTest, FakeEnroll) {
  auto request = CreateSignRequest(
      {fido_parsing_utils::Materialize(test_data::kKeyHandleAlpha),
       fido_parsing_utils::Materialize(test_data::kKeyHandleBeta)});

  auto device = std::make_unique<MockFidoDevice>();
  device->ExpectWinkedAtLeastOnce();
  InSequence s;
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithKeyAlpha,
      test_data::kU2fWrongDataApduResponse);
  device->ExpectRequestAndRespondWith(test_data::kU2fSignCommandApduWithKeyBeta,
                                      test_data::kU2fWrongDataApduResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fFakeRegisterCommand,
      test_data::kApduEncodedNoErrorRegisterResponse);

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();
  sign_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
            sign_callback_receiver().status());
  EXPECT_FALSE(sign_callback_receiver().value());
}

// Tests that U2F fake enrollment should be re-tried repeatedly if no
// credentials are valid for the authenticator and user presence is not
// obtained.
TEST_F(U2fSignOperationTest, DelayedFakeEnrollment) {
  auto request = CreateSignRequest(
      {fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)});

  // Simulates a device that times out waiting for user presence during fake
  // enrollment.
  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device0"));
  device->ExpectWinkedAtLeastOnce();
  InSequence s;
  device->ExpectRequestAndRespondWith(test_data::kU2fSignCommandApdu,
                                      test_data::kU2fWrongDataApduResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fFakeRegisterCommand,
      test_data::kU2fConditionNotSatisfiedApduResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fFakeRegisterCommand,
      test_data::kApduEncodedNoErrorRegisterResponse);

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();
  sign_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
            sign_callback_receiver().status());
  EXPECT_FALSE(sign_callback_receiver().value());
}

// Tests that request is dropped gracefully if device returns error on all
// requests (including fake enrollment).
TEST_F(U2fSignOperationTest, FakeEnrollErroringOut) {
  auto request = CreateSignRequest(
      {fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)});

  // Simulates a device that errors out on all requests (including the sign
  // request and fake registration attempt). The device should then be abandoned
  // to prevent the test from crashing or timing out.
  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device0"));
  device->ExpectWinkedAtLeastOnce();
  InSequence s;
  device->ExpectRequestAndRespondWith(test_data::kU2fSignCommandApdu,
                                      test_data::kU2fWrongDataApduResponse);
  device->ExpectRequestAndRespondWith(test_data::kU2fFakeRegisterCommand,
                                      test_data::kU2fWrongDataApduResponse);

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();
  sign_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            sign_callback_receiver().status());
  EXPECT_FALSE(sign_callback_receiver().value());
}

// Tests the scenario where device returns success response, but the response is
// unparse-able.
TEST_F(U2fSignOperationTest, SignWithCorruptedResponse) {
  auto request = CreateSignRequest(
      {fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)});

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();
  InSequence s;
  device->ExpectRequestAndRespondWith(test_data::kU2fSignCommandApdu,
                                      test_data::kTestCorruptedU2fSignResponse);

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();
  sign_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            sign_callback_receiver().status());
  EXPECT_FALSE(sign_callback_receiver().value());
}

TEST_F(U2fSignOperationTest, AlternativeApplicationParameter) {
  auto request = CreateSignRequest(
      {fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)});
  request.app_id = test_data::kAppId;
  request.alternative_application_parameter =
      fido_parsing_utils::Materialize(base::span<const uint8_t, 32>(
          test_data::kAlternativeApplicationParameter));

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();
  InSequence s;
  // The first request will use the alternative app_param.
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithAlternativeApplicationParameter,
      test_data::kApduEncodedNoErrorSignResponse);

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();
  sign_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            sign_callback_receiver().status());
  const auto& response_value = sign_callback_receiver().value();
  EXPECT_THAT(response_value->signature(),
              ::testing::ElementsAreArray(test_data::kU2fSignature));
  EXPECT_THAT(response_value->raw_credential_id(),
              ::testing::ElementsAreArray(test_data::kU2fSignKeyHandle));
  EXPECT_THAT(response_value->GetRpIdHash(),
              ::testing::ElementsAreArray(base::span<const uint8_t, 32>(
                  test_data::kAlternativeApplicationParameter)));
}

// This is a regression test in response to https://crbug.com/833398.
TEST_F(U2fSignOperationTest, AlternativeApplicationParameterRejection) {
  auto request = CreateSignRequest(
      {fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)});
  request.app_id = test_data::kAppId;
  request.alternative_application_parameter =
      fido_parsing_utils::Materialize(base::span<const uint8_t, 32>(
          test_data::kAlternativeApplicationParameter));

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();
  InSequence s;
  // The first request will use the alternative app_param, which will be
  // rejected.
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithAlternativeApplicationParameter,
      test_data::kU2fWrongDataApduResponse);
  // After the rejection, request with primary application parameter should
  // be tried, which will also be rejected.
  device->ExpectRequestAndRespondWith(test_data::kU2fSignCommandApdu,
                                      test_data::kU2fWrongDataApduResponse);
  // The second rejection will trigger a bogus register command. This will be
  // rejected as well, triggering the device to be abandoned.
  device->ExpectRequestAndRespondWith(test_data::kU2fFakeRegisterCommand,
                                      test_data::kU2fWrongDataApduResponse);

  auto u2f_sign = std::make_unique<U2fSignOperation>(
      device.get(), std::move(request), sign_callback_receiver().callback());
  u2f_sign->Start();
  sign_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            sign_callback_receiver().status());
  EXPECT_FALSE(sign_callback_receiver().value());
}

}  // namespace device
