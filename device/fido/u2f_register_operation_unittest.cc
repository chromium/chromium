// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/u2f_register_operation.h"

#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_types.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_u2f_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;

namespace {

// Creates a CtapMakeCredentialRequest with given |registered_keys| as
// exclude list.
CtapMakeCredentialRequest CreateRegisterRequestWithRegisteredKeys(
    std::vector<PublicKeyCredentialDescriptor> registered_keys,
    bool is_individual_attestation = false) {
  PublicKeyCredentialRpEntity rp(test_data::kRelyingPartyId);
  PublicKeyCredentialUserEntity user(
      fido_parsing_utils::Materialize(test_data::kUserId));

  CtapMakeCredentialRequest request(
      test_data::kClientDataJson, std::move(rp), std::move(user),
      PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>(1)));
  request.exclude_list = std::move(registered_keys);
  if (is_individual_attestation) {
    request.attestation_preference =
        AttestationConveyancePreference::kEnterpriseApprovedByBrowser;
  }

  return request;
}

// Creates a CtapMakeCredentialRequest with an empty exclude list.
CtapMakeCredentialRequest CreateRegisterRequest(
    bool is_individual_attestation = false) {
  return CreateRegisterRequestWithRegisteredKeys(
      std::vector<PublicKeyCredentialDescriptor>(), is_individual_attestation);
}

using TestRegisterFuture =
    base::test::TestFuture<CtapDeviceResponseCode,
                           std::optional<AuthenticatorMakeCredentialResponse>>;

}  // namespace

class U2fRegisterOperationTest : public ::testing::Test {
 public:
  TestRegisterFuture& register_future() { return register_future_; }

 private:
  base::test::TaskEnvironment task_environment_;
  TestRegisterFuture register_future_;
};

TEST_F(U2fRegisterOperationTest, TestRegisterSuccess) {
  auto request = CreateRegisterRequest();
  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);

  auto u2f_register = std::make_unique<U2fRegisterOperation>(
      device.get(), std::move(request), register_future().GetCallback());
  u2f_register->Start();
  EXPECT_TRUE(register_future().Wait());

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(register_future().Get()));
  ASSERT_TRUE(std::get<1>(register_future().Get()));
  EXPECT_THAT(std::get<1>(register_future().Get())
                  ->attestation_object.GetCredentialId(),
              ::testing::ElementsAreArray(test_data::kU2fSignKeyHandle));
}

TEST_F(U2fRegisterOperationTest, TestRegisterSuccessWithFake) {
  auto request = CreateRegisterRequest();

  auto device = std::make_unique<VirtualU2fDevice>();
  auto u2f_register = std::make_unique<U2fRegisterOperation>(
      device.get(), std::move(request), register_future().GetCallback());
  u2f_register->Start();
  EXPECT_TRUE(register_future().Wait());

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(register_future().Get()));
  // We don't verify the response from the fake, but do a quick sanity check.
  ASSERT_TRUE(std::get<1>(register_future().Get()));
  EXPECT_EQ(32ul, std::get<1>(register_future().Get())
                      ->attestation_object.GetCredentialId()
                      .size());
}

TEST_F(U2fRegisterOperationTest, TestDelayedSuccess) {
  auto request = CreateRegisterRequest();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();

  // Device error out once waiting for user presence before retrying.
  ::testing::InSequence s;
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kU2fConditionNotSatisfiedApduResponse);

  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);

  auto u2f_register = std::make_unique<U2fRegisterOperation>(
      device.get(), std::move(request), register_future().GetCallback());
  u2f_register->Start();
  EXPECT_TRUE(register_future().Wait());

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(register_future().Get()));
  ASSERT_TRUE(std::get<1>(register_future().Get()));
  EXPECT_THAT(std::get<1>(register_future().Get())
                  ->attestation_object.GetCredentialId(),
              ::testing::ElementsAreArray(test_data::kU2fSignKeyHandle));
}

// Tests a scenario where a single device is connected and registration call
// is received with two unknown key handles. We expect that two check
// only sign-in calls be processed before registration.
TEST_F(U2fRegisterOperationTest, TestRegistrationWithExclusionList) {
  auto request = CreateRegisterRequestWithRegisteredKeys(
      {PublicKeyCredentialDescriptor(
           CredentialType::kPublicKey,
           fido_parsing_utils::Materialize(test_data::kKeyHandleAlpha)),
       PublicKeyCredentialDescriptor(
           CredentialType::kPublicKey,
           fido_parsing_utils::Materialize(test_data::kKeyHandleBeta))});

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();
  // DeviceTransact() will be called three times including two sign-in calls
  // with bogus challenges and one registration call. For the first two calls,
  // device will invoke MockFidoDevice::WrongData/WrongLength as the
  // authenticator did not create the two key handles provided in the exclude
  // list. At the third call, MockFidoDevice::NoErrorRegister will be invoked
  // after registration.
  ::testing::InSequence s;
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithKeyAlphaAndBogusChallenge,
      test_data::kU2fWrongDataApduResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithKeyBetaAndBogusChallenge,
      test_data::kU2fWrongLengthApduResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);

  auto u2f_register = std::make_unique<U2fRegisterOperation>(
      device.get(), std::move(request), register_future().GetCallback());
  u2f_register->Start();
  EXPECT_TRUE(register_future().Wait());

  ASSERT_TRUE(std::get<1>(register_future().Get()));
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(register_future().Get()));
  EXPECT_THAT(std::get<1>(register_future().Get())
                  ->attestation_object.GetCredentialId(),
              ::testing::ElementsAreArray(test_data::kU2fSignKeyHandle));
}

// Tests a scenario where single device is connected and registration is
// called with a key in the exclude list that was created by this device. We
// assume that the duplicate key is the last key handle in the exclude list.
// Therefore, after duplicate key handle is found, the process is expected to
// terminate after calling bogus registration which checks for user presence.
TEST_F(U2fRegisterOperationTest, TestRegistrationWithDuplicateHandle) {
  // Simulate two unknown key handles followed by a duplicate key.
  auto request = CreateRegisterRequestWithRegisteredKeys(
      {PublicKeyCredentialDescriptor(
           CredentialType::kPublicKey,
           fido_parsing_utils::Materialize(test_data::kKeyHandleAlpha)),
       PublicKeyCredentialDescriptor(
           CredentialType::kPublicKey,
           fido_parsing_utils::Materialize(test_data::kKeyHandleBeta)),
       PublicKeyCredentialDescriptor(
           CredentialType::kPublicKey,
           fido_parsing_utils::Materialize(test_data::kKeyHandleGamma))});

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
  device->ExpectWinkedAtLeastOnce();
  // For three keys in exclude list, the first two keys will return
  // SW_WRONG_DATA and the final duplicate key handle will invoke
  // SW_NO_ERROR. This means user presence has already been collected, so the
  // request is concluded with Ctap2ErrCredentialExcluded.
  ::testing::InSequence s;
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithKeyAlphaAndBogusChallenge,
      test_data::kU2fWrongDataApduResponse);
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithKeyBetaAndBogusChallenge,
      test_data::kU2fWrongDataApduResponse);
  // The signature in the response is intentionally incorrect since nothing
  // should depend on it being correct.
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApduWithKeyGammaAndBogusChallenge,
      test_data::kApduEncodedNoErrorSignResponse);

  auto u2f_register = std::make_unique<U2fRegisterOperation>(
      device.get(), std::move(request), register_future().GetCallback());
  u2f_register->Start();
  EXPECT_TRUE(register_future().Wait());

  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrCredentialExcluded,
            std::get<0>(register_future().Get()));
  EXPECT_FALSE(std::get<1>(register_future().Get()));
}

MATCHER_P(IndicatesIndividualAttestation, expected, "") {
  return arg.size() > 2 && ((arg[2] & 0x80) == 0x80) == expected;
}

TEST_F(U2fRegisterOperationTest, TestIndividualAttestation) {
  // Test that the individual attestation flag is correctly reflected in the
  // resulting registration APDU.
  for (const auto& individual_attestation : {false, true}) {
    SCOPED_TRACE(individual_attestation);
    TestRegisterFuture future;
    auto request = CreateRegisterRequest(individual_attestation);

    auto device = std::make_unique<MockFidoDevice>();
    EXPECT_CALL(*device, GetId()).WillRepeatedly(::testing::Return("device"));
    device->ExpectWinkedAtLeastOnce();

    device->ExpectRequestAndRespondWith(
        individual_attestation
            ? test_data::kU2fRegisterCommandApduWithIndividualAttestation
            : test_data::kU2fRegisterCommandApdu,
        test_data::kApduEncodedNoErrorRegisterResponse);

    auto u2f_register = std::make_unique<U2fRegisterOperation>(
        device.get(), std::move(request), future.GetCallback());
    u2f_register->Start();
    EXPECT_TRUE(future.Wait());

    EXPECT_EQ(CtapDeviceResponseCode::kSuccess, std::get<0>(future.Get()));
    ASSERT_TRUE(std::get<1>(future.Get()));
    EXPECT_THAT(std::get<1>(future.Get())->attestation_object.GetCredentialId(),
                ::testing::ElementsAreArray(test_data::kU2fSignKeyHandle));
  }
}

}  // namespace device
