// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/bluetooth_serial_device_enumerator.h"

#include <memory>
#include <optional>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "services/device/public/cpp/bluetooth/bluetooth_utils.h"
#include "services/device/public/cpp/device_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using ::base::test::InvokeFuture;
using ::base::test::TestFuture;
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
  MOCK_METHOD1(OnPortConnectedStateChanged, void(const mojom::SerialPortInfo&));
};

class BluetoothSerialDeviceEnumeratorTest : public testing::Test {
 public:
  BluetoothSerialDeviceEnumeratorTest() = default;

  scoped_refptr<base::SingleThreadTaskRunner> adapter_runner() {
    return task_environment_.GetMainThreadTaskRunner();
  }

 private:
  base::test::TaskEnvironment task_environment_;
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

  // Wait for the enumerator to complete its initial enumeration.
  TestFuture<std::vector<device::mojom::SerialPortInfoPtr>> get_devices_future;
  enumerator.GetDevicesAfterInitialEnumeration(
      get_devices_future.GetCallback());
  ASSERT_TRUE(get_devices_future.Wait());

  // Prevent memory leak warning.
  enumerator.SynchronouslyResetHelperForTesting();
}

TEST_F(BluetoothSerialDeviceEnumeratorTest, GetDevices) {
  auto mock_adapter =
      base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();

  base::RunLoop run_loop;
  mock_adapter->Initialize(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(mock_adapter->IsInitialized());

  auto mock_device =
      CreateDevice(mock_adapter.get(), kTestDeviceName, kTestDeviceAddress);
  mock_device->AddUUID(GetSerialPortProfileUUID());
  mock_adapter->AddMockDevice(std::move(mock_device));
  EXPECT_CALL(*mock_adapter, GetDevices())
      .WillOnce(Return(mock_adapter->GetConstMockDevices()));

  device::BluetoothAdapterFactory::Get()->SetAdapterForTesting(mock_adapter);

  BluetoothSerialDeviceEnumerator enumerator(adapter_runner());

  // Before the initial enumeration GetDevices returns no ports.
  EXPECT_TRUE(enumerator.GetDevices().empty());

  // Check that the port is found during initial enumeration.
  TestFuture<std::vector<device::mojom::SerialPortInfoPtr>> get_devices_future;
  enumerator.GetDevicesAfterInitialEnumeration(
      get_devices_future.GetCallback());
  EXPECT_EQ(1u, get_devices_future.Get().size());

  // After the initial enumeration GetDevices returns the port.
  EXPECT_EQ(1u, enumerator.GetDevices().size());

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
  MockSerialPortEnumeratorObserver observer;
  enumerator.AddObserver(&observer);
  TestFuture<const device::mojom::SerialPortInfo&> port_future;
  EXPECT_CALL(observer, OnPortAdded).WillOnce(InvokeFuture(port_future));
  EXPECT_FALSE(port_future.Get().token.is_empty());
  EXPECT_EQ(base::FilePath::FromASCII(kTestDeviceAddress),
            port_future.Get().path);
  EXPECT_EQ(kTestDeviceName, port_future.Get().display_name);
  EXPECT_EQ(std::nullopt, port_future.Get().serial_number);
  EXPECT_EQ(mojom::SerialPortType::BLUETOOTH_CLASSIC_RFCOMM,
            port_future.Get().type);
  EXPECT_FALSE(port_future.Get().has_vendor_id);
  EXPECT_EQ(0x0, port_future.Get().vendor_id);
  EXPECT_FALSE(port_future.Get().has_product_id);
  EXPECT_EQ(0x0, port_future.Get().product_id);

  TestFuture<std::vector<device::mojom::SerialPortInfoPtr>> get_devices_future;
  enumerator.GetDevicesAfterInitialEnumeration(
      get_devices_future.GetCallback());
  ASSERT_EQ(1u, get_devices_future.Get().size());

  auto address = enumerator.GetAddressFromToken(port_future.Get().token);
  ASSERT_TRUE(address);
  EXPECT_EQ(*address, kTestDeviceAddress);
  EXPECT_EQ(GetSerialPortProfileUUID(),
            enumerator.GetServiceClassIdFromToken(port_future.Get().token));

  const base::UnguessableToken unused_token;
  EXPECT_FALSE(enumerator.GetAddressFromToken(unused_token));
  EXPECT_FALSE(enumerator.GetServiceClassIdFromToken(unused_token).IsValid());

  // Second add - which will be skipped.
  const BluetoothDevice::UUIDSet service_class_ids = {
      GetSerialPortProfileUUID()};
  const std::u16string device_name(
      base::UTF8ToUTF16(std::string(kTestDeviceName)));
  enumerator.DeviceAddedOrChanged(kTestDeviceAddress, device_name,
                                  service_class_ids, /*is_connected=*/true);
  ASSERT_EQ(1u, enumerator.GetDevices().size());

  // Remove device.
  TestFuture<const mojom::SerialPortInfo&> disconnect_future;
  EXPECT_CALL(observer, OnPortConnectedStateChanged).Times(0);
  EXPECT_CALL(observer, OnPortRemoved)
      .WillOnce(InvokeFuture(disconnect_future));
  enumerator.DeviceRemoved(kTestDeviceAddress);
  EXPECT_EQ(disconnect_future.Get().token, port_future.Get().token);
  EXPECT_FALSE(disconnect_future.Get().connected);

  // Remove again - now nonexistent.
  enumerator.DeviceRemoved(kTestDeviceAddress);

  // Prevent memory leak warning.
  enumerator.SynchronouslyResetHelperForTesting();
}

TEST_F(BluetoothSerialDeviceEnumeratorTest, PortConnectedState) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kSerialPortConnected);

  // Set a mock adapter and wait until it is initialized.
  auto mock_adapter =
      base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
  device::BluetoothAdapterFactory::Get()->SetAdapterForTesting(mock_adapter);
  base::RunLoop adapter_initialized_loop;
  mock_adapter->Initialize(adapter_initialized_loop.QuitClosure());
  adapter_initialized_loop.Run();
  EXPECT_TRUE(mock_adapter->IsInitialized());

  // Create the enumerator and wait until it has completed its initial
  // enumeration.
  EXPECT_CALL(*mock_adapter, GetDevices())
      .WillOnce(Return(mock_adapter->GetConstMockDevices()));
  BluetoothSerialDeviceEnumerator enumerator(adapter_runner());
  TestFuture<std::vector<device::mojom::SerialPortInfoPtr>> get_devices_future;
  enumerator.GetDevicesAfterInitialEnumeration(
      get_devices_future.GetCallback());
  ASSERT_TRUE(get_devices_future.Wait());

  MockSerialPortEnumeratorObserver observer;
  enumerator.AddObserver(&observer);

  // Create a Bluetooth device with a serial port.
  auto mock_device =
      CreateDevice(mock_adapter.get(), kTestDeviceName, kTestDeviceAddress);
  mock_device->AddUUID(GetSerialPortProfileUUID());

  // Add the device. The observer is notified that a new port was added.
  TestFuture<const mojom::SerialPortInfo&> port_future;
  EXPECT_CALL(observer, OnPortAdded).WillOnce(InvokeFuture(port_future));
  EXPECT_CALL(observer, OnPortConnectedStateChanged).Times(0);
  enumerator.DeviceAddedForTesting(mock_adapter.get(), mock_device.get());
  EXPECT_TRUE(port_future.Get().connected);

  // Disconnect the Bluetooth device and wait for the observer to be notified.
  TestFuture<const mojom::SerialPortInfo&> disconnect_future;
  EXPECT_CALL(observer, OnPortConnectedStateChanged)
      .WillOnce(InvokeFuture(disconnect_future));
  mock_device->SetConnected(false);
  enumerator.DeviceChangedForTesting(mock_adapter.get(), mock_device.get());
  EXPECT_EQ(disconnect_future.Get().token, port_future.Get().token);
  EXPECT_FALSE(disconnect_future.Get().connected);

  // Reconnect the Bluetooth device and wait for the observer to be notified.
  TestFuture<const mojom::SerialPortInfo&> reconnect_future;
  EXPECT_CALL(observer, OnPortConnectedStateChanged)
      .WillOnce(InvokeFuture(reconnect_future));
  mock_device->SetConnected(true);
  enumerator.DeviceChangedForTesting(mock_adapter.get(), mock_device.get());
  EXPECT_EQ(reconnect_future.Get().token, port_future.Get().token);
  EXPECT_TRUE(reconnect_future.Get().connected);

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

TEST_F(BluetoothSerialDeviceEnumeratorTest,
       PendingCallbacksInvokedOnDestruction) {
  auto mock_adapter =
      base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
  {
    base::RunLoop run_loop;
    mock_adapter->Initialize(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(mock_adapter->IsInitialized());
  }

  device::BluetoothAdapterFactory::Get()->SetAdapterForTesting(mock_adapter);
  auto enumerator =
      std::make_unique<BluetoothSerialDeviceEnumerator>(adapter_runner());

  TestFuture<std::vector<mojom::SerialPortInfoPtr>> get_devices_future;
  enumerator->GetDevicesAfterInitialEnumeration(
      get_devices_future.GetCallback());

  // Destroy the enumerator and make sure the callback is called.
  enumerator->SynchronouslyResetHelperForTesting();
  enumerator.reset();
  EXPECT_TRUE(get_devices_future.Get().empty());
}

}  // namespace device
