// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "device/base/features.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

constexpr std::array<uint8_t, kAaguidLength> kTestDeviceAaguid = {
    {0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17, 0x11,
     0x1F, 0x9E, 0xDC, 0x7D}};

using TestMakeCredentialTaskCallback =
    ::device::test::StatusAndValueCallbackReceiver<
        CtapDeviceResponseCode,
        base::Optional<AuthenticatorMakeCredentialResponse>>;

class FidoMakeCredentialTaskTest : public testing::Test {
 public:
  FidoMakeCredentialTaskTest() {}

  std::unique_ptr<MakeCredentialTask> CreateMakeCredentialTask(
      FidoDevice* device) {
    PublicKeyCredentialRpEntity rp(test_data::kRelyingPartyId);
    PublicKeyCredentialUserEntity user(
        fido_parsing_utils::Materialize(test_data::kUserId));
    return std::make_unique<MakeCredentialTask>(
        device,
        CtapMakeCredentialRequest(
            test_data::kClientDataJson, std::move(rp), std::move(user),
            PublicKeyCredentialParams(
                std::vector<PublicKeyCredentialParams::CredentialInfo>(1))),
        callback_receiver_.callback());
  }

  TestMakeCredentialTaskCallback& make_credential_callback_receiver() {
    return callback_receiver_;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestMakeCredentialTaskCallback callback_receiver_;
};

TEST_F(FidoMakeCredentialTaskTest, MakeCredentialSuccess) {
  auto device = MockFidoDevice::MakeCtap();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);

  const auto task = CreateMakeCredentialTask(device.get());
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            make_credential_callback_receiver().status());
  EXPECT_TRUE(make_credential_callback_receiver().value());
  EXPECT_EQ(device->supported_protocol(), ProtocolVersion::kCtap2);
  EXPECT_TRUE(device->device_info());
}

TEST_F(FidoMakeCredentialTaskTest, TestRegisterSuccessWithFake) {
  auto device = std::make_unique<VirtualCtap2Device>();
  test::TestCallbackReceiver<> done_init;
  device->DiscoverSupportedProtocolAndDeviceInfo(done_init.callback());
  done_init.WaitForCallback();
  const auto task = CreateMakeCredentialTask(device.get());
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            make_credential_callback_receiver().status());

  // We don't verify the response from the fake, but do a quick sanity check.
  ASSERT_TRUE(make_credential_callback_receiver().value());
  EXPECT_EQ(
      32u,
      make_credential_callback_receiver().value()->raw_credential_id().size());
}

TEST_F(FidoMakeCredentialTaskTest, FallbackToU2fRegisterSuccess) {
  auto device = MockFidoDevice::MakeU2f();
  device->ExpectWinkedAtLeastOnce();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);

  const auto task = CreateMakeCredentialTask(device.get());
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(ProtocolVersion::kU2f, device->supported_protocol());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            make_credential_callback_receiver().status());
}

TEST_F(FidoMakeCredentialTaskTest, DefaultToU2fWhenClientPinSet) {
  AuthenticatorGetInfoResponse device_info(
      {ProtocolVersion::kCtap2, ProtocolVersion::kU2f}, kTestDeviceAaguid);
  AuthenticatorSupportedOptions options;
  options.client_pin_availability =
      AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet;
  device_info.options = std::move(options);

  auto device = MockFidoDevice::MakeCtap(std::move(device_info));
  device->ExpectWinkedAtLeastOnce();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);

  const auto task = CreateMakeCredentialTask(device.get());
  make_credential_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            make_credential_callback_receiver().status());
  EXPECT_TRUE(make_credential_callback_receiver().value());
}

TEST_F(FidoMakeCredentialTaskTest, EnforceClientPinWhenUserVerificationSet) {
  AuthenticatorGetInfoResponse device_info(
      {ProtocolVersion::kCtap2, ProtocolVersion::kU2f}, kTestDeviceAaguid);
  AuthenticatorSupportedOptions options;
  options.client_pin_availability =
      AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet;
  device_info.options = std::move(options);

  auto device = MockFidoDevice::MakeCtap(std::move(device_info));
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential, base::nullopt);

  PublicKeyCredentialRpEntity rp(test_data::kRelyingPartyId);
  PublicKeyCredentialUserEntity user(
      fido_parsing_utils::Materialize(test_data::kUserId));
  auto request = CtapMakeCredentialRequest(
      test_data::kClientDataJson, std::move(rp), std::move(user),
      PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>(1)));
  request.user_verification = UserVerificationRequirement::kRequired;
  const auto task = std::make_unique<MakeCredentialTask>(
      device.get(), std::move(request), callback_receiver_.callback());

  make_credential_callback_receiver().WaitForCallback();
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            make_credential_callback_receiver().status());
  EXPECT_FALSE(make_credential_callback_receiver().value());
}

TEST_F(FidoMakeCredentialTaskTest, TestU2fOnly) {
  // Regardless of the device's supported protocol, it should receive a U2F
  // request, because the task is instantiated in U2F-only mode.
  auto device = MockFidoDevice::MakeCtap();

  device->ExpectWinkedAtLeastOnce();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);

  PublicKeyCredentialRpEntity rp(test_data::kRelyingPartyId);
  PublicKeyCredentialUserEntity user(
      fido_parsing_utils::Materialize(test_data::kUserId));
  auto request = CtapMakeCredentialRequest(
      test_data::kClientDataJson, std::move(rp), std::move(user),
      PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>(1)));
  request.is_u2f_only = true;
  const auto task = std::make_unique<MakeCredentialTask>(
      device.get(), std::move(request), callback_receiver_.callback());
  make_credential_callback_receiver().WaitForCallback();

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            make_credential_callback_receiver().status());
  EXPECT_TRUE(make_credential_callback_receiver().value());
}

}  // namespace
}  // namespace device
