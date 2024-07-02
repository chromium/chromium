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
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/get_assertion_request_handler.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/u2f_command_constructor.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/win/fake_webauthn_api.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/dbus/u2f/u2f_client.h"
#endif

namespace device {

namespace {

constexpr uint8_t kBogusCredentialId[] = {0x01, 0x02, 0x03, 0x04};
constexpr char kRequestTransportHistogram[] =
    "WebAuthentication.GetAssertionRequestTransport";
constexpr char kResponseTransportHistogram[] =
    "WebAuthentication.GetAssertionResponseTransport";

using TestGetAssertionRequestFuture = base::test::TestFuture<
    GetAssertionStatus,
    std::optional<std::vector<AuthenticatorGetAssertionResponse>>,
    FidoAuthenticator*>;

}  // namespace

using testing::_;

// FidoGetAssertionHandlerTest allows testing GetAssertionRequestHandler against
// MockFidoDevices injected via a FakeFidoDiscoveryFactory.
class FidoGetAssertionHandlerTest : public ::testing::Test {
 public:
  void SetUp() override {
    bluetooth_config_->SetLESupported(true);
    BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

#if BUILDFLAG(IS_CHROMEOS)
    chromeos::U2FClient::InitializeFake();
#endif
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS)
    task_environment_.RunUntilIdle();
    chromeos::U2FClient::Shutdown();
#endif
  }

  void ForgeDiscoveries() {
    discovery_ = fake_discovery_factory_->ForgeNextHidDiscovery();
    cable_discovery_ = fake_discovery_factory_->ForgeNextCableDiscovery();
    nfc_discovery_ = fake_discovery_factory_->ForgeNextNfcDiscovery();
    platform_discovery_ = fake_discovery_factory_->ForgeNextPlatformDiscovery();
  }

  CtapGetAssertionRequest CreateTestRequestWithCableExtension() {
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataJson);
    request.cable_extension.emplace();
    return request;
  }

  std::unique_ptr<GetAssertionRequestHandler> CreateGetAssertionHandlerU2f() {
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataJson);
    request.allow_list = {PublicKeyCredentialDescriptor(
        CredentialType::kPublicKey,
        fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle))};
    return CreateGetAssertionHandlerWithRequest(std::move(request));
  }

  std::unique_ptr<GetAssertionRequestHandler> CreateGetAssertionHandlerCtap() {
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataJson);
    request.allow_list = {PublicKeyCredentialDescriptor(
        CredentialType::kPublicKey,
        fido_parsing_utils::Materialize(
            test_data::kTestGetAssertionCredentialId))};
    return CreateGetAssertionHandlerWithRequest(std::move(request));
  }

  std::unique_ptr<GetAssertionRequestHandler>
  CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest request) {
    ForgeDiscoveries();

    auto handler = std::make_unique<GetAssertionRequestHandler>(
        fake_discovery_factory_.get(),
        std::vector<std::unique_ptr<FidoDiscoveryBase>>(),
        supported_transports_, std::move(request), CtapGetAssertionOptions(),
        /*allow_skipping_pin_touch=*/true, get_assertion_future_.GetCallback());
    return handler;
  }

  std::unique_ptr<GetAssertionRequestHandler>
  CreateGetAssertionHandlerWithRequestedTransports(
      std::vector<std::vector<FidoTransportProtocol>> transports) {
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataJson);
    for (uint8_t i = 0; i < transports.size(); ++i) {
      request.allow_list.emplace_back(CredentialType::kPublicKey,
                                      std::vector<uint8_t>{i});
      request.allow_list.back().transports = transports[i];
    }
    return CreateGetAssertionHandlerWithRequest(std::move(request));
  }

  void ExpectAllowedTransportsForRequestAre(
      GetAssertionRequestHandler* request_handler,
      base::flat_set<FidoTransportProtocol> transports) {
    using Transport = FidoTransportProtocol;
    if (base::Contains(transports, Transport::kUsbHumanInterfaceDevice))
      discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::Contains(transports, Transport::kHybrid))
      cable_discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::Contains(transports, Transport::kNearFieldCommunication))
      nfc_discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::Contains(transports, Transport::kInternal))
      platform_discovery()->WaitForCallToStartAndSimulateSuccess();

    task_environment_.FastForwardUntilNoTasksRemain();
    EXPECT_FALSE(get_assertion_future().IsReady());

    if (!base::Contains(transports, Transport::kUsbHumanInterfaceDevice))
      EXPECT_FALSE(discovery()->is_start_requested());
    if (!base::Contains(transports, Transport::kHybrid))
      EXPECT_FALSE(cable_discovery()->is_start_requested());
    if (!base::Contains(transports, Transport::kNearFieldCommunication))
      EXPECT_FALSE(nfc_discovery()->is_start_requested());
    if (!base::Contains(transports, Transport::kInternal))
      EXPECT_FALSE(platform_discovery()->is_start_requested());

    EXPECT_THAT(
        request_handler->transport_availability_info().available_transports,
        ::testing::UnorderedElementsAreArray(transports));
  }

  void ExpectAllTransportsAreAllowedForRequest(
      GetAssertionRequestHandler* request_handler) {
    ExpectAllowedTransportsForRequestAre(
        request_handler, {FidoTransportProtocol::kUsbHumanInterfaceDevice,
                          FidoTransportProtocol::kInternal,
                          FidoTransportProtocol::kNearFieldCommunication,
                          FidoTransportProtocol::kHybrid});
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  test::FakeFidoDiscovery* cable_discovery() const { return cable_discovery_; }
  test::FakeFidoDiscovery* nfc_discovery() const { return nfc_discovery_; }
  test::FakeFidoDiscovery* platform_discovery() const {
    return platform_discovery_;
  }
  TestGetAssertionRequestFuture& get_assertion_future() {
    return get_assertion_future_;
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
  raw_ptr<test::FakeFidoDiscovery, DanglingUntriaged> discovery_;
  raw_ptr<test::FakeFidoDiscovery, DanglingUntriaged> cable_discovery_;
  raw_ptr<test::FakeFidoDiscovery, DanglingUntriaged> nfc_discovery_;
  raw_ptr<test::FakeFidoDiscovery, DanglingUntriaged> platform_discovery_;
  scoped_refptr<::testing::NiceMock<MockBluetoothAdapter>> mock_adapter_ =
      base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
  TestGetAssertionRequestFuture get_assertion_future_;
  base::flat_set<FidoTransportProtocol> supported_transports_ = {
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      FidoTransportProtocol::kInternal,
      FidoTransportProtocol::kNearFieldCommunication,
      FidoTransportProtocol::kHybrid};
  std::unique_ptr<BluetoothAdapterFactory::GlobalOverrideValues>
      bluetooth_config_ =
          BluetoothAdapterFactory::Get()->InitGlobalOverrideValues();
  FidoRequestHandlerBase::ScopedAlwaysAllowBLECalls always_allow_ble_calls_;
};

TEST_F(FidoGetAssertionHandlerTest, TransportAvailabilityInfo) {
  {
    // Empty allow list.
    auto request_handler = CreateGetAssertionHandlerWithRequestedTransports({});
    EXPECT_EQ(FidoRequestType::kGetAssertion,
              request_handler->transport_availability_info().request_type);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .transport_list_did_include_internal);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .transport_list_did_include_hybrid);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .transport_list_did_include_security_key);
    EXPECT_TRUE(
        request_handler->transport_availability_info().has_empty_allow_list);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .is_only_hybrid_or_internal);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .request_is_internal_only);
  }
  {
    // Internal and a phone.
    auto request_handler = CreateGetAssertionHandlerWithRequestedTransports(
        {{FidoTransportProtocol::kInternal},
         {FidoTransportProtocol::kInternal, FidoTransportProtocol::kHybrid}});
    EXPECT_EQ(FidoRequestType::kGetAssertion,
              request_handler->transport_availability_info().request_type);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .transport_list_did_include_internal);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .transport_list_did_include_hybrid);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .transport_list_did_include_security_key);
    EXPECT_FALSE(
        request_handler->transport_availability_info().has_empty_allow_list);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .is_only_hybrid_or_internal);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .request_is_internal_only);
  }
  {
    // Internal, a phone, and USB.
    auto request_handler = CreateGetAssertionHandlerWithRequestedTransports(
        {{FidoTransportProtocol::kUsbHumanInterfaceDevice},
         {FidoTransportProtocol::kInternal},
         {FidoTransportProtocol::kInternal, FidoTransportProtocol::kHybrid}});
    EXPECT_EQ(FidoRequestType::kGetAssertion,
              request_handler->transport_availability_info().request_type);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .transport_list_did_include_internal);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .transport_list_did_include_hybrid);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .transport_list_did_include_security_key);
    EXPECT_FALSE(
        request_handler->transport_availability_info().has_empty_allow_list);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .is_only_hybrid_or_internal);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .request_is_internal_only);
  }
  {
    // Only USB.
    auto request_handler = CreateGetAssertionHandlerWithRequestedTransports(
        {{FidoTransportProtocol::kUsbHumanInterfaceDevice}});
    EXPECT_EQ(FidoRequestType::kGetAssertion,
              request_handler->transport_availability_info().request_type);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .transport_list_did_include_internal);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .transport_list_did_include_hybrid);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .transport_list_did_include_security_key);
    EXPECT_FALSE(
        request_handler->transport_availability_info().has_empty_allow_list);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .is_only_hybrid_or_internal);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .request_is_internal_only);
  }
  {
    // A phone and an unknown (empty) transport credential.
    auto request_handler = CreateGetAssertionHandlerWithRequestedTransports(
        {{}, {FidoTransportProtocol::kHybrid}});
    EXPECT_EQ(FidoRequestType::kGetAssertion,
              request_handler->transport_availability_info().request_type);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .transport_list_did_include_internal);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .transport_list_did_include_hybrid);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .transport_list_did_include_security_key);
    EXPECT_FALSE(
        request_handler->transport_availability_info().has_empty_allow_list);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .is_only_hybrid_or_internal);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .request_is_internal_only);
  }
  {
    // Internal only.
    auto request_handler = CreateGetAssertionHandlerWithRequestedTransports(
        {{FidoTransportProtocol::kInternal},
         {FidoTransportProtocol::kInternal}});
    EXPECT_EQ(FidoRequestType::kGetAssertion,
              request_handler->transport_availability_info().request_type);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .transport_list_did_include_internal);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .transport_list_did_include_hybrid);
    EXPECT_FALSE(request_handler->transport_availability_info()
                     .transport_list_did_include_security_key);
    EXPECT_FALSE(
        request_handler->transport_availability_info().has_empty_allow_list);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .is_only_hybrid_or_internal);
    EXPECT_TRUE(request_handler->transport_availability_info()
                    .request_is_internal_only);
  }
}

TEST_F(FidoGetAssertionHandlerTest, CtapRequestOnSingleDevice) {
  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  discovery()->AddDevice(std::move(device));
  EXPECT_TRUE(get_assertion_future().Wait());

  EXPECT_EQ(GetAssertionStatus::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_TRUE(std::get<1>(get_assertion_future().Get()));
}

// Test a scenario where the connected authenticator is a U2F device.
TEST_F(FidoGetAssertionHandlerTest, TestU2fSign) {
  auto request_handler = CreateGetAssertionHandlerU2f();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  discovery()->AddDevice(std::move(device));
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetAssertionStatus::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_TRUE(std::get<1>(get_assertion_future().Get()));
}

TEST_F(FidoGetAssertionHandlerTest, TestIncompatibleUserVerificationSetting) {
  auto request = CtapGetAssertionRequest(test_data::kRelyingPartyId,
                                         test_data::kClientDataJson);
  request.user_verification = UserVerificationRequirement::kRequired;
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutUvSupport);
  device->ExpectRequestAndRespondWith(
      MockFidoDevice::EncodeCBORRequest(AsCTAPRequestValuePair(
          MakeCredentialTask::GetTouchRequest(device.get()))),
      test_data::kTestMakeCredentialResponse);

  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetAssertionStatus::kAuthenticatorMissingUserVerification,
            std::get<0>(get_assertion_future().Get()));
}

TEST_F(FidoGetAssertionHandlerTest,
       TestU2fSignRequestWithUserVerificationRequired) {
  auto request = CtapGetAssertionRequest(test_data::kRelyingPartyId,
                                         test_data::kClientDataJson);
  request.allow_list = {PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle))};
  request.user_verification = UserVerificationRequirement::kRequired;
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      ConstructBogusU2fRegistrationCommand(),
      test_data::kApduEncodedNoErrorRegisterResponse);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetAssertionStatus::kAuthenticatorMissingUserVerification,
            std::get<0>(get_assertion_future().Get()));
}

TEST_F(FidoGetAssertionHandlerTest, IncorrectRpIdHash) {
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest(
          test_data::kRelyingPartyId, test_data::kClientDataJson));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponseWithIncorrectRpIdHash);

  discovery()->AddDevice(std::move(device));

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(GetAssertionStatus::kAuthenticatorResponseInvalid,
            std::get<0>(get_assertion_future().Get()));
}

// Tests a scenario where the authenticator responds with credential ID that
// is not included in the allowed list.
TEST_F(FidoGetAssertionHandlerTest, InvalidCredential) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);
  request.allow_list = {PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(test_data::kKeyHandleAlpha))};
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  // Resident Keys must be disabled, otherwise allow list check is skipped.
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutResidentKeySupport);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  discovery()->AddDevice(std::move(device));

  // The response with the invalid credential ID is considered to be an error at
  // the task level and the request handler will drop the authenticator.
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_assertion_future().IsReady());
}

// Tests a scenario where the authenticator responds with an empty credential.
// When GetAssertion request only has a single credential in the allow list,
// this is a valid response. Check that credential is set by the client before
// the response is returned to the relying party.
TEST_F(FidoGetAssertionHandlerTest, ValidEmptyCredential) {
  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  // Resident Keys must be disabled, otherwise allow list check is skipped.
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutResidentKeySupport);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponseWithEmptyCredential);
  discovery()->AddDevice(std::move(device));

  EXPECT_TRUE(get_assertion_future().Wait());
  const auto& response = std::get<1>(get_assertion_future().Get());
  EXPECT_EQ(GetAssertionStatus::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  ASSERT_TRUE(response);
  ASSERT_EQ(1u, response->size());
  EXPECT_TRUE(response.value()[0].credential);
  EXPECT_THAT(
      response.value()[0].credential->id,
      ::testing::ElementsAreArray(test_data::kTestGetAssertionCredentialId));
}

TEST_F(FidoGetAssertionHandlerTest, TruncatedUTF8) {
  // Webauthn says[1] that authenticators may truncate strings in user entities.
  // Since authenticators aren't going to do UTF-8 processing, that means that
  // they may truncate a multi-byte code point and thus produce an invalid
  // string in the CBOR. This test exercises that case.
  //
  // [1] https://www.w3.org/TR/webauthn/#sctn-user-credential-params
  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestCtap2OnlyAuthenticatorGetInfoResponse);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponseWithTruncatedUTF8);
  discovery()->AddDevice(std::move(device));

  EXPECT_TRUE(get_assertion_future().Wait());
  const auto& response = std::get<1>(get_assertion_future().Get());
  EXPECT_EQ(GetAssertionStatus::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  ASSERT_TRUE(response);
  ASSERT_EQ(1u, response->size());
  ASSERT_TRUE(response.value()[0].user_entity);
  EXPECT_EQ(63u, response.value()[0].user_entity->name->size());
}

TEST_F(FidoGetAssertionHandlerTest, TruncatedAndInvalidUTF8) {
  // This test exercises the case where a UTF-8 string is truncated in a
  // response, and the UTF-8 string contains invalid code-points that
  // |base::IsStringUTF8| will be unhappy with.
  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestCtap2OnlyAuthenticatorGetInfoResponse);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponseWithTruncatedAndInvalidUTF8);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_assertion_future().IsReady());
}

// Tests a scenario where authenticator responds without user entity in its
// response but client is expecting a resident key credential.
TEST_F(FidoGetAssertionHandlerTest, IncorrectUserEntity) {
  // Use a GetAssertion request with an empty allow list.
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest(
          test_data::kRelyingPartyId, test_data::kClientDataJson));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  discovery()->AddDevice(std::move(device));

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(GetAssertionStatus::kAuthenticatorResponseInvalid,
            std::get<0>(get_assertion_future().Get()));
}

TEST_F(FidoGetAssertionHandlerTest,
       AllTransportsAllowedIfAllowCredentialsListUndefined) {
  auto request = CreateTestRequestWithCableExtension();
  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllTransportsAreAllowedForRequest(request_handler.get());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllTransportsAllowedIfAllowCredentialsListIsEmpty) {
  auto request = CreateTestRequestWithCableExtension();
  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllTransportsAreAllowedForRequest(request_handler.get());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllTransportsAllowedIfHasAllowedCredentialWithEmptyTransportsList) {
  auto request = CreateTestRequestWithCableExtension();
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(kBogusCredentialId)),
  };

  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllTransportsAreAllowedForRequest(request_handler.get());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllowedTransportsAreUnionOfTransportsLists) {
  auto request = CreateTestRequestWithCableExtension();
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(kBogusCredentialId),
          {FidoTransportProtocol::kInternal,
           FidoTransportProtocol::kNearFieldCommunication}),
  };

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllowedTransportsForRequestAre(
      request_handler.get(), {FidoTransportProtocol::kUsbHumanInterfaceDevice,
                              FidoTransportProtocol::kInternal,
                              FidoTransportProtocol::kNearFieldCommunication});
}

TEST_F(FidoGetAssertionHandlerTest, SupportedTransportsAreOnlyNfc) {
  const base::flat_set<FidoTransportProtocol> kNfc = {
      FidoTransportProtocol::kNearFieldCommunication,
  };

  set_supported_transports(kNfc);
  auto request_handler = CreateGetAssertionHandlerWithRequest(
      CreateTestRequestWithCableExtension());
  ExpectAllowedTransportsForRequestAre(request_handler.get(), kNfc);
}

TEST_F(FidoGetAssertionHandlerTest,
       SupportedTransportsAreOnlyCableAndInternal) {
  const base::flat_set<FidoTransportProtocol> kCableAndInternal = {
      FidoTransportProtocol::kHybrid,
      FidoTransportProtocol::kInternal,
  };

  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  set_supported_transports(kCableAndInternal);
  auto request_handler = CreateGetAssertionHandlerWithRequest(
      CreateTestRequestWithCableExtension());
  ExpectAllowedTransportsForRequestAre(request_handler.get(),
                                       kCableAndInternal);
}

TEST_F(FidoGetAssertionHandlerTest, SuccessWithOnlyUsbTransportAllowed) {
  auto request = CreateTestRequestWithCableExtension();
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
  };

  set_supported_transports({FidoTransportProtocol::kUsbHumanInterfaceDevice});

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  EXPECT_TRUE(get_assertion_future().Wait());

  EXPECT_EQ(GetAssertionStatus::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_TRUE(std::get<1>(get_assertion_future().Get()));
  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(
          FidoTransportProtocol::kUsbHumanInterfaceDevice));
}

TEST_F(FidoGetAssertionHandlerTest, SuccessWithOnlyNfcTransportAllowed) {
  auto request = CreateTestRequestWithCableExtension();
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kNearFieldCommunication}),
  };

  set_supported_transports({FidoTransportProtocol::kNearFieldCommunication});

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->SetDeviceTransport(FidoTransportProtocol::kNearFieldCommunication);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  nfc_discovery()->WaitForCallToStartAndSimulateSuccess();
  nfc_discovery()->AddDevice(std::move(device));

  EXPECT_TRUE(get_assertion_future().Wait());

  EXPECT_EQ(GetAssertionStatus::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_TRUE(std::get<1>(get_assertion_future().Get()));
  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(
          FidoTransportProtocol::kNearFieldCommunication));
}

TEST_F(FidoGetAssertionHandlerTest, SuccessWithOnlyInternalTransportAllowed) {
  auto request = CreateTestRequestWithCableExtension();
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kInternal}),
  };

  set_supported_transports({FidoTransportProtocol::kInternal});

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));

  auto device = MockFidoDevice::MakeCtap(
      ReadCTAPGetInfoResponse(test_data::kTestGetInfoResponsePlatformDevice));
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestGetInfoResponsePlatformDevice);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  platform_discovery()->WaitForCallToStartAndSimulateSuccess();
  platform_discovery()->AddDevice(std::move(device));

  EXPECT_TRUE(get_assertion_future().Wait());

  EXPECT_EQ(GetAssertionStatus::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  EXPECT_TRUE(std::get<1>(get_assertion_future().Get()));
  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(FidoTransportProtocol::kInternal));
}

// If a device with transport type kInternal returns a
// CTAP2_ERR_OPERATION_DENIED error, the request should complete with
// GetAssertionStatus::kUserConsentDenied. Pending authenticators should be
// cancelled.
TEST_F(FidoGetAssertionHandlerTest,
       TestRequestWithOperationDeniedErrorPlatform) {
  auto request_handler = CreateGetAssertionHandlerCtap();

  auto platform_device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponsePlatformDevice);
  platform_device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  platform_device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      CtapDeviceResponseCode::kCtap2ErrOperationDenied, base::Microseconds(10));
  platform_discovery()->WaitForCallToStartAndSimulateSuccess();
  platform_discovery()->AddDevice(std::move(platform_device));

  auto other_device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  other_device->ExpectCtap2CommandAndDoNotRespond(
      CtapRequestCommand::kAuthenticatorGetAssertion);
  EXPECT_CALL(*other_device, Cancel);

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(other_device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(get_assertion_future().IsReady());
  EXPECT_EQ(GetAssertionStatus::kUserConsentDenied,
            std::get<0>(get_assertion_future().Get()));
}

// Like |TestRequestWithOperationDeniedErrorPlatform|, but with a
// cross-platform device.
TEST_F(FidoGetAssertionHandlerTest,
       TestRequestWithOperationDeniedErrorCrossPlatform) {
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      CtapDeviceResponseCode::kCtap2ErrOperationDenied);

  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(get_assertion_future().IsReady());
  EXPECT_EQ(GetAssertionStatus::kUserConsentDenied,
            std::get<0>(get_assertion_future().Get()));
}

// If a device returns CTAP2_ERR_PIN_AUTH_INVALID, the request should complete
// with GetAssertionStatus::kUserConsentDenied.
TEST_F(FidoGetAssertionHandlerTest, TestRequestWithPinAuthInvalid) {
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid);

  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(get_assertion_future().IsReady());
  EXPECT_EQ(GetAssertionStatus::kUserConsentDenied,
            std::get<0>(get_assertion_future().Get()));
}

MATCHER_P(IsCtap2Command, expected_command, "") {
  return !arg.empty() && arg[0] == base::strict_cast<uint8_t>(expected_command);
}

TEST_F(FidoGetAssertionHandlerTest, DeviceFailsImmediately) {
  // Test that, when a device immediately returns an unexpected error, the
  // request continues and waits for another device.

  auto broken_device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  EXPECT_CALL(
      *broken_device,
      DeviceTransactPtr(
          IsCtap2Command(CtapRequestCommand::kAuthenticatorGetAssertion), _))
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
                    CtapRequestCommand::kAuthenticatorGetAssertion,
                    test_data::kTestGetAssertionResponse);
                discovery()->AddDevice(std::move(working_device));
              })),
          ::testing::Return(0)));

  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(broken_device));

  EXPECT_TRUE(get_assertion_future().Wait());
  EXPECT_EQ(GetAssertionStatus::kSuccess,
            std::get<0>(get_assertion_future().Get()));
}

TEST_F(FidoGetAssertionHandlerTest, PinUvAuthTokenPreTouchFailure) {
  VirtualCtap2Device::Config config;
  config.ctap2_versions = {Ctap2Version::kCtap2_1};
  config.pin_uv_auth_token_support = true;
  config.internal_uv_support = true;
  config.override_response_map[CtapRequestCommand::kAuthenticatorClientPin] =
      CtapDeviceResponseCode::kCtap2ErrOther;
  auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
  state->fingerprints_enrolled = true;

  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);
  request.allow_list = {PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(
          test_data::kTestGetAssertionCredentialId))};
  request.user_verification = UserVerificationRequirement::kRequired;
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::make_unique<VirtualCtap2Device>(
      std::move(state), std::move(config)));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_assertion_future().IsReady());
}

// Tests a scenario where authenticator of incorrect transport type was used to
// conduct CTAP GetAssertion call.
//
// TODO(engedy): This should not happen, instead |allowCredentials| should be
// filtered to only contain items compatible with the transport actually used to
// talk to the authenticator.
TEST(GetAssertionRequestHandlerTest, IncorrectTransportType) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  device::test::VirtualFidoDeviceFactory virtual_device_factory;
  virtual_device_factory.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  ASSERT_TRUE(virtual_device_factory.mutable_state()->InjectRegistration(
      fido_parsing_utils::Materialize(test_data::kTestGetAssertionCredentialId),
      test_data::kRelyingPartyId));

  // Request the correct credential ID, but set a different transport hint.
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kBluetoothLowEnergy}),
  };

  TestGetAssertionRequestFuture future;
  auto request_handler = std::make_unique<GetAssertionRequestHandler>(
      &virtual_device_factory,
      std::vector<std::unique_ptr<FidoDiscoveryBase>>(),
      base::flat_set<FidoTransportProtocol>(
          {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
      std::move(request), CtapGetAssertionOptions(),
      /*allow_skipping_pin_touch=*/true, future.GetCallback());

  task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(future.IsReady());
}

TEST_F(FidoGetAssertionHandlerTest, ReportTransportMetric) {
  base::HistogramTester histograms;
  auto request_handler = CreateGetAssertionHandlerCtap();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  discovery()->AddDevice(std::move(device));

  auto nfc_device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  nfc_device->SetDeviceTransport(
      FidoTransportProtocol::kNearFieldCommunication);
  nfc_device->ExpectCtap2CommandAndDoNotRespond(
      CtapRequestCommand::kAuthenticatorGetAssertion);
  EXPECT_CALL(*nfc_device, Cancel(_));
  nfc_discovery()->AddDevice(std::move(nfc_device));

  nfc_discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  EXPECT_TRUE(get_assertion_future().Wait());

  EXPECT_EQ(GetAssertionStatus::kSuccess,
            std::get<0>(get_assertion_future().Get()));
  histograms.ExpectBucketCount(kRequestTransportHistogram,
                               FidoTransportProtocol::kUsbHumanInterfaceDevice,
                               1);
  histograms.ExpectBucketCount(kRequestTransportHistogram,
                               FidoTransportProtocol::kNearFieldCommunication,
                               1);
  histograms.ExpectUniqueSample(kResponseTransportHistogram,
                                FidoTransportProtocol::kUsbHumanInterfaceDevice,
                                1);
}

#if BUILDFLAG(IS_WIN)

// Verify that the request handler instantiates a HID device backed
// FidoDeviceAuthenticator or a WinNativeCrossPlatformAuthenticator, depending
// on API availability.
TEST(GetAssertionRequestHandlerWinTest, TestWinUsbDiscovery) {
  base::test::TaskEnvironment task_environment;
  for (const bool enable_api : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "enable_api=" << enable_api);
    FakeWinWebAuthnApi api;
    api.set_available(enable_api);
    api.InjectNonDiscoverableCredential(
        test_data::kTestGetAssertionCredentialId, test_data::kRelyingPartyId);
    WinWebAuthnApi::ScopedOverride win_webauthn_api_override(&api);

    // Simulate a connected HID device.
    ScopedFakeFidoHidManager fake_hid_manager;
    fake_hid_manager.AddFidoHidDevice("guid");

    TestGetAssertionRequestFuture future;
    FidoDiscoveryFactory fido_discovery_factory;
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataJson);
    request.allow_list = {PublicKeyCredentialDescriptor(
        CredentialType::kPublicKey,
        fido_parsing_utils::Materialize(
            test_data::kTestGetAssertionCredentialId))};
    auto handler = std::make_unique<GetAssertionRequestHandler>(
        &fido_discovery_factory,
        std::vector<std::unique_ptr<FidoDiscoveryBase>>(),
        base::flat_set<FidoTransportProtocol>(
            {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
        std::move(request), CtapGetAssertionOptions(),
        /*allow_skipping_pin_touch=*/true, future.GetCallback());
    task_environment.RunUntilIdle();

    EXPECT_EQ(handler->AuthenticatorsForTesting().size(), 1u);
    EXPECT_EQ(handler->AuthenticatorsForTesting().begin()->second->GetType() ==
                  AuthenticatorType::kWinNative,
              enable_api);
  }
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace device
