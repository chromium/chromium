// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ble/fido_ble_device.h"

#include "base/bind.h"
#include "base/optional.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/bluetooth_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/fido/ble/fido_ble_uuids.h"
#include "device/fido/ble/mock_fido_ble_connection.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Test;
using ::testing::WithoutArgs;
using TestDeviceCallbackReceiver =
    test::ValueCallbackReceiver<base::Optional<std::vector<uint8_t>>>;

using NiceMockBluetoothAdapter = ::testing::NiceMock<MockBluetoothAdapter>;
using NiceMockBluetoothDevice = ::testing::NiceMock<MockBluetoothDevice>;

constexpr uint16_t kControlPointLength = 20;
constexpr uint8_t kTestData[] = {'T', 'E', 'S', 'T'};
// BLE cancel command, followed bytes 2 bytes of zero length payload.
constexpr uint8_t kBleCancelCommand[] = {0xBE, 0x00, 0x00};

std::vector<std::vector<uint8_t>> ToSerializedFragments(
    FidoBleDeviceCommand command,
    std::vector<uint8_t> payload,
    size_t max_fragment_size) {
  FidoBleFrame frame(command, std::move(payload));
  auto fragments = frame.ToFragments(max_fragment_size);

  const size_t num_fragments = 1 /* init_fragment */ + fragments.second.size();
  std::vector<std::vector<uint8_t>> serialized_fragments(num_fragments);

  fragments.first.Serialize(&serialized_fragments[0]);
  for (size_t i = 1; i < num_fragments; ++i) {
    fragments.second.front().Serialize(&serialized_fragments[i]);
    fragments.second.pop();
  }

  return serialized_fragments;
}

void SetAdvertisingDataFlags(BluetoothDevice* device,
                             base::Optional<uint8_t> flags) {
  device->UpdateAdvertisementData(
      0 /* rssi */, std::move(flags), BluetoothDevice::UUIDList(),
      base::nullopt /* tx_power */, BluetoothDevice::ServiceDataMap(),
      BluetoothDevice::ManufacturerDataMap());
}

void SetServiceData(BluetoothDevice* device,
                    BluetoothDevice::ServiceDataMap service_data) {
  device->UpdateAdvertisementData(
      0 /* rssi */, base::nullopt /* flags */, BluetoothDevice::UUIDList(),
      base::nullopt /* tx_power */, std::move(service_data),
      BluetoothDevice::ManufacturerDataMap());
}

}  // namespace

class FidoBleDeviceTest : public Test {
 public:
  FidoBleDeviceTest() {
    auto connection = std::make_unique<MockFidoBleConnection>(
        adapter_.get(), BluetoothTestBase::kTestDeviceAddress1);
    connection_ = connection.get();
    device_ = std::make_unique<FidoBleDevice>(std::move(connection));
    connection_->read_callback() = device_->GetReadCallbackForTesting();
  }

  MockBluetoothAdapter* adapter() { return adapter_.get(); }
  FidoBleDevice* device() { return device_.get(); }
  MockFidoBleConnection* connection() { return connection_; }

  void ConnectWithLength(uint16_t length) {
    EXPECT_CALL(*connection(), ConnectPtr).WillOnce(Invoke([](auto* callback) {
      std::move(*callback).Run(true);
    }));

    EXPECT_CALL(*connection(), ReadControlPointLengthPtr(_))
        .WillOnce(Invoke([length](auto* cb) { std::move(*cb).Run(length); }));

    device()->Connect();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<NiceMockBluetoothAdapter>();
  MockFidoBleConnection* connection_;
  std::unique_ptr<FidoBleDevice> device_;
};

TEST_F(FidoBleDeviceTest, ConnectionFailureTest) {
  EXPECT_CALL(*connection(), ConnectPtr).WillOnce(Invoke([](auto* callback) {
    std::move(*callback).Run(false);
  }));

  device()->Connect();
}

TEST_F(FidoBleDeviceTest, SendPingTest_Failure_WriteFailed) {
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        task_environment_.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), false));
      }));

  TestDeviceCallbackReceiver callback_receiver;
  auto payload = fido_parsing_utils::Materialize(kTestData);
  device()->SendPing(std::move(payload), callback_receiver.callback());

  callback_receiver.WaitForCallback();
  EXPECT_FALSE(callback_receiver.value());
}

TEST_F(FidoBleDeviceTest, SendPingTest_Failure_NoResponse) {
  ConnectWithLength(kControlPointLength);
  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        task_environment_.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));
      }));

  TestDeviceCallbackReceiver callback_receiver;
  const auto payload = fido_parsing_utils::Materialize(kTestData);
  device()->SendPing(payload, callback_receiver.callback());

  callback_receiver.WaitForCallback();
  EXPECT_FALSE(callback_receiver.value().has_value());
}

TEST_F(FidoBleDeviceTest, SendPingTest_Failure_SlowResponse) {
  ConnectWithLength(kControlPointLength);
  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        task_environment_.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));
      }));

  TestDeviceCallbackReceiver callback_receiver;
  auto payload = fido_parsing_utils::Materialize(kTestData);
  device()->SendPing(payload, callback_receiver.callback());
  callback_receiver.WaitForCallback();
  EXPECT_FALSE(callback_receiver.value());

  // Imitate a ping response from the device after the timeout has passed.
  for (auto&& fragment :
       ToSerializedFragments(FidoBleDeviceCommand::kPing, std::move(payload),
                             kControlPointLength)) {
    connection()->read_callback().Run(std::move(fragment));
  }
}

TEST_F(FidoBleDeviceTest, SendPingTest) {
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(Invoke([this](const auto& data, auto* cb) {
        task_environment_.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));

        task_environment_.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(connection()->read_callback(), data));
      }));

  TestDeviceCallbackReceiver callback_receiver;
  const auto payload = fido_parsing_utils::Materialize(kTestData);
  device()->SendPing(payload, callback_receiver.callback());

  callback_receiver.WaitForCallback();
  const auto& value = callback_receiver.value();
  ASSERT_TRUE(value);
  EXPECT_EQ(payload, *value);
}

TEST_F(FidoBleDeviceTest, CancelDuringTransmission) {
  // Simulate a cancelation request that occurs while a multi-fragment message
  // is still being transmitted.
  device()->set_supported_protocol(ProtocolVersion::kCtap2);
  ConnectWithLength(kControlPointLength);

  ::testing::Sequence sequence;
  FidoDevice::CancelToken token = FidoDevice::kInvalidCancelToken;

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .InSequence(sequence)
      .WillOnce(testing::WithArg<1>(Invoke([this](auto* cb) {
        task_environment_.GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce(std::move(*cb), true));
      })));
  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .InSequence(sequence)
      .WillOnce(testing::WithArg<1>(Invoke([this, &token](auto* cb) {
        device()->Cancel(token);
        std::move(*cb).Run(true);
      })));
  EXPECT_CALL(*connection(),
              WriteControlPointPtr(
                  fido_parsing_utils::Materialize(kBleCancelCommand), _))
      .InSequence(sequence);

  TestDeviceCallbackReceiver callback_receiver;
  // The length of payload just needs to be enough to require two fragments.
  std::vector<uint8_t> payload(kControlPointLength + kControlPointLength / 2);
  token = static_cast<FidoDevice*>(device())->DeviceTransact(
      std::move(payload), callback_receiver.callback());

  callback_receiver.WaitForCallback();
}

TEST_F(FidoBleDeviceTest, CancelAfterTransmission) {
  // Simulate a cancelation request that occurs after the request has been sent.
  device()->set_supported_protocol(ProtocolVersion::kCtap2);
  ConnectWithLength(kControlPointLength);

  ::testing::Sequence sequence;
  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .InSequence(sequence)
      .WillOnce(testing::WithArg<1>(
          Invoke([](auto* cb) { std::move(*cb).Run(true); })));
  EXPECT_CALL(*connection(),
              WriteControlPointPtr(
                  fido_parsing_utils::Materialize(kBleCancelCommand), _))
      .InSequence(sequence);

  TestDeviceCallbackReceiver callback_receiver;
  // The length of payload should be small enough to require just one fragment.
  std::vector<uint8_t> payload(kControlPointLength / 2);
  const auto token = static_cast<FidoDevice*>(device())->DeviceTransact(
      std::move(payload), callback_receiver.callback());
  device()->Cancel(token);

  callback_receiver.WaitForCallback();
}

TEST_F(FidoBleDeviceTest, StaticGetIdTest) {
  std::string address = BluetoothTestBase::kTestDeviceAddress1;
  EXPECT_EQ("ble:" + address, FidoBleDevice::GetIdForAddress(address));
}

TEST_F(FidoBleDeviceTest, GetIdTest) {
  EXPECT_EQ(std::string("ble:") + BluetoothTestBase::kTestDeviceAddress1,
            device()->GetId());
}

TEST_F(FidoBleDeviceTest, IsInPairingMode) {
  // By default, a device is not in pairing mode.
  EXPECT_FALSE(device()->IsInPairingMode());

  // Initiate default connection behavior.
  EXPECT_CALL(*connection(), ConnectPtr)
      .WillOnce(Invoke([this](auto* callback) {
        connection()->FidoBleConnection::Connect(std::move(*callback));
      }));
  device()->Connect();

  // Add a mock fido device. This should also not be considered to be in pairing
  // mode.
  auto mock_bluetooth_device = std::make_unique<NiceMockBluetoothDevice>(
      adapter(), /* bluetooth_class */ 0u,
      BluetoothTestBase::kTestDeviceNameU2f,
      BluetoothTestBase::kTestDeviceAddress1, /* paired */ true,
      /* connected */ false);
  EXPECT_CALL(*adapter(), GetDevice(BluetoothTestBase::kTestDeviceAddress1))
      .WillRepeatedly(Return(mock_bluetooth_device.get()));

  EXPECT_FALSE(device()->IsInPairingMode());

  // Provide advertisement flags, but set neither the Limited nor General LE
  // Discoverable Mode bit.
  SetAdvertisingDataFlags(mock_bluetooth_device.get(), 0);
  EXPECT_FALSE(device()->IsInPairingMode());

  // Set the limited bit, expect to be in pairing mode.
  SetAdvertisingDataFlags(mock_bluetooth_device.get(),
                          1 << kLeLimitedDiscoverableModeBit);
  EXPECT_TRUE(device()->IsInPairingMode());

  // Set the general bit, expect to be in pairing mode.
  SetAdvertisingDataFlags(mock_bluetooth_device.get(),
                          1 << kLeGeneralDiscoverableModeBit);
  EXPECT_TRUE(device()->IsInPairingMode());

  // Set both bits, expect to be NOT in pairing mode.
  SetAdvertisingDataFlags(
      mock_bluetooth_device.get(),
      1 << kLeLimitedDiscoverableModeBit | 1 << kLeGeneralDiscoverableModeBit);
  EXPECT_FALSE(device()->IsInPairingMode());

  // Remove flags, should not result in pairing mode.
  SetAdvertisingDataFlags(mock_bluetooth_device.get(), base::nullopt);
  EXPECT_FALSE(device()->IsInPairingMode());

  // Update the advertised service data to include the corresponding pairing
  // mode flag. This should result in the device to be considered in pairing
  // mode.
  SetServiceData(mock_bluetooth_device.get(),
                 {{BluetoothUUID(kFidoServiceUUID),
                   {static_cast<int>(FidoServiceDataFlags::kPairingMode)}}});
  EXPECT_TRUE(device()->IsInPairingMode());

  // Clear out the service data again, device should not be considered to be in
  // pairing mode anymore.
  SetServiceData(mock_bluetooth_device.get(), {});
  EXPECT_FALSE(device()->IsInPairingMode());
}

TEST_F(FidoBleDeviceTest, DeviceMsgErrorTest) {
  // kError(BF), followed by payload length(0001), followed by INVALID_CMD(01).
  constexpr uint8_t kBleInvalidCommandError[] = {0xbf, 0x00, 0x01, 0x01};
  ConnectWithLength(kControlPointLength);

  EXPECT_CALL(*connection(), WriteControlPointPtr(_, _))
      .WillOnce(::testing::WithArg<1>(
          Invoke([this, kBleInvalidCommandError](auto* cb) {
            task_environment_.GetMainThreadTaskRunner()->PostTask(
                FROM_HERE, base::BindOnce(std::move(*cb), true));

            task_environment_.GetMainThreadTaskRunner()->PostTask(
                FROM_HERE, base::BindOnce(connection()->read_callback(),
                                          fido_parsing_utils::Materialize(
                                              kBleInvalidCommandError)));
          })));

  TestDeviceCallbackReceiver callback_receiver;
  const auto payload = fido_parsing_utils::Materialize(kTestData);
  device()->SendPing(payload, callback_receiver.callback());

  callback_receiver.WaitForCallback();
  EXPECT_EQ(FidoDevice::State::kMsgError, device()->state_for_testing());
}

TEST_F(FidoBleDeviceTest, Timeout) {
  EXPECT_CALL(*connection(), ConnectPtr);
  TestDeviceCallbackReceiver callback_receiver;
  device()->SendPing(std::vector<uint8_t>(), callback_receiver.callback());

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(FidoDevice::State::kDeviceError, device()->state_for_testing());
  EXPECT_TRUE(callback_receiver.was_called());
  EXPECT_FALSE(callback_receiver.value());
}

TEST_F(FidoBleDeviceTest, RequiresBlePairingPin) {
  EXPECT_TRUE(device()->RequiresBlePairingPin());

  EXPECT_CALL(*connection(), ConnectPtr)
      .WillOnce(Invoke([this](auto* callback) {
        connection()->FidoBleConnection::Connect(std::move(*callback));
      }));
  device()->Connect();

  // Add mock FIDO device.
  auto mock_bluetooth_device = std::make_unique<NiceMockBluetoothDevice>(
      adapter(), /* bluetooth_class */ 0u,
      BluetoothTestBase::kTestDeviceNameU2f,
      BluetoothTestBase::kTestDeviceAddress1, /* paired */ true,
      /* connected */ false);
  EXPECT_CALL(*adapter(), GetDevice(BluetoothTestBase::kTestDeviceAddress1))
      .WillRepeatedly(Return(mock_bluetooth_device.get()));

  // The default is for a device to require a PIN or passkey.
  EXPECT_TRUE(device()->RequiresBlePairingPin());

  // Clear the advertised service data to include the flag for requiring a PIN
  // or passkey during pairing.
  SetServiceData(mock_bluetooth_device.get(),
                 {{BluetoothUUID(kFidoServiceUUID), {}}});
  EXPECT_FALSE(device()->RequiresBlePairingPin());

  // Set the flag.
  SetServiceData(mock_bluetooth_device.get(),
                 {{BluetoothUUID(kFidoServiceUUID),
                   {static_cast<int>(FidoServiceDataFlags::kPasskeyEntry)}}});
  EXPECT_TRUE(device()->RequiresBlePairingPin());
}

}  // namespace device
