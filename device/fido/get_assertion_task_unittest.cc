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

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_types.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/public_key_credential_descriptor.h"
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
      fido_parsing_utils::Materialize(
          test_data::kTestGetAssertionCredentialId));

  auto task = std::make_unique<GetAssertionTask>(
      device.get(), std::move(request_param), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_EQ(std::get<1>(get_assertion_future().Get()).size(), 1u);
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
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle));

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
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(kCredentialId)));

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
      device.get(), std::move(request), CtapGetAssertionOptions(),
      get_assertion_future().GetCallback());
  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            std::get<0>(get_assertion_future().Get()));
}

}  // namespace
}  // namespace device
