// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_task.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#endif  // BUILDFLAG(IS_WIN)

using ::testing::_;

namespace device {

namespace {

using FakeTaskCallback =
    base::OnceCallback<void(CtapDeviceResponseCode status_code,
                            absl::optional<std::vector<uint8_t>>)>;
using FakeHandlerCallbackReceiver =
    test::StatusAndValuesCallbackReceiver<bool,
                                          absl::optional<std::vector<uint8_t>>,
                                          const FidoAuthenticator*>;

enum class FakeTaskResponse : uint8_t {
  kSuccess = 0x00,
  kErrorReceivedAfterObtainingUserPresence = 0x01,
  kProcessingError = 0x02,
  kOperationDenied = 0x03,
};

// FidoRequestHandler that automatically starts discovery but does nothing on
// DispatchRequest().
class EmptyRequestHandler : public FidoRequestHandlerBase {
 public:
  EmptyRequestHandler(const base::flat_set<FidoTransportProtocol>& protocols,
                      test::FakeFidoDiscoveryFactory* fake_discovery_factory)
      : FidoRequestHandlerBase(fake_discovery_factory, protocols) {
    Start();
  }
  ~EmptyRequestHandler() override = default;

  void DispatchRequest(FidoAuthenticator* authenticator) override {}
};

class TestObserver : public FidoRequestHandlerBase::Observer {
 public:
  using TransportAvailabilityNotificationReceiver = test::TestCallbackReceiver<
      FidoRequestHandlerBase::TransportAvailabilityInfo>;

  TestObserver() = default;

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  FidoRequestHandlerBase::TransportAvailabilityInfo
  WaitForTransportAvailabilityInfo() {
    transport_availability_notification_receiver_.WaitForCallback();
    return std::get<0>(*transport_availability_notification_receiver_.result());
  }

  void WaitForAndExpectAvailableTransportsAre(
      base::flat_set<FidoTransportProtocol> expected_transports,
      FidoRequestHandlerBase::RecognizedCredential
          has_platform_authenticator_credential =
              FidoRequestHandlerBase::RecognizedCredential::kUnknown) {
    auto result = WaitForTransportAvailabilityInfo();
    EXPECT_THAT(result.available_transports,
                ::testing::UnorderedElementsAreArray(expected_transports));
    EXPECT_EQ(result.has_platform_authenticator_credential,
              has_platform_authenticator_credential);
  }

 protected:
  // FidoRequestHandlerBase::Observer:
  void OnTransportAvailabilityEnumerated(
      FidoRequestHandlerBase::TransportAvailabilityInfo data) override {
    transport_availability_notification_receiver_.callback().Run(
        std::move(data));
  }
  bool EmbedderControlsAuthenticatorDispatch(
      const FidoAuthenticator&) override {
    return false;
  }

  void BluetoothAdapterPowerChanged(bool is_powered_on) override {}
  void FidoAuthenticatorAdded(const FidoAuthenticator& authenticator) override {
  }
  void FidoAuthenticatorRemoved(base::StringPiece device_id) override {}

  bool SupportsPIN() const override { return false; }

  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override {
    NOTREACHED();
  }

  void OnRetryUserVerification(int attempts) override {}

  void StartBioEnrollment(base::OnceClosure next_callback) override {}

  void OnSampleCollected(int remaining_samples) override {}

  void FinishCollectToken() override { NOTREACHED(); }

 private:
  TransportAvailabilityNotificationReceiver
      transport_availability_notification_receiver_;
};

// Fake FidoTask implementation that sends an empty byte array to the device
// when StartTask() is invoked.
class FakeFidoTask : public FidoTask {
 public:
  FakeFidoTask(FidoDevice* device, FakeTaskCallback callback)
      : FidoTask(device), callback_(std::move(callback)) {}
  ~FakeFidoTask() override = default;

  void Cancel() override {
    if (token_) {
      device()->Cancel(*token_);
      token_.reset();
    }
  }

  void StartTask() override {
    token_ = device()->DeviceTransact(
        std::vector<uint8_t>(),
        base::BindOnce(&FakeFidoTask::CompletionCallback,
                       weak_factory_.GetWeakPtr()));
  }

  void CompletionCallback(
      absl::optional<std::vector<uint8_t>> device_response) {
    DCHECK(device_response && device_response->size() == 1);
    switch (static_cast<FakeTaskResponse>(device_response->front())) {
      case FakeTaskResponse::kSuccess:
        std::move(callback_).Run(CtapDeviceResponseCode::kSuccess,
                                 std::vector<uint8_t>());
        return;

      case FakeTaskResponse::kErrorReceivedAfterObtainingUserPresence:
        std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
                                 std::vector<uint8_t>());
        return;

      case FakeTaskResponse::kOperationDenied:
        std::move(callback_).Run(
            CtapDeviceResponseCode::kCtap2ErrOperationDenied, absl::nullopt);
        return;
      case FakeTaskResponse::kProcessingError:
      default:
        std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                                 absl::nullopt);
        return;
    }
  }

 private:
  absl::optional<FidoDevice::CancelToken> token_;
  FakeTaskCallback callback_;
  base::WeakPtrFactory<FakeFidoTask> weak_factory_{this};
};

class FakeFidoRequestHandler : public FidoRequestHandlerBase {
 public:
  using CompletionCallback =
      base::OnceCallback<void(bool,
                              absl::optional<std::vector<uint8_t>>,
                              const FidoAuthenticator*)>;

  FakeFidoRequestHandler(test::FakeFidoDiscoveryFactory* fake_discovery_factory,
                         const base::flat_set<FidoTransportProtocol>& protocols,
                         CompletionCallback callback)
      : FidoRequestHandlerBase(fake_discovery_factory, protocols),
        completion_callback_(std::move(callback)) {
    Start();
  }
  ~FakeFidoRequestHandler() override = default;

  void set_has_platform_credential(
      RecognizedCredential has_platform_credential) {
    has_platform_credential_ = has_platform_credential;
  }

 private:
  void DispatchRequest(FidoAuthenticator* authenticator) override {
    // FidoRequestHandlerTest uses FakeDiscovery to inject mock devices
    // that get wrapped in a FidoDeviceAuthenticator, so we can safely cast
    // here.
    auto* device_authenticator =
        static_cast<FidoDeviceAuthenticator*>(authenticator);
    // Instead of sending a real CTAP request, send an empty byte array. Note
    // that during discovery, the device already has received a GetInfo command
    // at this point.
    device_authenticator->SetTaskForTesting(std::make_unique<FakeFidoTask>(
        device_authenticator->device(),
        base::BindOnce(&FakeFidoRequestHandler::HandleResponse,
                       weak_factory_.GetWeakPtr(), authenticator)));
  }

  void AuthenticatorAdded(FidoDiscoveryBase* discovery,
                          FidoAuthenticator* authenticator) override {
    if (authenticator->AuthenticatorTransport() ==
        FidoTransportProtocol::kInternal) {
      transport_availability_info().has_platform_authenticator_credential =
          has_platform_credential_;
    }

    FidoRequestHandlerBase::AuthenticatorAdded(discovery, authenticator);
  }

  void HandleResponse(FidoAuthenticator* authenticator,
                      CtapDeviceResponseCode status,
                      absl::optional<std::vector<uint8_t>> response) {
    auto* device_authenticator =
        static_cast<FidoDeviceAuthenticator*>(authenticator);
    device_authenticator->SetTaskForTesting(nullptr);

    if (status == CtapDeviceResponseCode::kCtap2ErrOther) {
      // Simulates an error that is sent without the user touching the
      // authenticator (FakeTaskResponse::kProcessingError). Don't resolve
      // the request for this response.
      return;
    }

    if (!completion_callback_) {
      return;
    }

    CancelActiveAuthenticators(authenticator->GetId());
    std::move(completion_callback_)
        .Run(status == CtapDeviceResponseCode::kSuccess, std::move(response),
             authenticator);
  }

  CompletionCallback completion_callback_;
  RecognizedCredential has_platform_credential_ =
      RecognizedCredential::kNoRecognizedCredential;

  base::WeakPtrFactory<FakeFidoRequestHandler> weak_factory_{this};
};

std::vector<uint8_t> CreateFakeSuccessDeviceResponse() {
  return {base::strict_cast<uint8_t>(FakeTaskResponse::kSuccess)};
}

std::vector<uint8_t> CreateFakeUserPresenceVerifiedError() {
  return {base::strict_cast<uint8_t>(
      FakeTaskResponse::kErrorReceivedAfterObtainingUserPresence)};
}

std::vector<uint8_t> CreateFakeDeviceProcesssingError() {
  return {base::strict_cast<uint8_t>(FakeTaskResponse::kProcessingError)};
}

std::vector<uint8_t> CreateFakeOperationDeniedError() {
  return {base::strict_cast<uint8_t>(FakeTaskResponse::kOperationDenied)};
}

}  // namespace

class FidoRequestHandlerTest : public ::testing::Test {
 public:
  FidoRequestHandlerTest() {
    mock_adapter_ =
        base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
    BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  void ForgeNextHidDiscovery() {
    discovery_ = fake_discovery_factory_.ForgeNextHidDiscovery();
  }

  std::unique_ptr<FakeFidoRequestHandler> CreateFakeHandler() {
    ForgeNextHidDiscovery();
    auto handler = std::make_unique<FakeFidoRequestHandler>(
        &fake_discovery_factory_,
        base::flat_set<FidoTransportProtocol>(
            {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
        cb_.callback());
    return handler;
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  scoped_refptr<::testing::NiceMock<MockBluetoothAdapter>> adapter() {
    return mock_adapter_;
  }
  FakeHandlerCallbackReceiver& callback() { return cb_; }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::FakeFidoDiscoveryFactory fake_discovery_factory_;
  scoped_refptr<::testing::NiceMock<MockBluetoothAdapter>> mock_adapter_;
  raw_ptr<test::FakeFidoDiscovery> discovery_;
  FakeHandlerCallbackReceiver cb_;
};

TEST_F(FidoRequestHandlerTest, TestSingleDeviceSuccess) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = std::make_unique<MockFidoDevice>();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo, absl::nullopt);
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  // Device returns success response.
  device->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                      CreateFakeSuccessDeviceResponse());

  discovery()->AddDevice(std::move(device));
  callback().WaitForCallback();
  EXPECT_TRUE(callback().status());
}

// Tests a scenario where two unresponsive authenticators are connected and
// cancel request has been sent either from the user or from the relying party
// (i.e. FidoRequestHandler object is destroyed.) Upon destruction, cancel
// command must be invoked to all connected authenticators.
TEST_F(FidoRequestHandlerTest, TestAuthenticatorHandlerReset) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device0 = std::make_unique<MockFidoDevice>();
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  device0->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  EXPECT_CALL(*device0, Cancel(_));
  auto device1 = std::make_unique<MockFidoDevice>();
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  EXPECT_CALL(*device1, Cancel(_));

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));
  task_environment_.FastForwardUntilNoTasksRemain();
  request_handler.reset();
}

// Test a scenario where 2 devices are connected and a response is received
// from only a single device(device1) and the remaining device hangs.
TEST_F(FidoRequestHandlerTest, TestRequestWithMultipleDevices) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Represents a connected device that hangs without a response.
  auto device0 = std::make_unique<MockFidoDevice>();
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  // Device is unresponsive and cancel command is invoked afterwards.
  device0->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  EXPECT_CALL(*device0, Cancel(_));

  // Represents a connected device that response successfully.
  auto device1 = std::make_unique<MockFidoDevice>();
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeSuccessDeviceResponse());

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));

  callback().WaitForCallback();
  EXPECT_TRUE(callback().status());
}

// Test a scenario where 2 devices respond successfully with small time
// delay. Only the first received response should be passed on to the relying
// party, and cancel request should be sent to the other authenticator.
TEST_F(FidoRequestHandlerTest, TestRequestWithMultipleSuccessResponses) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Represents a connected device that responds successfully after small time
  // delay.
  auto device0 = std::make_unique<MockFidoDevice>();
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  device0->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeSuccessDeviceResponse(),
                                       base::Microseconds(1));

  // Represents a device that returns a success response after a longer time
  // delay.
  auto device1 = std::make_unique<MockFidoDevice>();
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  device1->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeSuccessDeviceResponse(),
                                       base::Microseconds(10));
  // Cancel command is invoked after receiving response from |device0|.
  EXPECT_CALL(*device1, Cancel(_));

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));

  task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_TRUE(callback().status());
}

// Test a scenario where 3 devices respond with a processing error, an UP(user
// presence) verified failure response with small time delay, and an UP
// verified failure response with big time delay, respectively. Request for
// device with processing error should be immediately dropped. Also, for UP
// verified failures, the first received response should be passed on to the
// relying party and cancel command should be sent to the remaining device.
TEST_F(FidoRequestHandlerTest, TestRequestWithMultipleFailureResponses) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Represents a connected device that immediately responds with a processing
  // error.
  auto device0 = std::make_unique<MockFidoDevice>();
  device0->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device0, GetId()).WillRepeatedly(testing::Return("device0"));
  EXPECT_CALL(*device0, GetDisplayName())
      .WillRepeatedly(testing::Return(std::string()));
  device0->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeDeviceProcesssingError());

  // Represents a device that returns an UP verified failure response after a
  // small time delay.
  auto device1 = std::make_unique<MockFidoDevice>();
  device1->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device1, GetId()).WillRepeatedly(testing::Return("device1"));
  EXPECT_CALL(*device1, GetDisplayName())
      .WillRepeatedly(testing::Return(std::string()));
  device1->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeUserPresenceVerifiedError(),
                                       base::Microseconds(1));

  // Represents a device that returns an UP verified failure response after a
  // big time delay.
  auto device2 = std::make_unique<MockFidoDevice>();
  device2->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestAuthenticatorGetInfoResponse);
  EXPECT_CALL(*device2, GetId()).WillRepeatedly(testing::Return("device2"));
  EXPECT_CALL(*device2, GetDisplayName())
      .WillRepeatedly(testing::Return(std::string()));
  device2->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeDeviceProcesssingError(),
                                       base::Microseconds(10));
  EXPECT_CALL(*device2, Cancel(_));

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));
  discovery()->AddDevice(std::move(device2));

  task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_FALSE(callback().status());
}

// If a device with transport type kInternal returns a
// CTAP2_ERR_OPERATION_DENIED error, the request should be cancelled on all
// pending authenticators.
TEST_F(FidoRequestHandlerTest,
       TestRequestWithOperationDeniedErrorInternalTransport) {
  TestObserver observer;

  // Device will send CTAP2_ERR_OPERATION_DENIED.
  auto device0 = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponsePlatformDevice);
  device0->SetDeviceTransport(FidoTransportProtocol::kInternal);
  device0->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeOperationDeniedError(),
                                       base::Microseconds(10));

  ForgeNextHidDiscovery();
  auto* platform_discovery =
      fake_discovery_factory_.ForgeNextPlatformDiscovery();
  auto request_handler = std::make_unique<FakeFidoRequestHandler>(
      &fake_discovery_factory_,
      base::flat_set<FidoTransportProtocol>(
          {FidoTransportProtocol::kInternal,
           FidoTransportProtocol::kUsbHumanInterfaceDevice}),
      callback().callback());
  request_handler->set_observer(&observer);

  auto device1 = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device1->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  EXPECT_CALL(*device1, Cancel(_));

  discovery()->AddDevice(std::move(device0));
  platform_discovery->AddDevice(std::move(device1));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  platform_discovery->WaitForCallToStartAndSimulateSuccess();

  task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_FALSE(callback().status());
}

// Like |TestRequestWithOperationDeniedErrorInternalTransport|, but with a
// cross-platform authenticator.
TEST_F(FidoRequestHandlerTest,
       TestRequestWithOperationDeniedErrorCrossPlatform) {
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  // Device will send CTAP2_ERR_OPERATION_DENIED.
  auto device0 = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device0->SetDeviceTransport(FidoTransportProtocol::kUsbHumanInterfaceDevice);
  device0->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                       CreateFakeOperationDeniedError());

  auto device1 = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device1->ExpectRequestAndDoNotRespond(std::vector<uint8_t>());
  EXPECT_CALL(*device1, Cancel(_));

  discovery()->AddDevice(std::move(device0));
  discovery()->AddDevice(std::move(device1));

  task_environment_.FastForwardUntilNoTasksRemain();
  callback().WaitForCallback();
  EXPECT_FALSE(callback().status());
}

// Requests should be dispatched to the platform authenticator.
TEST_F(FidoRequestHandlerTest, TestWithPlatformAuthenticator) {
  // A platform authenticator usually wouldn't usually use a FidoDevice, but
  // that's not the point of the test here. The test is only trying to ensure
  // the authenticator gets injected and used.
  auto device = MockFidoDevice::MakeCtap();
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestGetInfoResponsePlatformDevice);
  // Device returns success response.
  device->ExpectRequestAndRespondWith(std::vector<uint8_t>(),
                                      CreateFakeSuccessDeviceResponse());
  device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  auto* fake_discovery = fake_discovery_factory_.ForgeNextPlatformDiscovery(
      test::FakeFidoDiscovery::StartMode::kAutomatic);

  TestObserver observer;
  auto request_handler = std::make_unique<FakeFidoRequestHandler>(
      &fake_discovery_factory_,
      base::flat_set<FidoTransportProtocol>({FidoTransportProtocol::kInternal}),
      callback().callback());
  request_handler->set_has_platform_credential(
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);
  request_handler->set_observer(&observer);
  fake_discovery->AddDevice(std::move(device));

  observer.WaitForAndExpectAvailableTransportsAre(
      {FidoTransportProtocol::kInternal},
      FidoRequestHandlerBase::RecognizedCredential::kHasRecognizedCredential);

  callback().WaitForCallback();
  EXPECT_TRUE(callback().status());
}

TEST_F(FidoRequestHandlerTest, InternalTransportDisallowedIfMarkedUnavailable) {
  TestObserver observer;
  auto request_handler = std::make_unique<FakeFidoRequestHandler>(
      &fake_discovery_factory_,
      base::flat_set<FidoTransportProtocol>({FidoTransportProtocol::kInternal}),
      callback().callback());
  request_handler->set_observer(&observer);

  observer.WaitForAndExpectAvailableTransportsAre({});
}

TEST_F(FidoRequestHandlerTest,
       TransportAvailabilityNotificationOnObserverSetLate) {
  TestObserver observer;
  auto request_handler = CreateFakeHandler();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  task_environment_.FastForwardUntilNoTasksRemain();

  request_handler->set_observer(&observer);
  observer.WaitForAndExpectAvailableTransportsAre(
      {FidoTransportProtocol::kUsbHumanInterfaceDevice});
}

#if BUILDFLAG(IS_WIN)
TEST_F(FidoRequestHandlerTest, TransportAvailabilityOfWindowsAuthenticator) {
  FakeWinWebAuthnApi api;
  fake_discovery_factory_.set_win_webauthn_api(&api);
  for (const bool api_available : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "api_available=" << api_available);
    api.set_available(api_available);

    TestObserver observer;
    ForgeNextHidDiscovery();
    EmptyRequestHandler request_handler(
        {FidoTransportProtocol::kUsbHumanInterfaceDevice},
        &fake_discovery_factory_);
    request_handler.set_observer(&observer);

    // If the windows API is not enabled, the request is dispatched to the USB
    // discovery. Simulate a success to fill the transport availability info.
    if (!api_available)
      discovery()->WaitForCallToStartAndSimulateSuccess();

    task_environment_.FastForwardUntilNoTasksRemain();

    auto transport_availability_info =
        observer.WaitForTransportAvailabilityInfo();
    EXPECT_EQ(transport_availability_info.available_transports.empty(),
              api_available);
    EXPECT_EQ(transport_availability_info.has_win_native_api_authenticator,
              api_available);
    EXPECT_EQ(transport_availability_info.win_native_api_authenticator_id,
              api_available ? "WinWebAuthnApiAuthenticator" : "");
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace device
