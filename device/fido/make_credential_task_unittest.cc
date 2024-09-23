// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_task.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/fido/authenticator_get_info_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_types.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_ctap2_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

constexpr std::array<uint8_t, kAaguidLength> kTestDeviceAaguid = {
    {0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17, 0x11,
     0x1F, 0x9E, 0xDC, 0x7D}};

using TestMakeCredentialTaskFuture = ::base::test::TestFuture<
    CtapDeviceResponseCode,
    std::optional<AuthenticatorMakeCredentialResponse>>;

class FidoMakeCredentialTaskTest : public testing::Test {
 public:
  FidoMakeCredentialTaskTest() = default;

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
        MakeCredentialOptions(), future_.GetCallback());
  }

  TestMakeCredentialTaskFuture& make_credential_future() { return future_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestMakeCredentialTaskFuture future_;
};

TEST_F(FidoMakeCredentialTaskTest, MakeCredentialSuccess) {
  auto device = MockFidoDevice::MakeCtap();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);

  const auto task = CreateMakeCredentialTask(device.get());
  EXPECT_TRUE(make_credential_future().Wait());

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(make_credential_future().Get()));
  EXPECT_TRUE(std::get<1>(make_credential_future().Get()));
  EXPECT_EQ(device->supported_protocol(), ProtocolVersion::kCtap2);
  EXPECT_TRUE(device->device_info());
}

TEST_F(FidoMakeCredentialTaskTest, TestRegisterSuccessWithFake) {
  auto device = std::make_unique<VirtualCtap2Device>();
  base::test::TestFuture<void> done_init;
  device->DiscoverSupportedProtocolAndDeviceInfo(done_init.GetCallback());
  EXPECT_TRUE(done_init.Wait());
  const auto task = CreateMakeCredentialTask(device.get());
  EXPECT_TRUE(make_credential_future().Wait());

  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(make_credential_future().Get()));

  // We don't verify the response from the fake, but do a quick sanity check.
  ASSERT_TRUE(std::get<1>(make_credential_future().Get()));
  EXPECT_EQ(32u, std::get<1>(make_credential_future().Get())
                     ->attestation_object.GetCredentialId()
                     .size());
}

TEST_F(FidoMakeCredentialTaskTest, FallbackToU2fRegisterSuccess) {
  auto device = MockFidoDevice::MakeU2f();
  device->ExpectWinkedAtLeastOnce();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);

  const auto task = CreateMakeCredentialTask(device.get());
  EXPECT_TRUE(make_credential_future().Wait());

  EXPECT_EQ(ProtocolVersion::kU2f, device->supported_protocol());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(make_credential_future().Get()));
}

TEST_F(FidoMakeCredentialTaskTest, DefaultToU2fWhenClientPinSet) {
  AuthenticatorGetInfoResponse device_info(
      {ProtocolVersion::kCtap2, ProtocolVersion::kU2f},
      {Ctap2Version::kCtap2_0}, kTestDeviceAaguid);
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
  EXPECT_TRUE(make_credential_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kSuccess,
            std::get<0>(make_credential_future().Get()));
  EXPECT_TRUE(std::get<1>(make_credential_future().Get()));
}

TEST_F(FidoMakeCredentialTaskTest, EnforceClientPinWhenUserVerificationSet) {
  AuthenticatorGetInfoResponse device_info(
      {ProtocolVersion::kCtap2, ProtocolVersion::kU2f},
      {Ctap2Version::kCtap2_0}, kTestDeviceAaguid);
  AuthenticatorSupportedOptions options;
  options.client_pin_availability =
      AuthenticatorSupportedOptions::ClientPinAvailability::kSupportedAndPinSet;
  device_info.options = std::move(options);

  auto device = MockFidoDevice::MakeCtap(std::move(device_info));
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential, std::nullopt);

  PublicKeyCredentialRpEntity rp(test_data::kRelyingPartyId);
  PublicKeyCredentialUserEntity user(
      fido_parsing_utils::Materialize(test_data::kUserId));
  auto request = CtapMakeCredentialRequest(
      test_data::kClientDataJson, std::move(rp), std::move(user),
      PublicKeyCredentialParams(
          std::vector<PublicKeyCredentialParams::CredentialInfo>(1)));
  request.user_verification = UserVerificationRequirement::kRequired;
  const auto task = std::make_unique<MakeCredentialTask>(
      device.get(), std::move(request), MakeCredentialOptions(),
      make_credential_future().GetCallback());

  EXPECT_TRUE(make_credential_future().Wait());
  EXPECT_EQ(CtapDeviceResponseCode::kCtap2ErrOther,
            std::get<0>(make_credential_future().Get()));
  EXPECT_FALSE(std::get<1>(make_credential_future().Get()));
}

}  // namespace
}  // namespace device
