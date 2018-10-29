// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/make_credential_request_handler.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace device {

namespace {

using TestMakeCredentialRequestCallback = test::StatusAndValuesCallbackReceiver<
    FidoReturnCode,
    base::Optional<AuthenticatorMakeCredentialResponse>,
    FidoTransportProtocol>;

}  // namespace

class FidoMakeCredentialHandlerTest : public ::testing::Test {
 public:
  FidoMakeCredentialHandlerTest() {
    mock_adapter_ =
        base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
    BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  void ForgeDiscoveries() {
    discovery_ = scoped_fake_discovery_factory_.ForgeNextHidDiscovery();
    ble_discovery_ = scoped_fake_discovery_factory_.ForgeNextBleDiscovery();
    nfc_discovery_ = scoped_fake_discovery_factory_.ForgeNextNfcDiscovery();
  }

  std::unique_ptr<MakeCredentialRequestHandler> CreateMakeCredentialHandler() {
    return CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
        AuthenticatorSelectionCriteria());
  }

  std::unique_ptr<MakeCredentialRequestHandler>
  CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
      AuthenticatorSelectionCriteria authenticator_selection_criteria) {
    ForgeDiscoveries();
    PublicKeyCredentialRpEntity rp(test_data::kRelyingPartyId);
    PublicKeyCredentialUserEntity user(
        fido_parsing_utils::Materialize(test_data::kUserId));
    PublicKeyCredentialParams credential_params(
        std::vector<PublicKeyCredentialParams::CredentialInfo>(1));

    auto request_parameter = CtapMakeCredentialRequest(
        test_data::kClientDataHash, std::move(rp), std::move(user),
        std::move(credential_params));

    auto handler = std::make_unique<MakeCredentialRequestHandler>(
        nullptr, supported_transports_, std::move(request_parameter),
        std::move(authenticator_selection_criteria), cb_.callback());
    handler->SetPlatformAuthenticatorOrMarkUnavailable(
        CreatePlatformAuthenticator());
    return handler;
  }

  void ExpectAllowedTransportsForRequestAre(
      MakeCredentialRequestHandler* request_handler,
      base::flat_set<FidoTransportProtocol> transports) {
    using Transport = FidoTransportProtocol;
    if (base::ContainsKey(transports, Transport::kUsbHumanInterfaceDevice))
      discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::ContainsKey(transports, Transport::kBluetoothLowEnergy))
      ble_discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::ContainsKey(transports, Transport::kNearFieldCommunication))
      nfc_discovery()->WaitForCallToStartAndSimulateSuccess();

    scoped_task_environment_.FastForwardUntilNoTasksRemain();
    EXPECT_FALSE(callback().was_called());

    if (!base::ContainsKey(transports, Transport::kUsbHumanInterfaceDevice))
      EXPECT_FALSE(discovery()->is_start_requested());
    if (!base::ContainsKey(transports, Transport::kBluetoothLowEnergy))
      EXPECT_FALSE(ble_discovery()->is_start_requested());
    if (!base::ContainsKey(transports, Transport::kNearFieldCommunication))
      EXPECT_FALSE(nfc_discovery()->is_start_requested());

    EXPECT_THAT(
        request_handler->transport_availability_info().available_transports,
        ::testing::UnorderedElementsAreArray(transports));
  }

  void InitFeatureListAndDisableCtapFlag() {
    scoped_feature_list_.InitAndDisableFeature(kNewCtap2Device);
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  test::FakeFidoDiscovery* ble_discovery() const { return ble_discovery_; }
  test::FakeFidoDiscovery* nfc_discovery() const { return nfc_discovery_; }
  TestMakeCredentialRequestCallback& callback() { return cb_; }

  void set_mock_platform_device(std::unique_ptr<MockFidoDevice> device) {
    pending_mock_platform_device_ = std::move(device);
  }

  void set_supported_transports(
      base::flat_set<FidoTransportProtocol> transports) {
    supported_transports_ = std::move(transports);
  }

 protected:
  base::Optional<PlatformAuthenticatorInfo> CreatePlatformAuthenticator() {
    if (!pending_mock_platform_device_)
      return base::nullopt;
    return PlatformAuthenticatorInfo(
        std::make_unique<FidoDeviceAuthenticator>(
            std::move(pending_mock_platform_device_)),
        false /* has_recognized_mac_touch_id_credential_available */);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};
  test::ScopedFakeFidoDiscoveryFactory scoped_fake_discovery_factory_;
  test::FakeFidoDiscovery* discovery_;
  test::FakeFidoDiscovery* ble_discovery_;
  test::FakeFidoDiscovery* nfc_discovery_;
  scoped_refptr<::testing::NiceMock<MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<MockFidoDevice> pending_mock_platform_device_;
  TestMakeCredentialRequestCallback cb_;
  base::flat_set<FidoTransportProtocol> supported_transports_ =
      GetAllTransportProtocols();
};

TEST_F(FidoMakeCredentialHandlerTest, TransportAvailabilityInfo) {
  auto request_handler = CreateMakeCredentialHandler();

  EXPECT_EQ(FidoRequestHandlerBase::RequestType::kMakeCredential,
            request_handler->transport_availability_info().request_type);
  EXPECT_EQ(test_data::kRelyingPartyId,
            request_handler->transport_availability_info().rp_id);
}

TEST_F(FidoMakeCredentialHandlerTest, TestCtap2MakeCredentialWithFlagEnabled) {
  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);
  discovery()->AddDevice(std::move(device));

  callback().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
  EXPECT_TRUE(request_handler->is_complete());
}

// Test a scenario where the connected authenticator is a U2F device.
TEST_F(FidoMakeCredentialHandlerTest, TestU2fRegisterWithFlagEnabled) {
  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);
  discovery()->AddDevice(std::move(device));

  callback().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
  EXPECT_TRUE(request_handler->is_complete());
}

// Test a scenario where the connected authenticator is a U2F device using a
// logic that defaults to handling U2F devices.
TEST_F(FidoMakeCredentialHandlerTest, TestU2fRegisterWithoutFlagEnabled) {
  InitFeatureListAndDisableCtapFlag();
  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);

  discovery()->AddDevice(std::move(device));
  callback().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
  EXPECT_TRUE(request_handler->is_complete());
}

TEST_F(FidoMakeCredentialHandlerTest, U2fRegisterWithUserVerificationRequired) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
              false /* require_resident_key */,
              UserVerificationRequirement::kRequired));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(callback().was_called());
}

TEST_F(FidoMakeCredentialHandlerTest, U2fRegisterWithResidentKeyRequirement) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
              true /* require_resident_key */,
              UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(callback().was_called());
}

TEST_F(FidoMakeCredentialHandlerTest, UserVerificationRequirementNotMet) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
              false /* require_resident_key */,
              UserVerificationRequirement::kRequired));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutUvSupport);
  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(callback().was_called());
}

// TODO(crbug.com/873710): Platform authenticators are temporarily disabled if
// AuthenticatorAttachment is unset (kAny).
TEST_F(FidoMakeCredentialHandlerTest, AnyAttachment) {
  auto platform_device = MockFidoDevice::MakeCtap(
      ReadCTAPGetInfoResponse(test_data::kTestGetInfoResponsePlatformDevice));
  platform_device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  set_mock_platform_device(std::move(platform_device));
  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
              false /* require_resident_key */,
              UserVerificationRequirement::kPreferred));

  // MakeCredentialHandler will not dispatch the kAny request to the platform
  // authenticator since the request does not get dispatched through UI. Despite
  // setting a platform authenticator, the internal transport never becomes
  // available.
  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(callback().was_called());

  // kCloudAssistedBluetoothLowEnergy not yet supported for MakeCredential.
  ExpectAllowedTransportsForRequestAre(
      request_handler.get(), {FidoTransportProtocol::kBluetoothLowEnergy,
                              FidoTransportProtocol::kNearFieldCommunication,
                              FidoTransportProtocol::kUsbHumanInterfaceDevice});
}

TEST_F(FidoMakeCredentialHandlerTest, CrossPlatformAttachment) {
  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::
                  kCrossPlatform,
              false /* require_resident_key */,
              UserVerificationRequirement::kPreferred));

  // kCloudAssistedBluetoothLowEnergy not yet supported for MakeCredential.
  ExpectAllowedTransportsForRequestAre(
      request_handler.get(), {FidoTransportProtocol::kBluetoothLowEnergy,
                              FidoTransportProtocol::kNearFieldCommunication,
                              FidoTransportProtocol::kUsbHumanInterfaceDevice});
}

TEST_F(FidoMakeCredentialHandlerTest, PlatformAttachment) {
  // Add a platform device to trigger the transport actually becoming available.
  // We don't care about the result of the request.
  auto platform_device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponsePlatformDevice);
  platform_device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  platform_device->ExpectCtap2CommandAndDoNotRespond(
      CtapRequestCommand::kAuthenticatorMakeCredential);
  EXPECT_CALL(*platform_device, Cancel());
  set_mock_platform_device(std::move(platform_device));

  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::
                  kPlatform,
              false /* require_resident_key */,
              UserVerificationRequirement::kRequired));

  ExpectAllowedTransportsForRequestAre(request_handler.get(),
                                       {FidoTransportProtocol::kInternal});
}

TEST_F(FidoMakeCredentialHandlerTest, ResidentKeyRequirementNotMet) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
              true /* require_resident_key */,
              UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutResidentKeySupport);
  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(callback().was_called());
}

TEST_F(FidoMakeCredentialHandlerTest,
       AuthenticatorSelectionCriteriaSatisfiedByCrossPlatformDevice) {
  set_supported_transports({FidoTransportProtocol::kUsbHumanInterfaceDevice});
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::
                  kCrossPlatform,
              true /* require_resident_key */,
              UserVerificationRequirement::kRequired));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);
  discovery()->AddDevice(std::move(device));

  callback().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());

  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(
          FidoTransportProtocol::kUsbHumanInterfaceDevice));
}

TEST_F(FidoMakeCredentialHandlerTest,
       AuthenticatorSelectionCriteriaSatisfiedByPlatformDevice) {
  set_supported_transports({FidoTransportProtocol::kInternal});
  auto platform_device = MockFidoDevice::MakeCtap(
      ReadCTAPGetInfoResponse(test_data::kTestGetInfoResponsePlatformDevice));
  platform_device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  EXPECT_CALL(*platform_device, GetId())
      .WillRepeatedly(testing::Return("device0"));
  platform_device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestGetInfoResponsePlatformDevice);
  platform_device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);
  set_mock_platform_device(std::move(platform_device));

  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::
                  kPlatform,
              true /* require_resident_key */,
              UserVerificationRequirement::kRequired));

  callback().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());

  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(FidoTransportProtocol::kInternal));
}

// A cross-platform authenticator claiming to be a platform authenticator as per
// its GetInfo response is rejected.
TEST_F(FidoMakeCredentialHandlerTest,
       CrossPlatformAuthenticatorPretendingToBePlatform) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::
                  kCrossPlatform,
              false /* require_resident_key */,
              UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponsePlatformDevice);
  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(callback().was_called());
}

// A platform authenticator claiming to be a cross-platform authenticator as per
// its GetInfo response is rejected.
TEST_F(FidoMakeCredentialHandlerTest,
       PlatformAuthenticatorPretendingToBeCrossPlatform) {
  auto platform_device = MockFidoDevice::MakeCtap(
      ReadCTAPGetInfoResponse(test_data::kTestAuthenticatorGetInfoResponse));
  platform_device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  EXPECT_CALL(*platform_device, GetId())
      .WillRepeatedly(testing::Return("device0"));
  platform_device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  set_mock_platform_device(std::move(platform_device));

  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::
                  kPlatform,
              true /* require_resident_key */,
              UserVerificationRequirement::kRequired));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(callback().was_called());
}

TEST_F(FidoMakeCredentialHandlerTest, SupportedTransportsAreOnlyBleAndNfc) {
  const base::flat_set<FidoTransportProtocol> kBleAndNfc = {
      FidoTransportProtocol::kBluetoothLowEnergy,
      FidoTransportProtocol::kNearFieldCommunication,
  };

  set_supported_transports(kBleAndNfc);
  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::
                  kCrossPlatform,
              false /* require_resident_key */,
              UserVerificationRequirement::kPreferred));

  ExpectAllowedTransportsForRequestAre(request_handler.get(), kBleAndNfc);
}

TEST_F(FidoMakeCredentialHandlerTest, IncorrectRpIdHash) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
              false /* require_resident_key */,
              UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponseWithIncorrectRpIdHash);
  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(callback().was_called());
}

// Tests that only authenticators with resident key support will successfully
// process MakeCredential request when the relying party requires using resident
// keys in AuthenicatorSelectionCriteria.
TEST_F(FidoMakeCredentialHandlerTest,
       SuccessfulMakeCredentialWithResidentKeyOption) {
  auto device = std::make_unique<VirtualCtap2Device>();
  AuthenticatorSupportedOptions option;
  option.SetSupportsResidentKey(true);
  device->SetAuthenticatorSupportedOptions(std::move(option));

  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
              true /* require_resident_key */,
              UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_EQ(FidoReturnCode::kSuccess, callback().status());
}

// Tests that MakeCredential request fails when asking to use resident keys with
// authenticators that do not support resident key.
TEST_F(FidoMakeCredentialHandlerTest,
       MakeCredentialFailsForIncompatibleResidentKeyOption) {
  auto device = std::make_unique<VirtualCtap2Device>();
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
              true /* require_resident_key */,
              UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(callback().was_called());
}

// If a device with transport type kInternal returns a
// CTAP2_ERR_OPERATION_DENIED error, the request should complete with
// FidoReturnCode::kUserConsentDenied.
TEST_F(FidoMakeCredentialHandlerTest,
       TestRequestWithOperationDeniedErrorPlatform) {
  auto platform_device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponsePlatformDevice);
  platform_device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  platform_device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      CtapDeviceResponseCode::kCtap2ErrOperationDenied);
  set_mock_platform_device(std::move(platform_device));

  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::
                  kPlatform,
              false /* require_resident_key */,
              UserVerificationRequirement::kPreferred));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback().was_called());
  EXPECT_EQ(FidoReturnCode::kUserConsentDenied, callback().status());
}

// Like |TestRequestWithOperationDeniedErrorPlatform|, but with a
// cross-platform device.
TEST_F(FidoMakeCredentialHandlerTest,
       TestRequestWithOperationDeniedErrorCrossPlatform) {
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      CtapDeviceResponseCode::kCtap2ErrOperationDenied);

  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorSelectionCriteria::AuthenticatorAttachment::kAny,
              false /* require_resident_key */,
              UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  scoped_task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback().was_called());
  EXPECT_EQ(FidoReturnCode::kUserConsentDenied, callback().status());
}

}  // namespace device
