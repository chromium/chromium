// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/hid/fido_hid_device.h"

#include <array>
#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/hid/fido_hid_message.h"
#include "device/fido/test_callback_receiver.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/cpp/hid/hid_device_filter.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

using ::testing::_;
using ::testing::Invoke;

namespace {

// HID_MSG(83), followed by payload length(000b), followed by response data
// "MOCK_DATA", followed by APDU SW_NO_ERROR response code(9000).
constexpr uint8_t kU2fMockResponseMessage[] = {
    0x83, 0x00, 0x0b, 0x4d, 0x4f, 0x43, 0x4b,
    0x5f, 0x44, 0x41, 0x54, 0x41, 0x90, 0x00,
};

// HID_WINK(0x08), followed by payload length(0).
constexpr uint8_t kU2fWinkResponseMessage[] = {0x08, 0x00};

// APDU encoded success response with data "MOCK_DATA" followed by a SW_NO_ERROR
// APDU response code(9000).
constexpr uint8_t kU2fMockResponseData[] = {0x4d, 0x4f, 0x43, 0x4b, 0x5f, 0x44,
                                            0x41, 0x54, 0x41, 0x90, 0x00};

// HID_ERROR(BF), followed by payload length(0001), followed by
// kInvalidCommand(01).
constexpr uint8_t kHidUnknownCommandError[] = {0xBF, 0x00, 0x01, 0x01};

// HID_KEEP_ALIVE(bb), followed by payload length(0001), followed by
// status processing(01) byte.
constexpr uint8_t kMockKeepAliveResponseSuffix[] = {0xbb, 0x00, 0x01, 0x01};

// 4 byte broadcast channel id(ffffffff), followed by an HID_INIT command(86),
// followed by a fixed size payload length(11). 8 byte nonce and 4 byte channel
// ID must be appended to create a well formed  HID_INIT packet.
constexpr uint8_t kInitResponsePrefix[] = {
    0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11,
};

// Mock APDU encoded U2F request with empty data and mock P1 parameter(0x04).
constexpr uint8_t kMockU2fRequest[] = {0x00, 0x04, 0x00, 0x00,
                                       0x00, 0x00, 0x00};

constexpr uint8_t kMockCancelResponse[] = {
    // clang-format off
    0x90,        // CTAPHID_CBOR
    0, 1,        // one byte payload
    0x2d,        // CTAP2_ERR_KEEPALIVE_CANCEL
    // clang-format on
};

constexpr std::array<uint8_t, 4> kChannelId = {0x01, 0x02, 0x03, 0x04};

// Returns HID_INIT request to send to device with mock connection.
std::vector<uint8_t> CreateMockInitResponse(
    base::span<const uint8_t> nonce,
    base::span<const uint8_t> channel_id,
    base::span<const uint8_t> payload = base::span<const uint8_t>()) {
  auto init_response = fido_parsing_utils::Materialize(kInitResponsePrefix);
  fido_parsing_utils::Append(&init_response, nonce);
  fido_parsing_utils::Append(&init_response, channel_id);
  fido_parsing_utils::Append(&init_response, payload);
  init_response.resize(64);
  return init_response;
}

// Returns HID keep alive message encoded into HID packet format.
std::vector<uint8_t> GetKeepAliveHidMessage(
    base::span<const uint8_t> channel_id) {
  auto response = fido_parsing_utils::Materialize(channel_id);
  fido_parsing_utils::Append(&response, kMockKeepAliveResponseSuffix);
  response.resize(64);
  return response;
}

// Returns "U2F_v2" as a mock response to version request with given channel id.
std::vector<uint8_t> CreateMockResponseWithChannelId(
    base::span<const uint8_t> channel_id,
    base::span<const uint8_t> response_buffer) {
  auto response = fido_parsing_utils::Materialize(channel_id);
  fido_parsing_utils::Append(&response, response_buffer);
  response.resize(64);
  return response;
}

// Returns a APDU encoded U2F version request for testing.
std::vector<uint8_t> GetMockDeviceRequest() {
  return fido_parsing_utils::Materialize(kMockU2fRequest);
}

device::mojom::HidDeviceInfoPtr TestHidDevice() {
  auto c_info = device::mojom::HidCollectionInfo::New();
  c_info->usage = device::mojom::HidUsageAndPage::New(1, 0xf1d0);
  auto hid_device = device::mojom::HidDeviceInfo::New();
  hid_device->guid = "A";
  hid_device->product_name = "Test Fido device";
  hid_device->serial_number = "123FIDO";
  hid_device->bus_type = device::mojom::HidBusType::kHIDBusTypeUSB;
  hid_device->collections.push_back(std::move(c_info));
  hid_device->max_input_report_size = 64;
  hid_device->max_output_report_size = 64;
  return hid_device;
}

std::unique_ptr<MockFidoHidConnection>
CreateHidConnectionWithHidInitExpectations(
    const std::array<uint8_t, 4>& channel_id,
    FakeFidoHidManager* fake_hid_manager,
    ::testing::Sequence sequence) {
  auto hid_device = TestHidDevice();
  mojo::PendingRemote<device::mojom::HidConnection> connection_client;

  // Replace device HID connection with custom client connection bound to mock
  // server-side mojo connection.
  auto mock_connection = std::make_unique<MockFidoHidConnection>(
      hid_device.Clone(), connection_client.InitWithNewPipeAndPassReceiver(),
      channel_id);

  // Initial write for establishing channel ID.
  mock_connection->ExpectWriteHidInit();

  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      // Response to HID_INIT request.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(
            true, 0,
            CreateMockInitResponse(mock_connection->nonce(),
                                   mock_connection->connection_channel_id()));
      }));

  // Add device and set mock connection to fake hid manager.
  fake_hid_manager->AddDeviceAndSetConnection(std::move(hid_device),
                                              std::move(connection_client));
  return mock_connection;
}

// Set up expectations on mock_connection to read a potentially multi-packet
// response.
void SetupReadExpectation(MockFidoHidConnection* mock_connection,
                          FidoHidDeviceCommand command_type,
                          base::span<const uint8_t> payload,
                          ::testing::Sequence sequence) {
  auto channel_id_vector = mock_connection->connection_channel_id();
  uint32_t channel_id = channel_id_vector[0] << 24 |
                        channel_id_vector[1] << 16 | channel_id_vector[2] << 8 |
                        channel_id_vector[3];
  auto message = FidoHidMessage::Create(channel_id, command_type,
                                        kHidMaxPacketSize, payload);

  while (message->NumPackets() != 0) {
    EXPECT_CALL(*mock_connection, ReadPtr(_))
        .InSequence(sequence)
        .WillOnce(Invoke([packet = message->PopNextPacket()](
                             device::mojom::HidConnection::ReadCallback* cb) {
          std::move(*cb).Run(true, 0, std::move(packet));
        }));
  }
}

class FidoDeviceEnumerateCallbackReceiver
    : public test::TestCallbackReceiver<std::vector<mojom::HidDeviceInfoPtr>> {
 public:
  explicit FidoDeviceEnumerateCallbackReceiver(
      device::mojom::HidManager* hid_manager)
      : hid_manager_(hid_manager) {}
  ~FidoDeviceEnumerateCallbackReceiver() = default;

  std::vector<std::unique_ptr<FidoHidDevice>> TakeReturnedDevicesFiltered() {
    std::vector<std::unique_ptr<FidoHidDevice>> filtered_results;
    std::vector<mojom::HidDeviceInfoPtr> results;
    std::tie(results) = TakeResult();
    for (auto& device_info : results) {
      HidDeviceFilter filter;
      filter.SetUsagePage(0xf1d0);
      if (filter.Matches(*device_info)) {
        filtered_results.push_back(std::make_unique<FidoHidDevice>(
            std::move(device_info), hid_manager_));
      }
    }
    return filtered_results;
  }

 private:
  device::mojom::HidManager* hid_manager_;

  DISALLOW_COPY_AND_ASSIGN(FidoDeviceEnumerateCallbackReceiver);
};

using TestDeviceCallbackReceiver =
    ::device::test::ValueCallbackReceiver<base::Optional<std::vector<uint8_t>>>;

}  // namespace

class FidoHidDeviceTest : public ::testing::Test {
 public:
  void SetUp() override {
    fake_hid_manager_ = std::make_unique<FakeFidoHidManager>();
    fake_hid_manager_->AddReceiver2(hid_manager_.BindNewPipeAndPassReceiver());
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  mojo::Remote<device::mojom::HidManager> hid_manager_;
  std::unique_ptr<FakeFidoHidManager> fake_hid_manager_;
};

TEST_F(FidoHidDeviceTest, TestDeviceError) {
  // Setup and enumerate mock device.
  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());

  auto hid_device = TestHidDevice();
  fake_hid_manager_->AddDevice(std::move(hid_device));
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();

  ASSERT_EQ(static_cast<size_t>(1), u2f_devices.size());
  auto& device = u2f_devices.front();

  // Mock connection where writes always fail.
  FakeFidoHidConnection::mock_connection_error_ = true;

  TestDeviceCallbackReceiver receiver_0;
  device->DeviceTransact(GetMockDeviceRequest(), receiver_0.callback());
  receiver_0.WaitForCallback();
  EXPECT_FALSE(receiver_0.value());
  EXPECT_EQ(FidoDevice::State::kDeviceError, device->state_);

  // Add pending transactions manually and ensure they are processed.
  TestDeviceCallbackReceiver receiver_1;
  device->pending_transactions_.emplace_back(FidoHidDeviceCommand::kMsg,
                                             GetMockDeviceRequest(),
                                             receiver_1.callback(), 0);
  TestDeviceCallbackReceiver receiver_2;
  device->pending_transactions_.emplace_back(FidoHidDeviceCommand::kMsg,
                                             GetMockDeviceRequest(),
                                             receiver_2.callback(), 0);
  TestDeviceCallbackReceiver receiver_3;
  device->DeviceTransact(GetMockDeviceRequest(), receiver_3.callback());
  FakeFidoHidConnection::mock_connection_error_ = false;

  EXPECT_EQ(FidoDevice::State::kDeviceError, device->state_);
  EXPECT_FALSE(receiver_1.value());
  EXPECT_FALSE(receiver_2.value());
  EXPECT_FALSE(receiver_3.value());
}

TEST_F(FidoHidDeviceTest, TestRetryChannelAllocation) {
  constexpr uint8_t kIncorrectNonce[] = {0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00};
  auto hid_device = TestHidDevice();

  // Replace device HID connection with custom client connection bound to mock
  // server-side mojo connection.
  mojo::PendingRemote<device::mojom::HidConnection> connection_client;
  MockFidoHidConnection mock_connection(
      hid_device.Clone(), connection_client.InitWithNewPipeAndPassReceiver(),
      kChannelId);

  // Initial write for establishing a channel ID.
  mock_connection.ExpectWriteHidInit();

  // HID_MSG request to authenticator for version request.
  mock_connection.ExpectHidWriteWithCommand(FidoHidDeviceCommand::kMsg);

  EXPECT_CALL(mock_connection, ReadPtr(_))
      // First response to HID_INIT request with an incorrect nonce.
      .WillOnce(Invoke([kIncorrectNonce, &mock_connection](auto* cb) {
        std::move(*cb).Run(
            true, 0,
            CreateMockInitResponse(kIncorrectNonce,
                                   mock_connection.connection_channel_id()));
      }))
      // Second response to HID_INIT request with a correct nonce.
      .WillOnce(Invoke(
          [&mock_connection](device::mojom::HidConnection::ReadCallback* cb) {
            std::move(*cb).Run(true, 0,
                               CreateMockInitResponse(
                                   mock_connection.nonce(),
                                   mock_connection.connection_channel_id()));
          }))
      // Version response from the authenticator.
      .WillOnce(Invoke(
          [&mock_connection](device::mojom::HidConnection::ReadCallback* cb) {
            std::move(*cb).Run(true, 0,
                               CreateMockResponseWithChannelId(
                                   mock_connection.connection_channel_id(),
                                   kU2fMockResponseMessage));
          }));

  // Add device and set mock connection to fake hid manager.
  fake_hid_manager_->AddDeviceAndSetConnection(std::move(hid_device),
                                               std::move(connection_client));
  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  TestDeviceCallbackReceiver cb;
  device->DeviceTransact(GetMockDeviceRequest(), cb.callback());
  cb.WaitForCallback();

  const auto& value = cb.value();
  ASSERT_TRUE(value);
  EXPECT_THAT(*value, testing::ElementsAreArray(kU2fMockResponseData));
}

TEST_F(FidoHidDeviceTest, TestKeepAliveMessage) {
  ::testing::Sequence sequence;
  auto mock_connection = CreateHidConnectionWithHidInitExpectations(
      kChannelId, fake_hid_manager_.get(), sequence);

  // HID_CBOR request to authenticator.
  mock_connection->ExpectHidWriteWithCommand(FidoHidDeviceCommand::kCbor);

  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      // Keep alive message sent from the authenticator.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(
            true, 0,
            GetKeepAliveHidMessage(mock_connection->connection_channel_id()));
      }))
      // Repeated Read() invocation due to keep alive message. Sends a dummy
      // response that corresponds to U2F version response.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        auto almost_time_out =
            kDeviceTimeout - base::TimeDelta::FromMicroseconds(1);
        task_environment_.FastForwardBy(almost_time_out);

        std::move(*cb).Run(true, 0,
                           CreateMockResponseWithChannelId(
                               mock_connection->connection_channel_id(),
                               kU2fMockResponseMessage));
      }));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  // Keep alive message handling is only supported for CTAP HID device.
  device->set_supported_protocol(ProtocolVersion::kCtap2);
  TestDeviceCallbackReceiver cb;
  device->DeviceTransact(GetMockDeviceRequest(), cb.callback());
  cb.WaitForCallback();
  const auto& value = cb.value();
  ASSERT_TRUE(value);
  EXPECT_THAT(*value, testing::ElementsAreArray(kU2fMockResponseData));
}

// InvertChannelID inverts all the bits in the given channel ID. This is used to
// create a channel ID that will not be equal to the expected channel ID.
std::array<uint8_t, 4> InvertChannelID(
    const std::array<uint8_t, 4> channel_id) {
  std::array<uint8_t, 4> ret;
  memcpy(ret.data(), channel_id.data(), ret.size());
  for (size_t i = 0; i < ret.size(); i++) {
    ret[i] ^= 0xff;
  }
  return ret;
}

TEST_F(FidoHidDeviceTest, TestMessageOnOtherChannel) {
  // Test that a HID message with a different channel ID is ignored.
  ::testing::Sequence sequence;
  auto mock_connection = CreateHidConnectionWithHidInitExpectations(
      kChannelId, fake_hid_manager_.get(), sequence);

  // HID_CBOR request to authenticator.
  mock_connection->ExpectHidWriteWithCommand(FidoHidDeviceCommand::kMsg);

  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      // Message on wrong channel.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(
            true, 0,
            CreateMockResponseWithChannelId(
                InvertChannelID(mock_connection->connection_channel_id()),
                kHidUnknownCommandError));
      }))
      // Expected message on the correct channel.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(true, 0,
                           CreateMockResponseWithChannelId(
                               mock_connection->connection_channel_id(),
                               kU2fMockResponseMessage));
      }));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  TestDeviceCallbackReceiver cb;
  device->DeviceTransact(GetMockDeviceRequest(), cb.callback());
  cb.WaitForCallback();
  const auto& value = cb.value();
  ASSERT_TRUE(value);
  EXPECT_THAT(*value, testing::ElementsAreArray(kU2fMockResponseData));
}

TEST_F(FidoHidDeviceTest, TestContinuedMessageOnOtherChannel) {
  // Test that a multi-frame HID message with a different channel ID is
  // ignored.
  ::testing::Sequence sequence;
  auto mock_connection = CreateHidConnectionWithHidInitExpectations(
      kChannelId, fake_hid_manager_.get(), sequence);

  // HID_CBOR request to authenticator.
  mock_connection->ExpectHidWriteWithCommand(FidoHidDeviceCommand::kMsg);

  constexpr uint8_t kOtherChannelMsgPrefix[64] = {
      0x83,
      0x00,
      // Mark reply as being 64 bytes long, which is more than a single USB
      // frame can contain.
      0x40,
      0,
  };

  constexpr uint8_t kOtherChannelMsgSuffix[64] = {
      // Continuation packet zero.
      0x00,
      // Contents can be anything.
  };

  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      // Beginning of a message on the wrong channel.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(
            true, 0,
            CreateMockResponseWithChannelId(
                InvertChannelID(mock_connection->connection_channel_id()),
                kOtherChannelMsgPrefix));
      }))
      // Continuation of the message on the wrong channel.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(
            true, 0,
            CreateMockResponseWithChannelId(
                InvertChannelID(mock_connection->connection_channel_id()),
                kOtherChannelMsgSuffix));
      }))
      // Expected message on the correct channel.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(true, 0,
                           CreateMockResponseWithChannelId(
                               mock_connection->connection_channel_id(),
                               kU2fMockResponseMessage));
      }));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  TestDeviceCallbackReceiver cb;
  device->DeviceTransact(GetMockDeviceRequest(), cb.callback());
  cb.WaitForCallback();
  const auto& value = cb.value();
  ASSERT_TRUE(value);
  EXPECT_THAT(*value, testing::ElementsAreArray(kU2fMockResponseData));
}

TEST_F(FidoHidDeviceTest, TestDeviceTimeoutAfterKeepAliveMessage) {
  ::testing::Sequence sequence;
  auto mock_connection = CreateHidConnectionWithHidInitExpectations(
      kChannelId, fake_hid_manager_.get(), sequence);

  // HID_CBOR request to authenticator.
  mock_connection->ExpectHidWriteWithCommand(FidoHidDeviceCommand::kCbor);

  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      // Keep alive message sent from the authenticator.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(
            true, 0,
            GetKeepAliveHidMessage(mock_connection->connection_channel_id()));
      }))
      // Repeated Read() invocation due to keep alive message. The callback
      // is invoked only after 3 seconds, which should cause device to timeout.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        task_environment_.FastForwardBy(kDeviceTimeout);
        std::move(*cb).Run(true, 0,
                           CreateMockResponseWithChannelId(
                               mock_connection->connection_channel_id(),
                               kU2fMockResponseMessage));
      }));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  // Keep alive message handling is only supported for CTAP HID device.
  device->set_supported_protocol(ProtocolVersion::kCtap2);
  TestDeviceCallbackReceiver cb;
  device->DeviceTransact(GetMockDeviceRequest(), cb.callback());
  cb.WaitForCallback();
  const auto& value = cb.value();
  EXPECT_FALSE(value);
  EXPECT_EQ(FidoDevice::State::kDeviceError, device->state_for_testing());
}

TEST_F(FidoHidDeviceTest, TestCancel) {
  ::testing::Sequence sequence;
  auto mock_connection = CreateHidConnectionWithHidInitExpectations(
      kChannelId, fake_hid_manager_.get(), sequence);

  // HID_CBOR request to authenticator.
  mock_connection->ExpectHidWriteWithCommand(FidoHidDeviceCommand::kCbor);

  // Cancel request to authenticator.
  mock_connection->ExpectHidWriteWithCommand(FidoHidDeviceCommand::kCancel);

  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      // Device response with a significant delay.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        auto delay = base::TimeDelta::FromSeconds(2);
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(std::move(*cb), true, 0,
                           CreateMockResponseWithChannelId(
                               mock_connection->connection_channel_id(),
                               kU2fMockResponseMessage)),
            delay);
      }));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  // Keep alive message handling is only supported for CTAP HID device.
  device->set_supported_protocol(ProtocolVersion::kCtap2);
  TestDeviceCallbackReceiver cb;
  auto token = device->DeviceTransact(GetMockDeviceRequest(), cb.callback());
  auto delay_before_cancel = base::TimeDelta::FromSeconds(1);
  auto cancel_callback = base::BindOnce(
      &FidoHidDevice::Cancel, device->weak_factory_.GetWeakPtr(), token);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, std::move(cancel_callback), delay_before_cancel);
  task_environment_.FastForwardUntilNoTasksRemain();
}

TEST_F(FidoHidDeviceTest, TestCancelWhileWriting) {
  // Simulate a cancelation request that occurs while the request is being
  // written.
  ::testing::Sequence sequence;
  auto mock_connection = CreateHidConnectionWithHidInitExpectations(
      kChannelId, fake_hid_manager_.get(), sequence);

  FidoDevice::CancelToken token = FidoDevice::kInvalidCancelToken;
  FidoDevice* device = nullptr;

  EXPECT_CALL(*mock_connection, WritePtr(_, _, _))
      .InSequence(sequence)
      .WillOnce(Invoke(
          [&token, &device](auto&&, const std::vector<uint8_t>& buffer,
                            device::mojom::HidConnection::WriteCallback* cb) {
            device->Cancel(token);
            std::move(*cb).Run(true);
          }));
  EXPECT_CALL(*mock_connection, WritePtr(_, _, _))
      .InSequence(sequence)
      .WillOnce(Invoke([](auto&&, const std::vector<uint8_t>& buffer,
                          device::mojom::HidConnection::WriteCallback* cb) {
        std::move(*cb).Run(true);
      }));
  EXPECT_CALL(*mock_connection, WritePtr(_, _, _))
      .InSequence(sequence)
      .WillOnce(Invoke([](auto&&, const std::vector<uint8_t>& buffer,
                          device::mojom::HidConnection::WriteCallback* cb) {
        CHECK_LE(5u, buffer.size());
        CHECK_EQ(static_cast<uint8_t>(FidoHidDeviceCommand::kCancel) | 0x80,
                 buffer[4]);
        std::move(*cb).Run(true);
      }));
  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      .WillOnce(Invoke(
          [&mock_connection](device::mojom::HidConnection::ReadCallback* cb) {
            std::move(*cb).Run(true, 0,
                               CreateMockResponseWithChannelId(
                                   mock_connection->connection_channel_id(),
                                   kMockCancelResponse));
          }));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  device = u2f_devices.front().get();

  // Keep alive message handling is only supported for CTAP HID device.
  device->set_supported_protocol(ProtocolVersion::kCtap2);
  TestDeviceCallbackReceiver cb;
  // The size of |dummy_request| needs only to make the request need two USB
  // frames.
  std::vector<uint8_t> dummy_request(100);
  token = device->DeviceTransact(std::move(dummy_request), cb.callback());
  cb.WaitForCallback();
  ASSERT_TRUE(cb.value());
  ASSERT_EQ(1u, cb.value()->size());
  ASSERT_EQ(0x2d /* CTAP2_ERR_KEEPALIVE_CANCEL */, cb.value().value()[0]);
}

TEST_F(FidoHidDeviceTest, TestCancelAfterWriting) {
  // Simulate a cancelation request that occurs while waiting for a response.
  ::testing::Sequence sequence;
  auto mock_connection = CreateHidConnectionWithHidInitExpectations(
      kChannelId, fake_hid_manager_.get(), sequence);

  FidoDevice::CancelToken token = FidoDevice::kInvalidCancelToken;
  FidoDevice* device = nullptr;
  device::mojom::HidConnection::ReadCallback read_callback;

  EXPECT_CALL(*mock_connection, WritePtr(_, _, _))
      .InSequence(sequence)
      .WillOnce(Invoke([](auto&&, const std::vector<uint8_t>& buffer,
                          device::mojom::HidConnection::WriteCallback* cb) {
        std::move(*cb).Run(true);
      }));
  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      .WillOnce(Invoke([&read_callback, &device, &token](
                           device::mojom::HidConnection::ReadCallback* cb) {
        read_callback = std::move(*cb);
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE,
            base::BindOnce(
                [](FidoDevice* device, FidoDevice::CancelToken token) {
                  device->Cancel(token);
                },
                device, token));
      }));
  EXPECT_CALL(*mock_connection, WritePtr(_, _, _))
      .InSequence(sequence)
      .WillOnce(Invoke([&mock_connection, &read_callback](
                           auto&&, const std::vector<uint8_t>& buffer,
                           device::mojom::HidConnection::WriteCallback* cb) {
        CHECK_LE(5u, buffer.size());
        CHECK_EQ(static_cast<uint8_t>(FidoHidDeviceCommand::kCancel) | 0x80,
                 buffer[4]);
        std::move(*cb).Run(true);
        std::move(read_callback)
            .Run(true, 0,
                 CreateMockResponseWithChannelId(
                     mock_connection->connection_channel_id(),
                     kMockCancelResponse));
      }));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  device = u2f_devices.front().get();

  // Cancelation is only supported for CTAP HID device.
  device->set_supported_protocol(ProtocolVersion::kCtap2);
  TestDeviceCallbackReceiver cb;
  std::vector<uint8_t> dummy_request(1);
  token = device->DeviceTransact(std::move(dummy_request), cb.callback());
  cb.WaitForCallback();
  ASSERT_TRUE(cb.value());
  ASSERT_EQ(1u, cb.value()->size());
  ASSERT_EQ(0x2d /* CTAP2_ERR_KEEPALIVE_CANCEL */, cb.value().value()[0]);
}

TEST_F(FidoHidDeviceTest, TestCancelAfterReading) {
  // Simulate a cancelation request that occurs after the first frame of the
  // response has been received.
  ::testing::Sequence sequence;
  auto mock_connection = CreateHidConnectionWithHidInitExpectations(
      kChannelId, fake_hid_manager_.get(), sequence);

  FidoDevice::CancelToken token = FidoDevice::kInvalidCancelToken;
  FidoDevice* device = nullptr;
  device::mojom::HidConnection::ReadCallback read_callback;

  EXPECT_CALL(*mock_connection, WritePtr(_, _, _))
      .InSequence(sequence)
      .WillOnce(Invoke([](auto&&, const std::vector<uint8_t>& buffer,
                          device::mojom::HidConnection::WriteCallback* cb) {
        std::move(*cb).Run(true);
      }));
  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      .WillOnce(Invoke(
          [&mock_connection](device::mojom::HidConnection::ReadCallback* cb) {
            std::vector<uint8_t> frame = {0x90, 0, 64};
            frame.resize(64, 0);
            std::move(*cb).Run(true, 0,
                               CreateMockResponseWithChannelId(
                                   mock_connection->connection_channel_id(),
                                   std::move(frame)));
          }));
  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      .WillOnce(Invoke([&device, &token, &mock_connection](
                           device::mojom::HidConnection::ReadCallback* cb) {
        // This |Cancel| call should be a no-op because the response has already
        // started to be received.
        device->Cancel(token);

        std::vector<uint8_t> frame;
        frame.resize(64, 0);
        std::move(*cb).Run(
            true, 0,
            CreateMockResponseWithChannelId(
                mock_connection->connection_channel_id(), std::move(frame)));
      }));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  device = u2f_devices.front().get();

  // Cancelation is only supported for CTAP HID device.
  device->set_supported_protocol(ProtocolVersion::kCtap2);
  TestDeviceCallbackReceiver cb;
  std::vector<uint8_t> dummy_request(1);
  token = device->DeviceTransact(std::move(dummy_request), cb.callback());
  cb.WaitForCallback();
  ASSERT_TRUE(cb.value());
  ASSERT_EQ(64u, cb.value()->size());
}

TEST_F(FidoHidDeviceTest, TestGetInfoFailsOnDeviceError) {
  // HID_ERROR(7F), followed by payload length(0001), followed by kUnknown(7F).
  constexpr uint8_t kHidUnknownTransportError[] = {0x7F, 0x00, 0x01, 0x7F};
  ::testing::Sequence sequence;
  auto mock_connection = CreateHidConnectionWithHidInitExpectations(
      kChannelId, fake_hid_manager_.get(), sequence);

  // HID_CBOR request to authenticator.
  mock_connection->ExpectHidWriteWithCommand(FidoHidDeviceCommand::kCbor);

  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      // Device response with a significant delay.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        auto delay = base::TimeDelta::FromSeconds(2);
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(std::move(*cb), true, 0,
                           CreateMockResponseWithChannelId(
                               mock_connection->connection_channel_id(),
                               kHidUnknownTransportError)),
            delay);
      }));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  device::test::TestCallbackReceiver<> get_info_callback;
  device->DiscoverSupportedProtocolAndDeviceInfo(get_info_callback.callback());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_info_callback.was_called());
  EXPECT_EQ(FidoDevice::State::kDeviceError, device->state_for_testing());
}

// Test that FidoHidDevice::DiscoverSupportedProtocolAndDeviceInfo() invokes
// callback when device error outs with kMsgError state.
TEST_F(FidoHidDeviceTest, TestDeviceMessageError) {
  ::testing::Sequence sequence;
  auto mock_connection = CreateHidConnectionWithHidInitExpectations(
      kChannelId, fake_hid_manager_.get(), sequence);

  // HID_CBOR request to authenticator.
  mock_connection->ExpectHidWriteWithCommand(FidoHidDeviceCommand::kCbor);

  EXPECT_CALL(*mock_connection, ReadPtr(_))
      .InSequence(sequence)
      // Device response with a significant delay.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        auto delay = base::TimeDelta::FromSeconds(2);
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(std::move(*cb), true, 0,
                           CreateMockResponseWithChannelId(
                               mock_connection->connection_channel_id(),
                               kHidUnknownCommandError)),
            delay);
      }));

  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  device::test::TestCallbackReceiver<> get_info_callback;
  device->DiscoverSupportedProtocolAndDeviceInfo(get_info_callback.callback());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(get_info_callback.was_called());
}

// Test that the wink command does not get sent if the device does not support
// it.
TEST_F(FidoHidDeviceTest, TestWinkNotSupported) {
  constexpr uint8_t kWinkNotSupportedPayload[] = {0x00, 0x00, 0x00, 0x00, 0x00};

  auto hid_device = TestHidDevice();

  // Replace device HID connection with custom client connection bound to mock
  // server-side mojo connection.
  mojo::PendingRemote<device::mojom::HidConnection> connection_client;
  MockFidoHidConnection mock_connection(
      hid_device.Clone(), connection_client.InitWithNewPipeAndPassReceiver(),
      kChannelId);

  // Initial write for establishing a channel ID.
  mock_connection.ExpectWriteHidInit();

  // GetInfo command.
  mock_connection.ExpectHidWriteWithCommand(FidoHidDeviceCommand::kCbor);

  EXPECT_CALL(mock_connection, ReadPtr(_))
      // Respond to HID_INIT indicating the device does not support winking.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(
            true, 0,
            CreateMockInitResponse(mock_connection.nonce(),
                                   mock_connection.connection_channel_id(),
                                   kWinkNotSupportedPayload));
      }))
      // Respond to GetInfo with kHidUnknownCommandError to signal this is a
      // U2F device.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(true, 0,
                           CreateMockResponseWithChannelId(
                               mock_connection.connection_channel_id(),
                               kHidUnknownCommandError));
      }));

  // Add device and set mock connection to fake hid manager.
  fake_hid_manager_->AddDeviceAndSetConnection(std::move(hid_device),
                                               std::move(connection_client));
  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  device::test::TestCallbackReceiver<> callback_receiver;
  device->DiscoverSupportedProtocolAndDeviceInfo(callback_receiver.callback());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback_receiver.was_called());

  device->TryWink(callback_receiver.callback());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback_receiver.was_called());
}

// Test that the wink command does not get sent for CTAP2 devices, even if they
// support it.
// This is a workaround for crbug.com/994867
TEST_F(FidoHidDeviceTest, TestCtap2DeviceShouldNotBlink) {
  constexpr uint8_t kWinkSupportedPayload[] = {0x00, 0x00, 0x00, 0x00, 0x01};

  auto hid_device = TestHidDevice();

  // Replace device HID connection with custom client connection bound to mock
  // server-side mojo connection.
  mojo::PendingRemote<device::mojom::HidConnection> connection_client;
  MockFidoHidConnection mock_connection(
      hid_device.Clone(), connection_client.InitWithNewPipeAndPassReceiver(),
      kChannelId);

  // Initial write for establishing a channel ID.
  mock_connection.ExpectWriteHidInit();
  // Write for the GetInfo command.
  mock_connection.ExpectHidWriteWithCommand(FidoHidDeviceCommand::kCbor);

  ::testing::Sequence sequence;

  EXPECT_CALL(mock_connection, ReadPtr(_))
      // Respond to HID_INIT indicating the device supports winking.
      .InSequence(sequence)
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(
            true, 0,
            CreateMockInitResponse(mock_connection.nonce(),
                                   mock_connection.connection_channel_id(),
                                   kWinkSupportedPayload));
      }));

  SetupReadExpectation(&mock_connection, FidoHidDeviceCommand::kCbor,
                       test_data::kTestAuthenticatorGetInfoResponse, sequence);

  // Add device and set mock connection to fake hid manager.
  fake_hid_manager_->AddDeviceAndSetConnection(std::move(hid_device),
                                               std::move(connection_client));
  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  device::test::TestCallbackReceiver<> callback_receiver;
  device->DiscoverSupportedProtocolAndDeviceInfo(callback_receiver.callback());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback_receiver.was_called());

  device->TryWink(callback_receiver.callback());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback_receiver.was_called());
}

// Test that the wink command is sent to a device that supports it.
TEST_F(FidoHidDeviceTest, TestSuccessfulWink) {
  constexpr uint8_t kWinkSupportedPayload[] = {0x00, 0x00, 0x00, 0x00, 0x01};

  auto hid_device = TestHidDevice();

  // Replace device HID connection with custom client connection bound to mock
  // server-side mojo connection.
  mojo::PendingRemote<device::mojom::HidConnection> connection_client;
  MockFidoHidConnection mock_connection(
      hid_device.Clone(), connection_client.InitWithNewPipeAndPassReceiver(),
      kChannelId);

  // Initial write for establishing a channel ID.
  mock_connection.ExpectWriteHidInit();
  // GetInfo write.
  mock_connection.ExpectHidWriteWithCommand(FidoHidDeviceCommand::kCbor);
  mock_connection.ExpectHidWriteWithCommand(FidoHidDeviceCommand::kWink);

  EXPECT_CALL(mock_connection, ReadPtr(_))
      // Respond to HID_INIT indicating the device supports winking.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(
            true, 0,
            CreateMockInitResponse(mock_connection.nonce(),
                                   mock_connection.connection_channel_id(),
                                   kWinkSupportedPayload));
      }))
      // Respond to GetInfo with kHidUnknownCommandError to signal this is a
      // U2F device.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(true, 0,
                           CreateMockResponseWithChannelId(
                               mock_connection.connection_channel_id(),
                               kHidUnknownCommandError));
      }))
      // Response to HID_WINK.
      .WillOnce(Invoke([&](device::mojom::HidConnection::ReadCallback* cb) {
        std::move(*cb).Run(true, 0,
                           CreateMockResponseWithChannelId(
                               mock_connection.connection_channel_id(),
                               kU2fWinkResponseMessage));
      }));

  // Add device and set mock connection to fake hid manager.
  fake_hid_manager_->AddDeviceAndSetConnection(std::move(hid_device),
                                               std::move(connection_client));
  FidoDeviceEnumerateCallbackReceiver receiver(hid_manager_.get());
  hid_manager_->GetDevices(receiver.callback());
  receiver.WaitForCallback();

  std::vector<std::unique_ptr<FidoHidDevice>> u2f_devices =
      receiver.TakeReturnedDevicesFiltered();
  ASSERT_EQ(1u, u2f_devices.size());
  auto& device = u2f_devices.front();

  device::test::TestCallbackReceiver<> callback_receiver;
  device->DiscoverSupportedProtocolAndDeviceInfo(callback_receiver.callback());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback_receiver.was_called());

  device->TryWink(callback_receiver.callback());
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(callback_receiver.was_called());
}

}  // namespace device
