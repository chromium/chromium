// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/bluetooth_serial_device_enumerator.h"

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/cpp/device_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::testing::NiceMock;
using ::testing::Return;

constexpr char kTestDeviceAddress[] = "11:12:13:14:15:16";
constexpr char kTestDeviceName[] = "Device w/SPP";

std::unique_ptr<device::MockBluetoothDevice> CreateDevice(
    device::MockBluetoothAdapter* adapter,
    const char* device_name,
    const std::string& device_address) {
  return std::make_unique<NiceMock<device::MockBluetoothDevice>>(
      adapter, /*bluetooth_class=*/0u, device_name, device_address,
      /*paired=*/true, /*connected=*/true);
}

class MockSerialPortEnumeratorObserver
    : public SerialDeviceEnumerator::Observer {
 public:
  MOCK_METHOD1(OnPortAdded, void(const mojom::SerialPortInfo&));
  MOCK_METHOD1(OnPortRemoved, void(const mojom::SerialPortInfo&));
};

class BluetoothSerialDeviceEnumeratorTest : public testing::Test {
 public:
  BluetoothSerialDeviceEnumeratorTest() = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kEnableBluetoothSerialPortProfileInSerialApi}, {});
  }

  scoped_refptr<base::SingleThreadTaskRunner> adapter_runner() {
    return task_environment_.GetMainThreadTaskRunner();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

TEST_F(BluetoothSerialDeviceEnumeratorTest, ConstructDestruct) {
  auto mock_adapter =
      base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
  base::RunLoop run_loop;
  mock_adapter->Initialize(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(mock_adapter->IsInitialized());

  device::BluetoothAdapterFactory::Get()->SetAdapterForTesting(mock_adapter);

  BluetoothSerialDeviceEnumerator enumerator(adapter_runner());
  // Prevent memory leak warning.
  enumerator.SynchronouslyResetHelperForTesting();
}

TEST_F(BluetoothSerialDeviceEnumeratorTest, ConstructWaitForAdapter) {
  auto mock_adapter =
      base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();

  {
    base::RunLoop run_loop;
    mock_adapter->Initialize(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(mock_adapter->IsInitialized());
  }

  device::BluetoothAdapterFactory::Get()->SetAdapterForTesting(mock_adapter);

  BluetoothSerialDeviceEnumerator enumerator(adapter_runner());

  {
    base::RunLoop run_loop;
    enumerator.OnGotAdapterForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Prevent memory leak warning.
  enumerator.SynchronouslyResetHelperForTesting();
}

TEST_F(BluetoothSerialDeviceEnumeratorTest, CreateWithDevice) {
  auto mock_adapter =
      base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();

  {
    base::RunLoop run_loop;
    mock_adapter->Initialize(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(mock_adapter->IsInitialized());
  }

  auto mock_device =
      CreateDevice(mock_adapter.get(), kTestDeviceName, kTestDeviceAddress);
  mock_device->AddUUID(GetSerialPortProfileUUID());
  mock_adapter->AddMockDevice(std::move(mock_device));
  EXPECT_CALL(*mock_adapter, GetDevices())
      .WillOnce(Return(mock_adapter->GetConstMockDevices()));

  device::BluetoothAdapterFactory::Get()->SetAdapterForTesting(mock_adapter);

  BluetoothSerialDeviceEnumerator enumerator(adapter_runner());

  base::UnguessableToken port_token;
  MockSerialPortEnumeratorObserver observer;
  enumerator.AddObserver(&observer);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnPortAdded)
        .WillOnce([&run_loop,
                   &port_token](const mojom::SerialPortInfo& serial_port_info) {
          port_token = serial_port_info.token;
          EXPECT_FALSE(serial_port_info.token.is_empty());
          EXPECT_EQ(base::FilePath::FromASCII(kTestDeviceAddress),
                    serial_port_info.path);
          EXPECT_EQ(kTestDeviceName, serial_port_info.display_name);
          EXPECT_EQ(absl::nullopt, serial_port_info.serial_number);
          EXPECT_EQ(mojom::SerialPortType::BLUETOOTH_CLASSIC_RFCOMM,
                    serial_port_info.type);
          EXPECT_FALSE(serial_port_info.has_vendor_id);
          EXPECT_EQ(0x0, serial_port_info.vendor_id);
          EXPECT_FALSE(serial_port_info.has_product_id);
          EXPECT_EQ(0x0, serial_port_info.product_id);
          run_loop.Quit();
        });
    run_loop.Run();
  }

  ASSERT_EQ(1u, enumerator.GetDevices().size());

  auto address = enumerator.GetAddressFromToken(port_token);
  ASSERT_TRUE(address);
  EXPECT_EQ(*address, kTestDeviceAddress);
  EXPECT_EQ(GetSerialPortProfileUUID(),
            enumerator.GetServiceClassIdFromToken(port_token));

  const base::UnguessableToken unused_token;
  EXPECT_FALSE(enumerator.GetAddressFromToken(unused_token));
  EXPECT_FALSE(enumerator.GetServiceClassIdFromToken(unused_token).IsValid());

  {
    base::RunLoop run_loop;
    enumerator.OnGotAdapterForTesting(run_loop.QuitClosure());
    run_loop.Run();
  }

  // Second add - which will be skipped.
  const BluetoothDevice::UUIDSet service_class_ids = {
      GetSerialPortProfileUUID()};
  const std::u16string device_name(
      base::UTF8ToUTF16(std::string(kTestDeviceName)));
  enumerator.DeviceAdded(kTestDeviceAddress, device_name, service_class_ids);
  ASSERT_EQ(1u, enumerator.GetDevices().size());

  // Remove device.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, OnPortRemoved)
        .WillOnce([&run_loop,
                   &port_token](const mojom::SerialPortInfo& serial_port_info) {
          EXPECT_EQ(port_token, serial_port_info.token);
          run_loop.Quit();
        });
    enumerator.DeviceRemoved(kTestDeviceAddress);
    run_loop.Run();
  }

  // Remove again - now nonexistent.
  enumerator.DeviceRemoved(kTestDeviceAddress);

  // Prevent memory leak warning.
  enumerator.SynchronouslyResetHelperForTesting();
}

TEST_F(BluetoothSerialDeviceEnumeratorTest,
       RemoveObserverIsCalledWhenAdapterHelperDestruct) {
  auto mock_adapter =
      base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
  {
    base::RunLoop run_loop;
    mock_adapter->Initialize(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(mock_adapter->IsInitialized());
  }

  device::BluetoothAdapterFactory::Get()->SetAdapterForTesting(mock_adapter);
  std::unique_ptr<BluetoothSerialDeviceEnumerator> enumerator;
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_adapter, AddObserver)
        .WillOnce([&run_loop](BluetoothAdapter::Observer* observer) {
          run_loop.Quit();
        });
    enumerator =
        std::make_unique<BluetoothSerialDeviceEnumerator>(adapter_runner());
    run_loop.Run();
  }
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_adapter, RemoveObserver)
        .WillOnce([&run_loop](BluetoothAdapter::Observer* observer) {
          run_loop.Quit();
        });
    enumerator->SynchronouslyResetHelperForTesting();
    run_loop.Run();
  }
}

}  // namespace device
