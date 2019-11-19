// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/get_assertion_task.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "crypto/ec_private_key.h"
#include "device/base/features.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {
namespace {

using TestGetAssertionTaskCallbackReceiver =
    ::device::test::StatusAndValueCallbackReceiver<
        CtapDeviceResponseCode,
        base::Optional<AuthenticatorGetAssertionResponse>>;

class FidoGetAssertionTaskTest : public testing::Test {
 public:
  FidoGetAssertionTaskTest() {}

  TestGetAssertionTaskCallbackReceiver& get_assertion_callback_receiver() {
    return cb_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  TestGetAssertionTaskCallbackReceiver cb_;
};

TEST_F(FidoGetAssertionTaskTest, TestGetAssertionSuccess) {
  auto device = MockFidoDevice::MakeCtap();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  CtapGetAssertionRequest request_param(test_data::kRelyingPartyId,
                                        test_data::kClientDataJson);
  request_param.allow_list.emplace_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(
          test_data::kTestGetAssertionCredentialId)));

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request_param),
      get_assertion_callback_receiver().callback());

  get_assertion_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            get_assertion_callback_receiver().status());
  EXPECT_TRUE(get_assertion_callback_receiver().value());
}

TEST_F(FidoGetAssertionTaskTest, TestU2fSignSuccess) {
  auto device = MockFidoDevice::MakeU2f();
  device->ExpectWinkedAtLeastOnce();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  CtapGetAssertionRequest request_param(test_data::kRelyingPartyId,
                                        test_data::kClientDataJson);
  request_param.allow_list.emplace_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)));

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request_param),
      get_assertion_callback_receiver().callback());

  get_assertion_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            get_assertion_callback_receiver().status());
  EXPECT_TRUE(get_assertion_callback_receiver().value());
}

TEST_F(FidoGetAssertionTaskTest, TestSignSuccessWithFake) {
  auto private_key = crypto::ECPrivateKey::Create();
  std::string public_key;
  private_key->ExportRawPublicKey(&public_key);
  auto hash = fido_parsing_utils::CreateSHA256Hash(public_key);
  std::vector<uint8_t> key_handle(hash.begin(), hash.end());
  CtapGetAssertionRequest request_param(test_data::kRelyingPartyId,
                                        test_data::kClientDataJson);
  request_param.allow_list.emplace_back(
      PublicKeyCredentialDescriptor(CredentialType::kPublicKey, key_handle));
  ;

  auto device = std::make_unique<VirtualCtap2Device>();
  device->mutable_state()->registrations.emplace(
      key_handle,
      VirtualFidoDevice::RegistrationData(
          std::move(private_key),
          fido_parsing_utils::CreateSHA256Hash(test_data::kRelyingPartyId),
          42 /* counter */));
  test::TestCallbackReceiver<> done;
  device->DiscoverSupportedProtocolAndDeviceInfo(done.callback());
  done.WaitForCallback();

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request_param),
      get_assertion_callback_receiver().callback());

  get_assertion_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            get_assertion_callback_receiver().status());

  // Just a sanity check, we don't verify the actual signature.
  ASSERT_GE(32u + 1u + 4u + 8u,  // Minimal ECDSA signature is 8 bytes
            get_assertion_callback_receiver()
                .value()
                ->auth_data()
                .SerializeToByteArray()
                .size());
  EXPECT_EQ(0x01,
            get_assertion_callback_receiver()
                .value()
                ->auth_data()
                .SerializeToByteArray()[32]);  // UP flag
  // Counter is incremented for every sign request.
  EXPECT_EQ(43, get_assertion_callback_receiver()
                    .value()
                    ->auth_data()
                    .SerializeToByteArray()[36]);  // counter
}

TEST_F(FidoGetAssertionTaskTest, TestIncorrectGetAssertionResponse) {
  auto device = MockFidoDevice::MakeCtap();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion, base::nullopt);

  auto task = std::make_unique<GetAssertionTask>(
      device.get(),
      CtapGetAssertionRequest(test_data::kRelyingPartyId,
                              test_data::kClientDataJson),
      get_assertion_callback_receiver().callback());

  get_assertion_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            get_assertion_callback_receiver().status());
  EXPECT_FALSE(get_assertion_callback_receiver().value());
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
      device.get(), std::move(request),
      get_assertion_callback_receiver().callback());

  get_assertion_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
            get_assertion_callback_receiver().status());
  EXPECT_FALSE(get_assertion_callback_receiver().value());
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
      fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)));
  request.app_id = test_data::kAppId;
  request.alternative_application_parameter =
      fido_parsing_utils::Materialize(base::span<const uint8_t, 32>(
          test_data::kAlternativeApplicationParameter));
  request.allow_list = std::move(allowed_list);

  auto device = MockFidoDevice::MakeCtap();
  device->ExpectRequestAndRespondWith(test_data::kCtapSilentGetAssertionRequest,
                                      test_data::kTestGetAssertionResponse);
  device->ExpectRequestAndRespondWith(test_data::kCtapGetAssertionRequest,
                                      test_data::kTestGetAssertionResponse);

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request),
      get_assertion_callback_receiver().callback());

  get_assertion_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            get_assertion_callback_receiver().status());
}

TEST_F(FidoGetAssertionTaskTest, TestU2fFallbackForAppIdExtension) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);

  std::vector<PublicKeyCredentialDescriptor> allowed_list;
  allowed_list.push_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)));
  request.app_id = test_data::kAppId;
  request.alternative_application_parameter =
      fido_parsing_utils::Materialize(base::span<const uint8_t, 32>(
          test_data::kAlternativeApplicationParameter));
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
      device.get(), std::move(request),
      get_assertion_callback_receiver().callback());
  get_assertion_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            get_assertion_callback_receiver().status());
}

TEST_F(FidoGetAssertionTaskTest, TestAvoidSilentSignInForCtapOnlyDevice) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);

  std::vector<PublicKeyCredentialDescriptor> allowed_list;
  allowed_list.push_back(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle)));

  request.app_id = test_data::kAppId;
  request.alternative_application_parameter =
      fido_parsing_utils::Materialize(base::span<const uint8_t, 32>(
          test_data::kAlternativeApplicationParameter));
  request.allow_list = std::move(allowed_list);

  auto device = MockFidoDevice::MakeCtap(ReadCTAPGetInfoResponse(
      test_data::kTestCtap2OnlyAuthenticatorGetInfoResponse));
  std::array<uint8_t, 1> error{
      {base::strict_cast<uint8_t>(CtapDeviceResponseCode::kCtap2ErrOther)}};
  device->ExpectRequestAndRespondWith(test_data::kCtapGetAssertionRequest,
                                      error);

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request),
      get_assertion_callback_receiver().callback());
  get_assertion_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            get_assertion_callback_receiver().status());
}

}  // namespace
}  // namespace device
