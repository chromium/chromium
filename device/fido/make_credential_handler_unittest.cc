// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/authenticator_get_info_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/make_credential_request_handler.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#endif  // BUILDFLAG(IS_WIN)

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithoutArgs;

namespace device {

namespace {

using TestMakeCredentialRequestFuture =
    base::test::TestFuture<MakeCredentialStatus,
                           std::optional<AuthenticatorMakeCredentialResponse>,
                           const FidoAuthenticator*>;

}  // namespace

constexpr char kResponseTransportHistogram[] =
    "WebAuthentication.MakeCredentialResponseTransport";

class FidoMakeCredentialHandlerTest : public ::testing::Test {
 public:
  FidoMakeCredentialHandlerTest() {
    mock_adapter_ =
        base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
    BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  void ForgeDiscoveries() {
    discovery_ = fake_discovery_factory_->ForgeNextHidDiscovery();
    nfc_discovery_ = fake_discovery_factory_->ForgeNextNfcDiscovery();
    platform_discovery_ = fake_discovery_factory_->ForgeNextPlatformDiscovery();
  }

  std::unique_ptr<MakeCredentialRequestHandler> CreateMakeCredentialHandler(
      AuthenticatorSelectionCriteria authenticator_selection_criteria = {}) {
    ForgeDiscoveries();
    PublicKeyCredentialRpEntity rp(test_data::kRelyingPartyId);
    PublicKeyCredentialUserEntity user(
        fido_parsing_utils::Materialize(test_data::kUserId), "nia",
        std::nullopt);
    PublicKeyCredentialParams credential_params(
        std::vector<PublicKeyCredentialParams::CredentialInfo>(1));

    auto request_parameter = CtapMakeCredentialRequest(
        test_data::kClientDataJson, std::move(rp), std::move(user),
        std::move(credential_params));

    MakeCredentialOptions options(authenticator_selection_criteria);
    options.allow_skipping_pin_touch = true;

    auto handler = std::make_unique<MakeCredentialRequestHandler>(
        fake_discovery_factory_.get(),
        std::vector<std::unique_ptr<FidoDiscoveryBase>>(),
        supported_transports_, std::move(request_parameter), std::move(options),
        future_.GetCallback());
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
    if (base::Contains(transports, Transport::kNearFieldCommunication))
      nfc_discovery()->WaitForCallToStartAndSimulateSuccess();

    task_environment_.FastForwardUntilNoTasksRemain();
    EXPECT_FALSE(future().IsReady());

    if (!base::Contains(transports, Transport::kUsbHumanInterfaceDevice))
      EXPECT_FALSE(discovery()->is_start_requested());
    if (!base::Contains(transports, Transport::kNearFieldCommunication))
      EXPECT_FALSE(nfc_discovery()->is_start_requested());

    EXPECT_THAT(
        request_handler->transport_availability_info().available_transports,
        ::testing::UnorderedElementsAreArray(transports));
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  test::FakeFidoDiscovery* nfc_discovery() const { return nfc_discovery_; }
  TestMakeCredentialRequestFuture& future() { return future_; }

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
  raw_ptr<test::FakeFidoDiscovery, AcrossTasksDanglingUntriaged> discovery_;
  raw_ptr<test::FakeFidoDiscovery, AcrossTasksDanglingUntriaged> nfc_discovery_;
  raw_ptr<test::FakeFidoDiscovery, AcrossTasksDanglingUntriaged>
      platform_discovery_;
  scoped_refptr<::testing::NiceMock<MockBluetoothAdapter>> mock_adapter_;
  std::unique_ptr<MockFidoDevice> pending_mock_platform_device_;
  TestMakeCredentialRequestFuture future_;
  base::flat_set<FidoTransportProtocol> supported_transports_ = {
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      FidoTransportProtocol::kHybrid,
      FidoTransportProtocol::kNearFieldCommunication,
      FidoTransportProtocol::kInternal,
  };
};

TEST_F(FidoMakeCredentialHandlerTest, TransportAvailabilityInfo) {
  auto request_handler = CreateMakeCredentialHandler();

  EXPECT_EQ(request_handler->transport_availability_info().request_type,
            FidoRequestType::kMakeCredential);
}

TEST_F(FidoMakeCredentialHandlerTest, TransportAvailabilityInfoRk) {
  for (const auto rk : {ResidentKeyRequirement::kDiscouraged,
                        ResidentKeyRequirement::kPreferred,
                        ResidentKeyRequirement::kRequired}) {
    auto request_handler =
        CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
            AuthenticatorAttachment::kAny, rk,
            UserVerificationRequirement::kPreferred));
    EXPECT_EQ(
        request_handler->transport_availability_info().resident_key_requirement,
        rk);
  }
}

TEST_F(FidoMakeCredentialHandlerTest, TransportAvailabilityInfoIsInternalOnly) {
  {
    auto request_handler =
        CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
            AuthenticatorAttachment::kAny, ResidentKeyRequirement::kDiscouraged,
            UserVerificationRequirement::kPreferred));
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .request_is_internal_only);
  }
  {
    auto request_handler =
        CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
            AuthenticatorAttachment::kCrossPlatform,
            ResidentKeyRequirement::kDiscouraged,
            UserVerificationRequirement::kPreferred));
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .request_is_internal_only);
  }
  {
    auto request_handler =
        CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
            AuthenticatorAttachment::kPlatform,
            ResidentKeyRequirement::kDiscouraged,
            UserVerificationRequirement::kPreferred));
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .request_is_internal_only);
  }
}

TEST_F(FidoMakeCredentialHandlerTest, TestCtap2MakeCredential) {
  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);
  discovery()->AddDevice(std::move(device));

  EXPECT_TRUE(future().Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future().Get()));
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

  EXPECT_TRUE(future().Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future().Get()));
}

TEST_F(FidoMakeCredentialHandlerTest, U2fRegisterWithUserVerificationRequired) {
  auto request_handler =
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kDiscouraged,
          UserVerificationRequirement::kRequired));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fBogusRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MakeCredentialStatus::kAuthenticatorMissingUserVerification,
            std::get<0>(future().Get()));
}

TEST_F(FidoMakeCredentialHandlerTest, U2fRegisterWithResidentKeyRequirement) {
  auto request_handler =
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kRequired,
          UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fBogusRegisterCommandApdu,
      test_data::kApduEncodedNoErrorRegisterResponse);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MakeCredentialStatus::kAuthenticatorMissingResidentKeys,
            std::get<0>(future().Get()));
}

TEST_F(FidoMakeCredentialHandlerTest, UserVerificationRequirementNotMet) {
  auto request_handler =
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kDiscouraged,
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
            std::get<0>(future().Get()));
}

TEST_F(FidoMakeCredentialHandlerTest, CrossPlatformAttachment) {
  auto request_handler = CreateMakeCredentialHandler(
      AuthenticatorSelectionCriteria(AuthenticatorAttachment::kCrossPlatform,
                                     ResidentKeyRequirement::kDiscouraged,
                                     UserVerificationRequirement::kPreferred));

  // kHybrid is not enabled by default as it needs special setup in the
  // discovery factory.
  ExpectAllowedTransportsForRequestAre(request_handler.get(), {
    FidoTransportProtocol::kNearFieldCommunication,
#if BUILDFLAG(IS_CHROMEOS)
        // CrOS tries to instantiate a platform authenticator for cross-platform
        // requests if enabled via enterprise policy.
        FidoTransportProtocol::kInternal,
#endif
        FidoTransportProtocol::kUsbHumanInterfaceDevice
  });
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

  auto request_handler = CreateMakeCredentialHandler(
      AuthenticatorSelectionCriteria(AuthenticatorAttachment::kPlatform,
                                     ResidentKeyRequirement::kDiscouraged,
                                     UserVerificationRequirement::kRequired));

  ExpectAllowedTransportsForRequestAre(request_handler.get(),
                                       {FidoTransportProtocol::kInternal});
}

TEST_F(FidoMakeCredentialHandlerTest, ResidentKeyRequirementNotMet) {
  auto request_handler =
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kRequired,
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
            std::get<0>(future().Get()));
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

// Matches a CTAP command that is:
// * A valid make credential request,
// * if |is_uv| is true,
//   * with an options map present,
//   * and options.uv present and true.
// * if |is_uv_| is false,
//   * with an options map not present,
//   * or options.uv not present or false.
MATCHER_P(IsUvRequest, is_uv, "") {
  if (arg.empty() ||
      arg[0] != base::strict_cast<uint8_t>(
                    CtapRequestCommand::kAuthenticatorMakeCredential)) {
    *result_listener << "not make credential";
    return false;
  }

  base::span<const uint8_t> param_bytes(arg);
  param_bytes = param_bytes.subspan(1);
  const auto maybe_map = cbor::Reader::Read(param_bytes);
  if (!maybe_map || !maybe_map->is_map()) {
    *result_listener << "not a map";
    return false;
  }
  const auto& map = maybe_map->GetMap();

  const auto options_it = map.find(cbor::Value(7));
  if (options_it == map.end() || !options_it->second.is_map()) {
    return is_uv == false;
  }
  const auto& options = options_it->second.GetMap();

  const auto uv_it = options.find(cbor::Value("uv"));
  if (uv_it == options.end()) {
    return is_uv == false;
  }

  if (!uv_it->second.is_bool()) {
    *result_listener << "'uv' is not a boolean";
    return false;
  }

  return uv_it->second.GetBool() == is_uv;
}

ACTION_P(Reply, reply) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::OnceCallback<void(std::optional<std::vector<uint8_t>>)>
                 callback,
             std::vector<uint8_t> reply_bytes) {
            std::move(callback).Run(std::move(reply_bytes));
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
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kRequired,
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

  EXPECT_TRUE(future().Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future().Get()));
}

TEST_F(FidoMakeCredentialHandlerTest, ResidentKeyCancel) {
  // Create an internal-UV authenticator and trigger a resident-key
  // MakeCredential request. Ensure that a cancelation is received if the
  // request handler is deleted. When a user cancels, we don't want a stray
  // touch creating a resident key.
  auto request_handler =
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kRequired,
          UserVerificationRequirement::kRequired));

  auto delete_request_handler = [&request_handler]() {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
  auto request_handler = CreateMakeCredentialHandler(
      AuthenticatorSelectionCriteria(AuthenticatorAttachment::kCrossPlatform,
                                     ResidentKeyRequirement::kRequired,
                                     UserVerificationRequirement::kRequired));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);
  discovery()->AddDevice(std::move(device));

  EXPECT_TRUE(future().Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future().Get()));

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
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kPlatform, ResidentKeyRequirement::kRequired,
          UserVerificationRequirement::kRequired));

  EXPECT_TRUE(future().Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future().Get()));

  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(FidoTransportProtocol::kInternal));
}

// A platform authenticator is ignored for cross-platform requests.
TEST_F(FidoMakeCredentialHandlerTest,
       CrossPlatformAuthenticatorPretendingToBePlatform) {
  auto request_handler = CreateMakeCredentialHandler(
      AuthenticatorSelectionCriteria(AuthenticatorAttachment::kCrossPlatform,
                                     ResidentKeyRequirement::kDiscouraged,
                                     UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponsePlatformDevice);
#if BUILDFLAG(IS_CHROMEOS)
  // CrOS will dispatch to a platform authenticator and one can be
  // instantiated in such cases if enabled via enterprise policy.
  device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      CtapDeviceResponseCode::kCtap2ErrOperationDenied);
#endif
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_TRUE(future().IsReady());
#else
  EXPECT_FALSE(future().IsReady());
#endif
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
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kPlatform, ResidentKeyRequirement::kRequired,
          UserVerificationRequirement::kRequired));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(future().IsReady());
}

TEST_F(FidoMakeCredentialHandlerTest, SupportedTransportsAreOnlyNfc) {
  const base::flat_set<FidoTransportProtocol> kNfc = {
      FidoTransportProtocol::kNearFieldCommunication,
  };

  set_supported_transports(kNfc);
  auto request_handler = CreateMakeCredentialHandler(
      AuthenticatorSelectionCriteria(AuthenticatorAttachment::kCrossPlatform,
                                     ResidentKeyRequirement::kDiscouraged,
                                     UserVerificationRequirement::kPreferred));

  ExpectAllowedTransportsForRequestAre(request_handler.get(), kNfc);
}

TEST_F(FidoMakeCredentialHandlerTest, IncorrectRpIdHash) {
  auto request_handler =
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kDiscouraged,
          UserVerificationRequirement::kPreferred));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponseWithIncorrectRpIdHash);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(future().IsReady());
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
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kRequired,
          UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::make_unique<VirtualCtap2Device>(
      std::move(state), std::move(config)));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(future().Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future().Get()));
}

// Tests that MakeCredential request fails when asking to use resident keys with
// authenticators that do not support resident key.
TEST_F(FidoMakeCredentialHandlerTest,
       MakeCredentialFailsForIncompatibleResidentKeyOption) {
  auto device = std::make_unique<VirtualCtap2Device>();
  auto request_handler =
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kRequired,
          UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(MakeCredentialStatus::kAuthenticatorMissingResidentKeys,
            std::get<0>(future().Get()));
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

  auto request_handler = CreateMakeCredentialHandler(
      AuthenticatorSelectionCriteria(AuthenticatorAttachment::kPlatform,
                                     ResidentKeyRequirement::kDiscouraged,
                                     UserVerificationRequirement::kPreferred));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(future().IsReady());
  EXPECT_EQ(MakeCredentialStatus::kUserConsentDenied,
            std::get<0>(future().Get()));
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
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kDiscouraged,
          UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(future().IsReady());
  EXPECT_EQ(MakeCredentialStatus::kUserConsentDenied,
            std::get<0>(future().Get()));
}

TEST_F(FidoMakeCredentialHandlerTest,
       TestCrossPlatformAuthenticatorsForceUVWhenSupported) {
  const auto& test_info_response = test_data::kTestAuthenticatorGetInfoResponse;
  ASSERT_EQ(ReadCTAPGetInfoResponse(test_info_response)
                ->options.user_verification_availability,
            AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedAndConfigured);

  auto device =
      MockFidoDevice::MakeCtapWithGetInfoExpectation(test_info_response);
  device->SetDeviceTransport(FidoTransportProtocol::kUsbHumanInterfaceDevice);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse, base::TimeDelta(),
      IsUvRequest(true));

  auto request_handler =
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kDiscouraged,
          UserVerificationRequirement::kDiscouraged));
  discovery()->AddDevice(std::move(device));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  EXPECT_TRUE(future().Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future().Get()));
}

// If a device returns CTAP2_ERR_PIN_AUTH_INVALID, the request should complete
// with MakeCredentialStatus::kUserConsentDenied.
TEST_F(FidoMakeCredentialHandlerTest, TestRequestWithPinAuthInvalid) {
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid);

  auto request_handler =
      CreateMakeCredentialHandler(AuthenticatorSelectionCriteria(
          AuthenticatorAttachment::kAny, ResidentKeyRequirement::kDiscouraged,
          UserVerificationRequirement::kPreferred));

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(future().IsReady());
  EXPECT_EQ(MakeCredentialStatus::kUserConsentDenied,
            std::get<0>(future().Get()));
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
                base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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

  EXPECT_TRUE(future().Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future().Get()));
}

TEST_F(FidoMakeCredentialHandlerTest, PinUvAuthTokenPreTouchFailure) {
  VirtualCtap2Device::Config config;
  config.ctap2_versions = {Ctap2Version::kCtap2_1};
  config.pin_uv_auth_token_support = true;
  config.internal_uv_support = true;
  config.override_response_map[CtapRequestCommand::kAuthenticatorClientPin] =
      CtapDeviceResponseCode::kCtap2ErrOther;
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  state->fingerprints_enrolled = true;

  auto request_handler = CreateMakeCredentialHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::make_unique<VirtualCtap2Device>(
      std::move(state), std::move(config)));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(future().IsReady());
}

TEST_F(FidoMakeCredentialHandlerTest, ReportTransportMetric) {
  base::HistogramTester histograms;
  auto request_handler = CreateMakeCredentialHandler();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorMakeCredential,
      test_data::kTestMakeCredentialResponse);
  discovery()->AddDevice(std::move(device));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto nfc_device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  nfc_device->SetDeviceTransport(
      FidoTransportProtocol::kNearFieldCommunication);
  nfc_device->ExpectCtap2CommandAndDoNotRespond(
      CtapRequestCommand::kAuthenticatorMakeCredential);
  EXPECT_CALL(*nfc_device, Cancel(_));
  nfc_discovery()->AddDevice(std::move(nfc_device));
  nfc_discovery()->WaitForCallToStartAndSimulateSuccess();

  EXPECT_TRUE(future().Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future().Get()));
  histograms.ExpectUniqueSample(kResponseTransportHistogram,
                                FidoTransportProtocol::kUsbHumanInterfaceDevice,
                                1);
}

#if BUILDFLAG(IS_WIN)
TEST_F(FidoMakeCredentialHandlerTest, ReportTransportMetricWin) {
  FakeWinWebAuthnApi win_api;
  win_api.set_version(WEBAUTHN_API_VERSION_3);
  win_api.set_transport(WEBAUTHN_CTAP_TRANSPORT_BLE);
  WinWebAuthnApi::ScopedOverride win_webauthn_api_override(&win_api);
  base::HistogramTester histograms;
  fake_discovery_factory_->set_discover_win_webauthn_api_authenticator(true);
  auto request_handler = CreateMakeCredentialHandler();
  EXPECT_TRUE(future().Wait());
  EXPECT_EQ(MakeCredentialStatus::kSuccess, std::get<0>(future().Get()));
  histograms.ExpectUniqueSample(kResponseTransportHistogram,
                                FidoTransportProtocol::kBluetoothLowEnergy, 1);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace device
