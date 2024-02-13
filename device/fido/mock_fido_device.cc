// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mock_fido_device.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "components/apdu/apdu_response.h"
#include "components/cbor/writer.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"

namespace device {

namespace {
AuthenticatorGetInfoResponse DefaultAuthenticatorInfo() {
  return *ReadCTAPGetInfoResponse(test_data::kTestAuthenticatorGetInfoResponse);
}
}  // namespace

// static
std::unique_ptr<MockFidoDevice> MockFidoDevice::MakeU2f() {
  return std::make_unique<MockFidoDevice>(ProtocolVersion::kU2f, std::nullopt);
}

// static
std::unique_ptr<MockFidoDevice> MockFidoDevice::MakeCtap(
    std::optional<AuthenticatorGetInfoResponse> device_info) {
  if (!device_info) {
    device_info = DefaultAuthenticatorInfo();
  }
  auto device = std::make_unique<MockFidoDevice>(ProtocolVersion::kCtap2,
                                                 std::move(*device_info));
  device->StubGetDisplayName();
  return device;
}

// static
std::unique_ptr<MockFidoDevice>
MockFidoDevice::MakeU2fWithGetInfoExpectation() {
  auto device = std::make_unique<MockFidoDevice>();
  device->StubGetId();
  device->StubGetDisplayName();
  device->ExpectWinkedAtLeastOnce();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo, std::nullopt);
  return device;
}

// static
std::unique_ptr<MockFidoDevice> MockFidoDevice::MakeCtapWithGetInfoExpectation(
    std::optional<base::span<const uint8_t>> get_info_response) {
  if (!get_info_response) {
    get_info_response = test_data::kTestAuthenticatorGetInfoResponse;
  }

  auto get_info = ReadCTAPGetInfoResponse(*get_info_response);
  CHECK(get_info);
  auto device = MockFidoDevice::MakeCtap(std::move(*get_info));
  device->StubGetId();
  device->StubGetDisplayName();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo, std::move(get_info_response));
  return device;
}

std::vector<uint8_t> MockFidoDevice::EncodeCBORRequest(
    std::pair<CtapRequestCommand, std::optional<cbor::Value>> request) {
  std::vector<uint8_t> request_bytes;

  if (request.second) {
    std::optional<std::vector<uint8_t>> cbor_bytes =
        cbor::Writer::Write(*request.second);
    DCHECK(cbor_bytes);
    request_bytes = std::move(*cbor_bytes);
  }
  request_bytes.insert(request_bytes.begin(),
                       static_cast<uint8_t>(request.first));
  return request_bytes;
}

// Matcher to compare the first byte of the incoming requests.
MATCHER_P(IsCtap2Command, expected_command, "") {
  return !arg.empty() && arg[0] == base::strict_cast<uint8_t>(expected_command);
}

MockFidoDevice::MockFidoDevice() = default;
MockFidoDevice::MockFidoDevice(
    ProtocolVersion protocol_version,
    std::optional<AuthenticatorGetInfoResponse> device_info)
    : MockFidoDevice() {
  set_supported_protocol(protocol_version);
  if (device_info) {
    SetDeviceInfo(std::move(*device_info));
  }
}
MockFidoDevice::~MockFidoDevice() = default;

FidoDevice::CancelToken MockFidoDevice::DeviceTransact(
    std::vector<uint8_t> command,
    DeviceCallback cb) {
  return DeviceTransactPtr(command, cb);
}

void MockFidoDevice::TryWink(base::OnceClosure cb) {
  TryWinkRef(cb);
}

FidoTransportProtocol MockFidoDevice::DeviceTransport() const {
  return transport_protocol_;
}

base::WeakPtr<FidoDevice> MockFidoDevice::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void MockFidoDevice::ExpectWinkedAtLeastOnce() {
  EXPECT_CALL(*this, TryWinkRef(::testing::_))
      .Times(::testing::AtLeast(1))
      .WillRepeatedly([](base::OnceClosure& cb) { std::move(cb).Run(); });
}

void MockFidoDevice::StubGetId() {
  // Use a counter to keep the device ID unique.
  static size_t i = 0;
  EXPECT_CALL(*this, GetId())
      .WillRepeatedly(
          testing::Return(base::StrCat({"mockdevice", std::to_string(i++)})));
}

void MockFidoDevice::ExpectCtap2CommandAndRespondWith(
    CtapRequestCommand command,
    std::optional<base::span<const uint8_t>> response,
    base::TimeDelta delay,
    testing::Matcher<base::span<const uint8_t>> request_matcher) {
  auto data = fido_parsing_utils::MaterializeOrNull(response);
  auto send_response = [data(std::move(data)), delay](DeviceCallback& cb) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(cb), std::move(data)), delay);
  };

  EXPECT_CALL(*this,
              DeviceTransactPtr(AllOf(IsCtap2Command(command), request_matcher),
                                ::testing::_))
      .WillOnce(::testing::DoAll(
          ::testing::WithArg<1>(::testing::Invoke(send_response)),
          ::testing::Return(0)));
}

void MockFidoDevice::ExpectCtap2CommandAndRespondWithError(
    CtapRequestCommand command,
    CtapDeviceResponseCode response_code,
    base::TimeDelta delay) {
  std::array<uint8_t, 1> data{base::strict_cast<uint8_t>(response_code)};
  return ExpectCtap2CommandAndRespondWith(std::move(command), data, delay);
}

void MockFidoDevice::ExpectRequestAndRespondWith(
    base::span<const uint8_t> request,
    std::optional<base::span<const uint8_t>> response,
    base::TimeDelta delay) {
  auto data = fido_parsing_utils::MaterializeOrNull(response);
  auto send_response = [data(std::move(data)), delay](DeviceCallback& cb) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(std::move(cb), std::move(data)), delay);
  };

  auto request_as_vector = fido_parsing_utils::Materialize(request);
  EXPECT_CALL(*this,
              DeviceTransactPtr(std::move(request_as_vector), ::testing::_))
      .WillOnce(::testing::DoAll(
          ::testing::WithArg<1>(::testing::Invoke(send_response)),
          ::testing::Return(0)));
}

void MockFidoDevice::ExpectCtap2CommandAndDoNotRespond(
    CtapRequestCommand command) {
  EXPECT_CALL(*this, DeviceTransactPtr(IsCtap2Command(command), ::testing::_))
      .WillOnce(::testing::Return(0));
}

void MockFidoDevice::ExpectRequestAndDoNotRespond(
    base::span<const uint8_t> request) {
  auto request_as_vector = fido_parsing_utils::Materialize(request);
  EXPECT_CALL(*this,
              DeviceTransactPtr(std::move(request_as_vector), ::testing::_))
      .WillOnce(::testing::Return(0));
}

void MockFidoDevice::SetDeviceTransport(
    FidoTransportProtocol transport_protocol) {
  transport_protocol_ = transport_protocol;
}

void MockFidoDevice::StubGetDisplayName() {
  EXPECT_CALL(*this, GetDisplayName())
      .WillRepeatedly(testing::Return(std::string()));
}

}  // namespace device
