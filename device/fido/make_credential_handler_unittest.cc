// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
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
#include "device/fido/make_credential_task.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithoutArgs;

namespace device {

namespace {

using TestMakeCredentialRequestCallback = test::StatusAndValuesCallbackReceiver<
    MakeCredentialStatus,
    base::Optional<AuthenticatorMakeCredentialResponse>,
    const FidoAuthenticator*>;

}  // namespace

class FidoMakeCredentialHandlerTest : public ::testing::Test {
 public:
  FidoMakeCredentialHandlerTest() {
    mock_adapter_ =
        base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
    BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  void ForgeDiscoveries() {
    discovery_ = fake_discovery_factory_->ForgeNextHidDiscovery();
    ble_discovery_ = fake_discovery_factory_->ForgeNextBleDiscovery();
    nfc_discovery_ = fake_discovery_factory_->ForgeNextNfcDiscovery();
    platform_discovery_ = fake_discovery_factory_->ForgeNextPlatformDiscovery();
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
        test_data::kClientDataJson, std::move(rp), std::move(user),
        std::move(credential_params));

    auto handler = std::make_unique<MakeCredentialRequestHandler>(
        nullptr, fake_discovery_factory_.get(), supported_transports_,
        std::move(request_parameter),
        std::move(authenticator_selection_criteria),
        /*allow_skipping_pin_touch=*/true, cb_.callback());
    if (pending_mock_platform_device_) {
      platform_discovery_->AddDevice(std::move(pending_mock_platform_device_));
      platform_discovery_->WaitForCallToStartAndSimulateSuccess();
    }
    return handler;
  }

  void ExpectAllowedTransportsForRequestAre(
      MakeCredentialRequestHandler* request_handler,
      base::flat_set<FidoTransportProtocol> transports) {
    using Transport = FidoTransportProtocol;
    if (base::Contains(transports, Transport::kUsbHumanInterfaceDevice))
      discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::Contains(transports, Transport::kBluetoothLowEnergy))
      ble_discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::Contains(transports, Transport::kNearFieldCommunication))
      nfc_discovery()->WaitForCallToStartAndSimulateSuccess();

    task_environment_.FastForwardUntilNoTasksRemain();
    EXPECT_FALSE(callback().was_called());

    if (!base::Contains(transports, Transport::kUsbHumanInterfaceDevice))
      EXPECT_FALSE(discovery()->is_start_requested());
    if (!base::Contains(transports, Transport::kBluetoothLowEnergy))
      EXPECT_FALSE(ble_discovery()->is_start_requested());
    if (!base::Contains(transports, Transport::kNearFieldCommunication))
      EXPECT_FALSE(nfc_discovery()->is_start_requested());

    EXPECT_THAT(
        request_handler->transport_availability_info().available_transports,
        ::testing::UnorderedElementsAreArray(transports));
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
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<test::FakeFidoDiscoveryFactory> fake_discovery_factory_ =
      std::make_unique<test::FakeFidoDiscoveryFactory>();
  test::FakeFidoDiscovery* discovery_;
  test::FakeFidoDiscovery* ble_discovery_;
  test::FakeFidoDiscovery* nfc_discovery_;
  test::FakeFidoDiscovery* platform_discovery_;
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
}

TEST_F(FidoMakeCredentialHandlerTest, TestCtap2MakeCredential) {
  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);
  discovery()->AddDevice(std::move(device));

  callback().WaitForCallback();
  EXPECT_EQ(MakeCredentialStatus::kSuccess, callback().status());
}

// Test a scenario where the connected authenticator is a U2F device.
TEST_F(FidoMakeCredentialHandlerTest, TestU2fRegister) {
  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);
  discovery()->AddDevice(std::move(device));

  callback().WaitForCallback();
  EXPECT_EQ(MakeCredentialStatus::kSuccess, callback().status());
}

TEST_F(FidoMakeCredentialHandlerTest, U2fRegisterWithUserVerificationRequired) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kAny, /*require_resident_key=*/false,
              UserVerificationRequirement::kRequired));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fBogusRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MakeCredentialStatus::kAuthenticatorMissingUserVerification,
            callback().status());
}

TEST_F(FidoMakeCredentialHandlerTest, U2fRegisterWithResidentKeyRequirement) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kAny, /*require_resident_key=*/true,
              UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fBogusRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MakeCredentialStatus::kAuthenticatorMissingResidentKeys,
            callback().status());
}

TEST_F(FidoMakeCredentialHandlerTest, UserVerificationRequirementNotMet) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kAny, /*require_resident_key=*/false,
              UserVerificationRequirement::kRequired));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutUvSupport);
  device->ExpectRequestAndRespondWith(
      MockFidoDevice::EncodeCBORRequest(AsCTAPRequestValuePair(
          MakeCredentialTask::GetTouchRequest(device.get()))),
      test_data::kTestMakeCredentialResponse);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MakeCredentialStatus::kAuthenticatorMissingUserVerification,
            callback().status());
}

TEST_F(FidoMakeCredentialHandlerTest, CrossPlatformAttachment) {
  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kCrossPlatform,
              /*require_resident_key=*/false,
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
  EXPECT_CALL(*platform_device, Cancel(_));
  set_mock_platform_device(std::move(platform_device));

  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kPlatform,
              /*require_resident_key=*/false,
              UserVerificationRequirement::kRequired));

  ExpectAllowedTransportsForRequestAre(request_handler.get(),
                                       {FidoTransportProtocol::kInternal});
}

TEST_F(FidoMakeCredentialHandlerTest, ResidentKeyRequirementNotMet) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kAny, /*require_resident_key=*/true,
              UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutResidentKeySupport);
  device->ExpectRequestAndRespondWith(
      MockFidoDevice::EncodeCBORRequest(AsCTAPRequestValuePair(
          MakeCredentialTask::GetTouchRequest(device.get()))),
      test_data::kTestMakeCredentialResponse);

  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MakeCredentialStatus::kAuthenticatorMissingResidentKeys,
            callback().status());
}

MATCHER(IsResidentKeyRequest, "") {
  if (arg.empty() ||
      arg[0] != base::strict_cast<uint8_t>(
                    CtapRequestCommand::kAuthenticatorMakeCredential)) {
    return false;
  }

  base::span<const uint8_t> param_bytes(arg);
  param_bytes = param_bytes.subspan(1);
  const auto maybe_map = cbor::Reader::Read(param_bytes);
  if (!maybe_map || !maybe_map->is_map()) {
    return false;
  }
  const auto& map = maybe_map->GetMap();

  const auto options_it = map.find(cbor::Value(7));
  if (options_it == map.end() || !options_it->second.is_map()) {
    LOG(ERROR) << "options missing or not a map";
    return false;
  }
  const auto& options = options_it->second.GetMap();

  const auto rk_it = options.find(cbor::Value("rk"));
  if (rk_it == options.end() || !rk_it->second.is_bool() ||
      !rk_it->second.GetBool()) {
    LOG(ERROR) << "rk option missing, wrong type, or false";
    return false;
  }

  return true;
}

ACTION_P(Reply, reply) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceCallback<void(base::Optional<std::vector<uint8_t>>)>
                 callback,
             std::vector<uint8_t> reply) {
            std::move(callback).Run(std::move(reply));
          },
          std::move(arg1), std::vector<uint8_t>(reply.begin(), reply.end())));
}

TEST_F(FidoMakeCredentialHandlerTest, ResidentKeyCancelOtherAuthenticator) {
  // Create two internal-UV authenticators and trigger a resident-key
  // MakeCredential request which will go to both of them. Ensure that the other
  // is canceled when one completes. This is the scenario when cancelation is
  // most important: we don't want a stray touch to create a resident credential
  // on a second authenticator.
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kAny, /*require_resident_key=*/true,
              UserVerificationRequirement::kRequired));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device1 = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  auto device2 = MockFidoDevice::MakeCtapWithGetInfoExpectation();

  const FidoDevice::CancelToken token = 10;
  EXPECT_CALL(*device1, DeviceTransactPtr(IsResidentKeyRequest(), _))
      .WillOnce(Return(token));
  // The Cancel call should have the same CancelToken as was returned in the
  // previous line.
  EXPECT_CALL(*device1, Cancel(token));

  EXPECT_CALL(*device2, DeviceTransactPtr(IsResidentKeyRequest(), _))
      .WillOnce(DoAll(Reply(base::span<const uint8_t>(
                          test_data::kTestMakeCredentialResponse)),
                      Return(token + 1)));

  discovery()->AddDevice(std::move(device1));
  discovery()->AddDevice(std::move(device2));

  callback().WaitForCallback();
  EXPECT_EQ(MakeCredentialStatus::kSuccess, callback().status());
}

TEST_F(FidoMakeCredentialHandlerTest, ResidentKeyCancel) {
  // Create an internal-UV authenticator and trigger a resident-key
  // MakeCredential request. Ensure that a cancelation is received if the
  // request handler is deleted. When a user cancels, we don't want a stray
  // touch creating a resident key.
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kAny, /*require_resident_key=*/true,
              UserVerificationRequirement::kRequired));

  auto delete_request_handler = [&request_handler]() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<MakeCredentialRequestHandler>* unique_ptr) {
              unique_ptr->reset();
            },
            &request_handler));
  };

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  const FidoDevice::CancelToken token = 10;
  EXPECT_CALL(*device, DeviceTransactPtr(IsResidentKeyRequest(), _))
      .WillOnce(
          DoAll(WithoutArgs(Invoke(delete_request_handler)), Return(token)));
  EXPECT_CALL(*device, Cancel(token));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(request_handler);
}

TEST_F(FidoMakeCredentialHandlerTest,
       AuthenticatorSelectionCriteriaSatisfiedByCrossPlatformDevice) {
  set_supported_transports({FidoTransportProtocol::kUsbHumanInterfaceDevice});
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kCrossPlatform,
              /*require_resident_key=*/true,
              UserVerificationRequirement::kRequired));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);
  discovery()->AddDevice(std::move(device));

  callback().WaitForCallback();
  EXPECT_EQ(MakeCredentialStatus::kSuccess, callback().status());

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
              AuthenticatorAttachment::kPlatform,
              /*require_resident_key=*/true,
              UserVerificationRequirement::kRequired));

  callback().WaitForCallback();
  EXPECT_EQ(MakeCredentialStatus::kSuccess, callback().status());

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
              AuthenticatorAttachment::kCrossPlatform,
              /*require_resident_key=*/false,
              UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponsePlatformDevice);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
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
              AuthenticatorAttachment::kPlatform,
              /*require_resident_key=*/true,
              UserVerificationRequirement::kRequired));

  task_environment_.FastForwardUntilNoTasksRemain();
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
              AuthenticatorAttachment::kCrossPlatform,
              /*require_resident_key=*/false,
              UserVerificationRequirement::kPreferred));

  ExpectAllowedTransportsForRequestAre(request_handler.get(), kBleAndNfc);
}

TEST_F(FidoMakeCredentialHandlerTest, IncorrectRpIdHash) {
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kAny, /*require_resident_key=*/false,
              UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponseWithIncorrectRpIdHash);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(callback().was_called());
}

// Tests that only authenticators with resident key support will successfully
// process MakeCredential request when the relying party requires using resident
// keys in AuthenicatorSelectionCriteria.
TEST_F(FidoMakeCredentialHandlerTest,
       SuccessfulMakeCredentialWithResidentKeyOption) {
  VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  state->fingerprints_enrolled = true;

  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kAny, /*require_resident_key=*/true,
              UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::make_unique<VirtualCtap2Device>(
      std::move(state), std::move(config)));

  task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_EQ(MakeCredentialStatus::kSuccess, callback().status());
}

// Tests that MakeCredential request fails when asking to use resident keys with
// authenticators that do not support resident key.
TEST_F(FidoMakeCredentialHandlerTest,
       MakeCredentialFailsForIncompatibleResidentKeyOption) {
  auto device = std::make_unique<VirtualCtap2Device>();
  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kAny, /*require_resident_key=*/true,
              UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MakeCredentialStatus::kAuthenticatorMissingResidentKeys,
            callback().status());
}

// If a device with transport type kInternal returns a
// CTAP2_ERR_OPERATION_DENIED error, the request should complete with
// MakeCredentialStatus::kUserConsentDenied.
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
              AuthenticatorAttachment::kPlatform,
              /*require_resident_key=*/false,
              UserVerificationRequirement::kPreferred));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback().was_called());
  EXPECT_EQ(MakeCredentialStatus::kUserConsentDenied, callback().status());
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
              AuthenticatorAttachment::kAny, /*require_resident_key=*/false,
              UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback().was_called());
  EXPECT_EQ(MakeCredentialStatus::kUserConsentDenied, callback().status());
}

// If a device returns CTAP2_ERR_PIN_AUTH_INVALID, the request should complete
// with MakeCredentialStatus::kUserConsentDenied.
TEST_F(FidoMakeCredentialHandlerTest, TestRequestWithPinAuthInvalid) {
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid);

  auto request_handler =
      CreateMakeCredentialHandlerWithAuthenticatorSelectionCriteria(
          AuthenticatorSelectionCriteria(
              AuthenticatorAttachment::kAny, /*require_resident_key=*/false,
              UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback().was_called());
  EXPECT_EQ(MakeCredentialStatus::kUserConsentDenied, callback().status());
}

MATCHER_P(IsCtap2Command, expected_command, "") {
  return !arg.empty() && arg[0] == base::strict_cast<uint8_t>(expected_command);
}

TEST_F(FidoMakeCredentialHandlerTest, DeviceFailsImmediately) {
  // Test that, when a device immediately returns an unexpected error, the
  // request continues and waits for another device.

  auto broken_device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  EXPECT_CALL(
      *broken_device,
      DeviceTransactPtr(
          IsCtap2Command(CtapRequestCommand::kAuthenticatorMakeCredential), _))
      .WillOnce(::testing::DoAll(
          ::testing::WithArg<1>(
              ::testing::Invoke([this](FidoDevice::DeviceCallback& callback) {
                std::vector<uint8_t> response = {static_cast<uint8_t>(
                    CtapDeviceResponseCode::kCtap2ErrInvalidCBOR)};
                base::ThreadTaskRunnerHandle::Get()->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), std::move(response)));

                auto working_device =
                    MockFidoDevice::MakeCtapWithGetInfoExpectation();
                working_device->ExpectCtap2CommandAndRespondWith(
                    CtapRequestCommand::kAuthenticatorMakeCredential,
                    test_data::kTestMakeCredentialResponse);
                discovery()->AddDevice(std::move(working_device));
              })),
          ::testing::Return(0)));

  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(broken_device));

  callback().WaitForCallback();
  EXPECT_EQ(MakeCredentialStatus::kSuccess, callback().status());
}

}  // namespace device
