// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/browser/web_test_bluetooth_adapter_provider.h"

#include <set>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/threading/thread.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/public/cpp/bluetooth_address.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_descriptor.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using ::base::test::RunOnceCallback;
using ::device::BluetoothAdapter;
using ::device::BluetoothDevice;
using ::device::BluetoothGattCharacteristic;
using ::device::BluetoothGattService;
using ::device::BluetoothRemoteGattCharacteristic;
using ::device::BluetoothRemoteGattDescriptor;
using ::device::BluetoothRemoteGattService;
using ::device::BluetoothUUID;
using ::device::MockBluetoothAdapter;
using ::device::MockBluetoothDevice;
using ::device::MockBluetoothGattCharacteristic;
using ::device::MockBluetoothGattConnection;
using ::device::MockBluetoothGattDescriptor;
using ::device::MockBluetoothGattNotifySession;
using ::device::MockBluetoothGattService;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::ResultOf;
using ::testing::Return;
using ::testing::WithArg;
using NiceMockBluetoothAdapter = ::testing::NiceMock<MockBluetoothAdapter>;
using NiceMockBluetoothDevice = ::testing::NiceMock<MockBluetoothDevice>;
using NiceMockBluetoothGattDescriptor =
    ::testing::NiceMock<MockBluetoothGattDescriptor>;
using NiceMockBluetoothGattCharacteristic =
    ::testing::NiceMock<MockBluetoothGattCharacteristic>;
using NiceMockBluetoothGattConnection =
    ::testing::NiceMock<MockBluetoothGattConnection>;
using NiceMockBluetoothGattService =
    ::testing::NiceMock<MockBluetoothGattService>;
using NiceMockBluetoothGattNotifySession =
    ::testing::NiceMock<MockBluetoothGattNotifySession>;

// Bluetooth UUIDs suitable to pass to BluetoothUUID():
// Services:
const char kBatteryServiceUUID[] = "180f";
const char kBlocklistTestServiceUUID[] = "611c954a-263b-4f4a-aab6-01ddb953f985";
const char kDeviceInformationServiceUUID[] = "180a";
const char kGenericAccessServiceUUID[] = "1800";
const char kGlucoseServiceUUID[] = "1808";
const char kHealthThermometerUUID[] = "1809";
const char kHeartRateServiceUUID[] = "180d";
const char kHumanInterfaceDeviceServiceUUID[] = "1812";
const char kRequestDisconnectionServiceUUID[] =
    "01d7d889-7451-419f-aeb8-d65e7b9277af";
const char kTxPowerServiceUUID[] = "1804";
// Characteristics:
const char kBlocklistExcludeReadsCharacteristicUUID[] =
    "bad1c9a2-9a5b-4015-8b60-1579bbbf2135";
const char kRequestDisconnectionCharacteristicUUID[] =
    "01d7d88a-7451-419f-aeb8-d65e7b9277af";
const char kBodySensorLocation[] = "2a38";
const char kDeviceNameUUID[] = "2a00";
const char kMeasurementIntervalUUID[] = "2a21";
const char kHeartRateMeasurementUUID[] = "2a37";
const char kSerialNumberStringUUID[] = "2a25";
const char kPeripheralPrivacyFlagUUID[] = "2a02";
// Descriptors:
const char kUserDescriptionUUID[] = "2901";
// Client Config is in our blocklist.  It must not be writable
const char kClientConfigUUID[] = "2902";
// Blocklisted descriptor
const char kBlocklistedDescriptorUUID[] =
    "bad2ddcf-60db-45cd-bef9-fd72b153cf7c";
const char kBlocklistedReadDescriptorUUID[] =
    "bad3ec61-3cc3-4954-9702-7977df514114";
const char kCharacteristicUserDescription[] =
    "gatt.characteristic_user_description";

// Invokes Run() on the k-th argument of the function with no arguments.
ACTION_TEMPLATE(RunCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_0_VALUE_PARAMS()) {
  return std::move(std::get<k>(args)).Run();
}

// Invokes Run() on the k-th argument of the function with 1 argument.
ACTION_TEMPLATE(RunCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p0)) {
  return std::move(std::get<k>(args)).Run(p0);
}

// Invokes Run() on the k-th argument of the function with the result
// of |func| as an argument.
ACTION_TEMPLATE(RunCallbackWithResult,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(func)) {
  return std::move(std::get<k>(args)).Run(func());
}

// Invokes Run() on the k-th argument of the function with the arguments p0
// and p1
ACTION_TEMPLATE(RunCallbackWithResult,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p0, p1)) {
  return std::move(std::get<k>(args)).Run(p0, p1);
}

// Invokes Run() on the k-th argument of the function with the arguments p0
// and the result from func
ACTION_TEMPLATE(RunCallbackWithResultFunction,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(p0, func)) {
  return std::move(std::get<k>(args)).Run(p0, func());
}

// Invokes Run() on the k-th argument of the function with the
// result from func and argument p1
ACTION_TEMPLATE(RunCallbackWithFunctionResult,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_2_VALUE_PARAMS(func, p1)) {
  return std::move(std::get<k>(args)).Run(func(), p1);
}

// Function to iterate over the adapter's devices and return the one
// that matches the address.
ACTION_P(GetMockDevice, adapter) {
  std::string address = arg0;
  for (BluetoothDevice* device : adapter->GetMockDevices()) {
    if (device->GetAddress() == address)
      return device;
  }
  return nullptr;
}

std::set<BluetoothUUID> GetUUIDs(
    const device::BluetoothDiscoveryFilter* filter) {
  std::set<BluetoothUUID> result;
  filter->GetUUIDs(result);
  return result;
}

// Notifies the adapter's observers for each device id the adapter.
void NotifyDevicesAdded(MockBluetoothAdapter* adapter) {
  for (BluetoothDevice* device : adapter->GetMockDevices()) {
    for (auto& observer : adapter->GetObservers())
      observer.DeviceAdded(adapter, device);
  }
}

// Notifies the adapter's observers that the services have been discovered.
void NotifyServicesDiscovered(MockBluetoothAdapter* adapter,
                              MockBluetoothDevice* device) {
  for (auto& observer : adapter->GetObservers())
    observer.GattServicesDiscovered(adapter, device);
}

// Notifies the adapter's observers that a device has changed.
void NotifyDeviceChanged(MockBluetoothAdapter* adapter,
                         MockBluetoothDevice* device) {
  for (auto& observer : adapter->GetObservers())
    observer.DeviceChanged(adapter, device);
}

}  // namespace

namespace content {

// static
scoped_refptr<BluetoothAdapter>
WebTestBluetoothAdapterProvider::GetBluetoothAdapter(
    const std::string& fake_adapter_name) {
  // When modifying the set of supported test adapters this information must be
  // kept in sync with
  // third_party/blink/renderer/modules/bluetooth/testing/clusterfuzz/wbt_fakes.py
  // so that invalid test cases are not generated.
  if (fake_adapter_name == "BaseAdapter")
    return GetBaseAdapter();
  if (fake_adapter_name == "ScanFilterCheckingAdapter")
    return GetScanFilterCheckingAdapter();
  if (fake_adapter_name == "EmptyAdapter")
    return GetEmptyAdapter();
  if (fake_adapter_name == "FailStartDiscoveryAdapter")
    return GetFailStartDiscoveryAdapter();
  if (fake_adapter_name == "GlucoseHeartRateAdapter")
    return GetGlucoseHeartRateAdapter();
  if (fake_adapter_name == "MissingServiceHeartRateAdapter")
    return GetMissingServiceHeartRateAdapter();
  if (fake_adapter_name == "MissingCharacteristicHeartRateAdapter")
    return GetMissingCharacteristicHeartRateAdapter();
  if (fake_adapter_name == "HeartRateAdapter")
    return GetHeartRateAdapter();
  if (fake_adapter_name == "NoNameDeviceAdapter")
    return GetNoNameDeviceAdapter();
  if (fake_adapter_name == "EmptyNameHeartRateAdapter")
    return GetEmptyNameHeartRateAdapter();
  if (fake_adapter_name == "NoNameHeartRateAdapter")
    return GetNoNameHeartRateAdapter();
  if (fake_adapter_name == "TwoHeartRateServicesAdapter")
    return GetTwoHeartRateServicesAdapter();
  if (fake_adapter_name == "DisconnectingHeartRateAdapter")
    return GetDisconnectingHeartRateAdapter();
  if (fake_adapter_name == "DisconnectingHealthThermometerAdapter")
    return GetDisconnectingHealthThermometer(true);
  if (fake_adapter_name ==
      "MissingDescriptorsDisconnectingHealthThermometerAdapter")
    return GetDisconnectingHealthThermometer(false);
  if (fake_adapter_name == "DisconnectingDuringServiceRetrievalAdapter")
    return GetServicesDiscoveredAfterReconnectionAdapter(true /* disconnect */);
  if (fake_adapter_name == "ServicesDiscoveredAfterReconnectionAdapter")
    return GetServicesDiscoveredAfterReconnectionAdapter(
        false /* disconnect */);
  if (fake_adapter_name == "DisconnectingDuringSuccessGATTOperationAdapter") {
    return GetGATTOperationFinishesAfterReconnectionAdapter(
        true /* disconnect */, true /* succeeds */);
  }
  if (fake_adapter_name == "DisconnectingDuringFailureGATTOperationAdapter") {
    return GetGATTOperationFinishesAfterReconnectionAdapter(
        true /* disconnect */, false /* succeeds */);
  }
  if (fake_adapter_name == "GATTOperationSucceedsAfterReconnectionAdapter") {
    return GetGATTOperationFinishesAfterReconnectionAdapter(
        false /* disconnect */, true /* succeeds */);
  }
  if (fake_adapter_name == "GATTOperationFailsAfterReconnectionAdapter") {
    return GetGATTOperationFinishesAfterReconnectionAdapter(
        false /* disconnect */, false /* succeeds */);
  }
  if (fake_adapter_name == "DisconnectingDuringStopNotifySessionAdapter") {
    return GetStopNotifySessionFinishesAfterReconnectionAdapter(
        true /* disconnect */);
  }
  if (fake_adapter_name ==
      "StopNotifySessionFinishesAfterReconnectionAdapter") {
    return GetStopNotifySessionFinishesAfterReconnectionAdapter(
        false /* disconnect */);
  }
  if (fake_adapter_name == "BlocklistTestAdapter")
    return GetBlocklistTestAdapter();
  if (fake_adapter_name == "FailingConnectionsAdapter")
    return GetFailingConnectionsAdapter();
  if (fake_adapter_name == "FailingGATTOperationsAdapter")
    return GetFailingGATTOperationsAdapter();
  if (fake_adapter_name == "SecondDiscoveryFindsHeartRateAdapter")
    return GetSecondDiscoveryFindsHeartRateAdapter();
  if (fake_adapter_name == "DeviceEventAdapter")
    return GetDeviceEventAdapter();
  if (fake_adapter_name == "DevicesRemovedAdapter")
    return GetDevicesRemovedAdapter();
  if (fake_adapter_name == "DelayedServicesDiscoveryAdapter")
    return GetDelayedServicesDiscoveryAdapter();
  if (fake_adapter_name.empty())
    return nullptr;

  LOG(ERROR) << "Test requested unrecognized adapter: " << fake_adapter_name;
  return nullptr;
}

// Adapters

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetBaseAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(
      new NiceMockBluetoothAdapter());

  // Using Invoke allows the adapter returned from this method to be futher
  // modified and have devices added to it. The call to ::GetDevices will
  // invoke ::GetConstMockDevices, returning all devices added up to that time.
  ON_CALL(*adapter, GetDevices())
      .WillByDefault(
          Invoke(adapter.get(), &MockBluetoothAdapter::GetConstMockDevices));

  // The call to ::GetDevice will invoke GetMockDevice which returns a device
  // matching the address provided if the device was added to the mock.
  ON_CALL(*adapter, GetDevice(_)).WillByDefault(GetMockDevice(adapter.get()));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetPresentAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetBaseAdapter());
  ON_CALL(*adapter, IsPresent()).WillByDefault(Return(true));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetPoweredAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPresentAdapter());
  ON_CALL(*adapter, IsPowered()).WillByDefault(Return(true));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetScanFilterCheckingAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());
  MockBluetoothAdapter* adapter_ptr = adapter.get();

  // This fails the test with an error message listing actual and expected UUIDs
  // if StartDiscoverySessionWithFilter() is called with the wrong argument.
  EXPECT_CALL(
      *adapter,
      StartScanWithFilter_(
          ResultOf(&GetUUIDs, ElementsAre(BluetoothUUID(kGlucoseServiceUUID),
                                          BluetoothUUID(kHeartRateServiceUUID),
                                          BluetoothUUID(kBatteryServiceUUID))),
          _))
      .WillRepeatedly(
          Invoke([adapter_ptr](
                     const device::BluetoothDiscoveryFilter* discovery_filter,
                     device::BluetoothAdapter::DiscoverySessionResultCallback&
                         callback) {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(&NotifyDevicesAdded,
                                          base::RetainedRef(adapter_ptr)));

            std::move(callback).Run(
                /*is_error=*/false,
                device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
          }));

  // Any unexpected call results in the failure callback.
  ON_CALL(*adapter, StartScanWithFilter_(_, _))
      .WillByDefault(RunCallbackWithResult<1 /* result_callback */>(
          /*is_error=*/true,
          device::UMABluetoothDiscoverySessionOutcome::UNKNOWN));

  // We need to add a device otherwise requestDevice would reject.
  adapter->AddMockDevice(GetBatteryDevice(adapter.get()));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetFailStartDiscoveryAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());

  ON_CALL(*adapter, StartScanWithFilter_(_, _))
      .WillByDefault(RunCallbackWithResult<1 /* result_callback */>(
          /*is_error=*/true,
          device::UMABluetoothDiscoverySessionOutcome::UNKNOWN));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetEmptyAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());

  MockBluetoothAdapter* adapter_ptr = adapter.get();

  ON_CALL(*adapter, StartScanWithFilter_(_, _))
      .WillByDefault(RunCallbackWithResultFunction<1 /* result_callback */>(
          /*is_error=*/false, [adapter_ptr]() {
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(&NotifyDevicesAdded,
                                          base::RetainedRef(adapter_ptr)));

            return device::UMABluetoothDiscoverySessionOutcome::SUCCESS;
          }));

  ON_CALL(*adapter, StopScan(_))
      .WillByDefault(
          Invoke([](device::BluetoothAdapter::DiscoverySessionResultCallback
                        callback) {
            std::move(callback).Run(
                /*is_error=*/false,
                device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
          }));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetGlucoseHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetHeartRateDevice(adapter.get()));
  adapter->AddMockDevice(GetGlucoseDevice(adapter.get()));

  return adapter;
}

// Adds a device to |adapter| and notifies all observers about that new device.
// Mocks can call this asynchronously to cause changes in the middle of a test.
static void AddDevice(scoped_refptr<NiceMockBluetoothAdapter> adapter,
                      std::unique_ptr<NiceMockBluetoothDevice> new_device) {
  NiceMockBluetoothDevice* new_device_ptr = new_device.get();
  adapter->AddMockDevice(std::move(new_device));
  for (auto& observer : adapter->GetObservers())
    observer.DeviceAdded(adapter.get(), new_device_ptr);
}

static void RemoveDevice(scoped_refptr<NiceMockBluetoothAdapter> adapter,
                         const std::string& device_address) {
  std::unique_ptr<MockBluetoothDevice> removed_device =
      adapter->RemoveMockDevice(device_address);
  for (auto& observer : adapter->GetObservers())
    observer.DeviceRemoved(adapter.get(), removed_device.get());
}
// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetSecondDiscoveryFindsHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  EXPECT_CALL(*adapter, StartScanWithFilter_(_, _))
      .WillOnce(RunCallbackWithResult<1 /* result_callback */>(
          /*is_error=*/false,
          device::UMABluetoothDiscoverySessionOutcome::SUCCESS))
      .WillOnce(RunCallbackWithResultFunction<1 /* result_callback */>(
          /*is_error=*/false, [adapter_ptr]() {
            // In the second discovery session, have the adapter discover a new
            // device, shortly after the session starts.
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(&AddDevice, base::WrapRefCounted(adapter_ptr),
                               GetHeartRateDevice(adapter_ptr)));
            return device::UMABluetoothDiscoverySessionOutcome::SUCCESS;
          }));

  EXPECT_CALL(*adapter, StopScan(_)).Times(2);

  ON_CALL(*adapter, StopScan(_))
      .WillByDefault(
          Invoke([](device::BluetoothAdapter::DiscoverySessionResultCallback
                        callback) {
            std::move(callback).Run(
                /*is_error=*/false,
                device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
          }));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetDeviceEventAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  // Add ConnectedHeartRateDevice.
  auto connected_hr(GetBaseDevice(adapter.get(), "Connected Heart Rate Device",
                                  {BluetoothUUID(kHeartRateServiceUUID)},
                                  makeMACAddress(0x0)));
  connected_hr->SetConnected(true);
  adapter->AddMockDevice(std::move(connected_hr));

  // Add ChangingBatteryDevice with no uuids.
  auto changing_battery(GetBaseDevice(adapter.get(), "Changing Battery Device",
                                      BluetoothDevice::UUIDList(),
                                      makeMACAddress(0x1)));
  changing_battery->SetConnected(false);

  NiceMockBluetoothDevice* changing_battery_ptr = changing_battery.get();
  adapter->AddMockDevice(std::move(changing_battery));

  // Add Non Connected Tx Power Device.
  auto non_connected_tx_power(
      GetBaseDevice(adapter.get(), "Non Connected Tx Power Device",
                    {BluetoothUUID(kTxPowerServiceUUID)}, makeMACAddress(0x2)));
  non_connected_tx_power->SetConnected(false);
  adapter->AddMockDevice(std::move(non_connected_tx_power));

  // Add Discovery Generic Access Device with no uuids.
  auto discovery_generic_access(
      GetBaseDevice(adapter.get(), "Discovery Generic Access Device",
                    BluetoothDevice::UUIDList(), makeMACAddress(0x3)));
  discovery_generic_access->SetConnected(true);

  NiceMockBluetoothDevice* discovery_generic_access_ptr =
      discovery_generic_access.get();
  adapter->AddMockDevice(std::move(discovery_generic_access));

  ON_CALL(*adapter, StartScanWithFilter_(_, _))
      .WillByDefault(RunCallbackWithResultFunction<1 /* result_callback */>(
          /*is_error=*/false,
          [adapter_ptr, changing_battery_ptr, discovery_generic_access_ptr]() {
            if (adapter_ptr->GetDevices().size() == 4) {
              // Post task to add NewGlucoseDevice.
              auto glucose_device(GetBaseDevice(
                  adapter_ptr, "New Glucose Device",
                  {BluetoothUUID(kGlucoseServiceUUID)}, makeMACAddress(0x4)));

              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&AddDevice, base::WrapRefCounted(adapter_ptr),
                                 std::move(glucose_device)));

              // Add uuid and notify of device changed.
              changing_battery_ptr->AddUUID(BluetoothUUID(kBatteryServiceUUID));
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(&NotifyDeviceChanged,
                                            base::RetainedRef(adapter_ptr),
                                            changing_battery_ptr));

              // Add uuid and notify of services discovered.
              discovery_generic_access_ptr->AddUUID(
                  BluetoothUUID(kGenericAccessServiceUUID));
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(&NotifyServicesDiscovered,
                                            base::RetainedRef(adapter_ptr),
                                            discovery_generic_access_ptr));
            }
            return device::UMABluetoothDiscoverySessionOutcome::SUCCESS;
          }));

  ON_CALL(*adapter, StopScan(_))
      .WillByDefault(
          Invoke([](device::BluetoothAdapter::DiscoverySessionResultCallback
                        callback) {
            std::move(callback).Run(
                /*is_error=*/false,
                device::UMABluetoothDiscoverySessionOutcome::SUCCESS);
          }));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetDevicesRemovedAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetPoweredAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  // Add ConnectedHeartRateDevice.
  auto connected_hr(GetBaseDevice(adapter.get(), "Connected Heart Rate Device",
                                  {BluetoothUUID(kHeartRateServiceUUID)},
                                  makeMACAddress(0x0)));
  connected_hr->SetConnected(true);
  std::string connected_hr_address = connected_hr->GetAddress();
  adapter->AddMockDevice(std::move(connected_hr));

  ON_CALL(*adapter, StartScanWithFilter_(_, _))
      .WillByDefault(RunCallbackWithResultFunction<1 /* result_callback */>(
          /*is_error=*/false, [adapter_ptr, connected_hr_address]() {
            if (adapter_ptr->GetDevices().size() == 1) {
              // Post task to add NewGlucoseDevice.
              auto glucose_device(GetBaseDevice(
                  adapter_ptr, "New Glucose Device",
                  {BluetoothUUID(kGlucoseServiceUUID)}, makeMACAddress(0x4)));

              std::string glucose_address = glucose_device->GetAddress();

              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&AddDevice, base::WrapRefCounted(adapter_ptr),
                                 std::move(glucose_device)));

              // Post task to remove ConnectedHeartRateDevice.
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(&RemoveDevice,
                                            base::WrapRefCounted(adapter_ptr),
                                            connected_hr_address));

              // Post task to remove NewGlucoseDevice.
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE, base::BindOnce(&RemoveDevice,
                                            base::WrapRefCounted(adapter_ptr),
                                            glucose_address));
            }
            return device::UMABluetoothDiscoverySessionOutcome::SUCCESS;
          }));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetMissingServiceHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  adapter->AddMockDevice(GetHeartRateDevice(adapter.get()));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetMissingCharacteristicHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  auto device(GetHeartRateDevice(adapter.get()));

  auto generic_access(GetBaseGATTService("Generic Access", device.get(),
                                         kGenericAccessServiceUUID));
  auto heart_rate(
      GetBaseGATTService("Heart Rate", device.get(), kHeartRateServiceUUID));

  // Intentionally NOT adding a characteristic to heart_rate service.

  device->AddMockService(std::move(generic_access));
  device->AddMockService(std::move(heart_rate));
  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetDelayedServicesDiscoveryAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  auto device(GetHeartRateDevice(adapter.get()));

  MockBluetoothAdapter* adapter_ptr = adapter.get();
  MockBluetoothDevice* device_ptr = device.get();

  // Override the previous mock implementation of
  // IsGattServicesDiscoveryComplete so that the first time the function is
  // called it returns false, adds a service and posts a task to notify
  // the services have been discovered. Subsequent calls to the function
  // will return true.
  ON_CALL(*device, IsGattServicesDiscoveryComplete())
      .WillByDefault(Invoke([adapter_ptr, device_ptr] {
        std::vector<BluetoothRemoteGattService*> services =
            device_ptr->GetMockServices();

        if (services.size() == 0) {
          auto heart_rate(GetBaseGATTService("Heart Rate", device_ptr,
                                             kHeartRateServiceUUID));

          device_ptr->AddMockService(std::move(heart_rate));
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(&NotifyServicesDiscovered,
                             base::RetainedRef(adapter_ptr), device_ptr));

          DCHECK(services.empty());
          return false;
        }

        return true;
      }));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  auto device(GetHeartRateDevice(adapter.get()));

  // TODO(ortuno): Implement the rest of the service's characteristics
  // See: http://crbug.com/529975

  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));
  adapter->AddMockDevice(std::move(device));
  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetDisconnectingHealthThermometer(
    bool add_descriptors) {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  auto device(GetConnectableDevice(
      adapter_ptr, "Disconnecting Health Thermometer",
      std::vector<BluetoothUUID>({BluetoothUUID(kGenericAccessServiceUUID),
                                  BluetoothUUID(kHealthThermometerUUID)})));

  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetDisconnectingService(adapter.get(), device.get()));

  auto health_thermometer(GetBaseGATTService("Health Thermometer", device.get(),
                                             kHealthThermometerUUID));

  // Measurement Interval
  auto measurement_interval(GetBaseGATTCharacteristic(
      "Measurement Interval", health_thermometer.get(),
      kMeasurementIntervalUUID,
      BluetoothRemoteGattCharacteristic::PROPERTY_READ |
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITE |
          BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));
  NiceMockBluetoothGattCharacteristic* measurement_ptr =
      measurement_interval.get();

  ON_CALL(*measurement_interval, ReadRemoteCharacteristic_(_))
      .WillByDefault(RunCallbackWithResult<0>(/*error_code=*/std::nullopt,
                                              std::vector<uint8_t>({1})));

  ON_CALL(*measurement_interval, WriteRemoteCharacteristic_(_, _, _, _))
      .WillByDefault(RunCallback<2 /* success_callback */>());

  ON_CALL(*measurement_interval, DeprecatedWriteRemoteCharacteristic_(_, _, _))
      .WillByDefault(RunCallback<1 /* success_callback */>());

  ON_CALL(*measurement_interval, StartNotifySession_(_, _))
      .WillByDefault(
          RunCallbackWithResult<0 /* success_callback */>([measurement_ptr]() {
            return GetBaseGATTNotifySession(measurement_ptr->GetWeakPtr());
          }));

  if (add_descriptors) {
    const std::string descriptorName = kCharacteristicUserDescription;
    auto user_description = std::make_unique<NiceMockBluetoothGattDescriptor>(
        measurement_interval.get(), descriptorName,
        BluetoothUUID(kUserDescriptionUUID),
        device::BluetoothRemoteGattCharacteristic::PROPERTY_READ);

    ON_CALL(*user_description, ReadRemoteDescriptor_(_))
        .WillByDefault(
            Invoke([descriptorName](
                       BluetoothRemoteGattDescriptor::ValueCallback& callback) {
              std::vector<uint8_t> value(descriptorName.begin(),
                                         descriptorName.end());
              std::move(callback).Run(/*error_code=*/std::nullopt, value);
            }));

    ON_CALL(*user_description, WriteRemoteDescriptor_(_, _, _))
        .WillByDefault(RunCallback<1 /* success_callback */>());

    auto client_config = std::make_unique<NiceMockBluetoothGattDescriptor>(
        measurement_interval.get(), "gatt.client_characteristic_configuration",
        BluetoothUUID(kClientConfigUUID),
        device::BluetoothRemoteGattCharacteristic::PROPERTY_READ |
            device::BluetoothRemoteGattCharacteristic::PROPERTY_WRITE);

    // Crash if WriteRemoteDescriptor called. Not using GoogleMock's Expect
    // because this is used in web tests that may not report a mock
    // expectation.
    ON_CALL(*client_config, WriteRemoteDescriptor_(_, _, _))
        .WillByDefault(
            Invoke([](const std::vector<uint8_t>&, base::OnceClosure&,
                      BluetoothRemoteGattDescriptor::ErrorCallback&) {
              NOTREACHED_IN_MIGRATION();
            }));

    auto no_read_descriptor = std::make_unique<NiceMockBluetoothGattDescriptor>(
        measurement_interval.get(), kBlocklistedReadDescriptorUUID,
        BluetoothUUID(kBlocklistedReadDescriptorUUID),
        device::BluetoothRemoteGattCharacteristic::PROPERTY_READ |
            device::BluetoothRemoteGattCharacteristic::PROPERTY_WRITE);

    // Crash if ReadRemoteDescriptor called. Not using GoogleMock's Expect
    // because this is used in web tests that may not report a mock
    // expectation
    // error correctly as a web test failure.
    ON_CALL(*no_read_descriptor, ReadRemoteDescriptor_(_))
        .WillByDefault(
            Invoke([](BluetoothRemoteGattDescriptor::ValueCallback&) {
              NOTREACHED_IN_MIGRATION();
            }));

    // Add it here with full permission as the blocklist should prevent us from
    // accessing this descriptor
    auto blocklisted_descriptor =
        std::make_unique<NiceMockBluetoothGattDescriptor>(
            measurement_interval.get(), kBlocklistedDescriptorUUID,
            BluetoothUUID(kBlocklistedDescriptorUUID),
            device::BluetoothRemoteGattCharacteristic::PROPERTY_READ |
                device::BluetoothRemoteGattCharacteristic::PROPERTY_WRITE);

    measurement_interval->AddMockDescriptor(std::move(user_description));
    measurement_interval->AddMockDescriptor(std::move(client_config));
    measurement_interval->AddMockDescriptor(std::move(blocklisted_descriptor));
    measurement_interval->AddMockDescriptor(std::move(no_read_descriptor));
  }
  health_thermometer->AddMockCharacteristic(std::move(measurement_interval));
  device->AddMockService(std::move(health_thermometer));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetNoNameDeviceAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  auto device(GetConnectableDevice(adapter.get(), nullptr /* device_name */));

  adapter->AddMockDevice(std::move(device));
  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetEmptyNameHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  auto device(GetHeartRateDevice(adapter.get(), /* device_name */ ""));

  // TODO(ortuno): Implement the rest of the service's characteristics
  // See: http://crbug.com/529975

  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetNoNameHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  auto device(GetHeartRateDevice(adapter.get(), /* device_name */ nullptr));

  // TODO(ortuno): Implement the rest of the service's characteristics
  // See: http://crbug.com/529975

  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetTwoHeartRateServicesAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  auto device(GetHeartRateDevice(adapter.get()));

  device->AddMockService(GetGenericAccessService(device.get()));

  // First Heart Rate Service has one Heart Rate Measurement characteristic
  // and one Body Sensor Location characteristic.
  auto first_heart_rate(GetBaseGATTService("First Heart Rate", device.get(),
                                           kHeartRateServiceUUID));

  // Heart Rate Measurement
  auto heart_rate_measurement(GetBaseGATTCharacteristic(
      "Heart Rate Measurement", first_heart_rate.get(),
      kHeartRateMeasurementUUID,
      BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));

  // Body Sensor Location Characteristic
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      body_sensor_location_chest(GetBaseGATTCharacteristic(
          "Body Sensor Location Chest", first_heart_rate.get(),
          kBodySensorLocation,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  first_heart_rate->AddMockCharacteristic(std::move(heart_rate_measurement));
  first_heart_rate->AddMockCharacteristic(
      std::move(body_sensor_location_chest));
  device->AddMockService(std::move(first_heart_rate));

  // Second Heart Rate Service has only one Body Sensor Location
  // characteristic.
  auto second_heart_rate(GetBaseGATTService("Second Heart Rate", device.get(),
                                            kHeartRateServiceUUID));
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      body_sensor_location_wrist(GetBaseGATTCharacteristic(
          "Body Sensor Location Wrist", second_heart_rate.get(),
          kBodySensorLocation,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  second_heart_rate->AddMockCharacteristic(
      std::move(body_sensor_location_wrist));
  device->AddMockService(std::move(second_heart_rate));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetDisconnectingHeartRateAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  auto device(GetHeartRateDevice(adapter.get()));

  // TODO(ortuno): Implement the rest of the service's characteristics
  // See: http://crbug.com/529975

  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));
  device->AddMockService(GetDisconnectingService(adapter.get(), device.get()));

  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetServicesDiscoveredAfterReconnectionAdapter(
    bool disconnect) {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();
  auto device(GetHeartRateDevice(adapter.get()));
  NiceMockBluetoothDevice* device_ptr = device.get();

  // When called before IsGattDiscoveryComplete, run success callback with a new
  // Gatt connection. When called after after IsGattDiscoveryComplete runs
  // success callback with a new Gatt connection and notifies of services
  // discovered.
  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(
          [adapter_ptr, device_ptr](
              BluetoothDevice::GattConnectionCallback callback,
              std::optional<BluetoothUUID> service_uuid) {
            std::vector<BluetoothRemoteGattService*> services =
                device_ptr->GetMockServices();

            if (services.size() != 0) {
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&NotifyServicesDiscovered,
                                 base::RetainedRef(adapter_ptr), device_ptr));
            }

            device_ptr->SetConnected(true);
            std::move(callback).Run(
                std::make_unique<NiceMockBluetoothGattConnection>(
                    adapter_ptr, device_ptr->GetAddress()),
                /*error_code=*/std::nullopt);
          });

  // The first time this function is called we:
  // 1. Add a service (This indicates that this function has been called)
  // 2. If |disconnect| is true, disconnect the device.
  // 3. Return false.
  // The second time this function is called we just return true.
  ON_CALL(*device, IsGattServicesDiscoveryComplete())
      .WillByDefault(Invoke([adapter_ptr, device_ptr, disconnect] {
        std::vector<BluetoothRemoteGattService*> services =
            device_ptr->GetMockServices();
        if (services.size() == 0) {
          auto heart_rate(GetBaseGATTService("Heart Rate", device_ptr,
                                             kHeartRateServiceUUID));

          device_ptr->AddMockService(GetGenericAccessService(device_ptr));
          device_ptr->AddMockService(
              GetHeartRateService(adapter_ptr, device_ptr));

          if (disconnect) {
            device_ptr->SetConnected(false);
            base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(&NotifyDeviceChanged,
                               base::RetainedRef(adapter_ptr), device_ptr));
          }
          DCHECK(services.empty());
          return false;
        }

        return true;
      }));
  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter> WebTestBluetoothAdapterProvider::
    GetGATTOperationFinishesAfterReconnectionAdapter(bool disconnect,
                                                     bool succeeds) {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  auto device(GetConnectableDevice(
      adapter_ptr, "GATT Operation finishes after reconnection Device",
      BluetoothDevice::UUIDList({BluetoothUUID(kGenericAccessServiceUUID),
                                 BluetoothUUID(kHealthThermometerUUID)})));
  NiceMockBluetoothDevice* device_ptr = device.get();

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault([adapter_ptr, device_ptr](
                         BluetoothDevice::GattConnectionCallback callback,
                         std::optional<BluetoothUUID> service_uuid) {
        device_ptr->SetConnected(true);
        std::move(callback).Run(
            std::make_unique<NiceMockBluetoothGattConnection>(
                adapter_ptr, device_ptr->GetAddress()),
            /*error_code=*/std::nullopt);
        device_ptr->RunPendingCallbacks();
      });

  device->AddMockService(GetGenericAccessService(device.get()));

  auto health_thermometer(GetBaseGATTService("Health Thermometer", device.get(),
                                             kHealthThermometerUUID));

  // Measurement Interval
  auto measurement_interval(GetBaseGATTCharacteristic(
      "Measurement Interval", health_thermometer.get(),
      kMeasurementIntervalUUID,
      BluetoothRemoteGattCharacteristic::PROPERTY_READ |
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITE |
          BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));
  NiceMockBluetoothGattCharacteristic* measurement_ptr =
      measurement_interval.get();

  ON_CALL(*measurement_interval, ReadRemoteCharacteristic_(_))
      .WillByDefault(Invoke(
          [adapter_ptr, device_ptr, disconnect, succeeds](
              BluetoothRemoteGattCharacteristic::ValueCallback& callback) {
            base::OnceClosure pending;
            if (succeeds) {
              pending = base::BindOnce(std::move(callback),
                                       /*error_code=*/std::nullopt,
                                       std::vector<uint8_t>({1}));
            } else {
              pending =
                  base::BindOnce(std::move(callback),
                                 BluetoothGattService::GattErrorCode::kFailed,
                                 /*value=*/std::vector<uint8_t>());
            }
            device_ptr->PushPendingCallback(std::move(pending));
            if (disconnect) {
              device_ptr->SetConnected(false);
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&NotifyDeviceChanged,
                                 base::RetainedRef(adapter_ptr), device_ptr));
            }
          }));

  ON_CALL(*measurement_interval, WriteRemoteCharacteristic_(_, _, _, _))
      .WillByDefault(
          Invoke([adapter_ptr, device_ptr, disconnect, succeeds](
                     const std::vector<uint8_t>& value,
                     BluetoothRemoteGattCharacteristic::WriteType write_type,
                     base::OnceClosure& callback,
                     BluetoothRemoteGattCharacteristic::ErrorCallback&
                         error_callback) {
            base::OnceClosure pending;
            if (succeeds) {
              pending = std::move(callback);
            } else {
              pending =
                  base::BindOnce(std::move(error_callback),
                                 BluetoothGattService::GattErrorCode::kFailed);
            }
            device_ptr->PushPendingCallback(std::move(pending));
            if (disconnect) {
              device_ptr->SetConnected(false);
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&NotifyDeviceChanged,
                                 base::RetainedRef(adapter_ptr), device_ptr));
            }
          }));

  ON_CALL(*measurement_interval, DeprecatedWriteRemoteCharacteristic_(_, _, _))
      .WillByDefault(Invoke(
          [adapter_ptr, device_ptr, disconnect, succeeds](
              const std::vector<uint8_t>& value, base::OnceClosure& callback,
              BluetoothRemoteGattCharacteristic::ErrorCallback&
                  error_callback) {
            base::OnceClosure pending;
            if (succeeds) {
              pending = std::move(callback);
            } else {
              pending =
                  base::BindOnce(std::move(error_callback),
                                 BluetoothGattService::GattErrorCode::kFailed);
            }
            device_ptr->PushPendingCallback(std::move(pending));
            if (disconnect) {
              device_ptr->SetConnected(false);
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&NotifyDeviceChanged,
                                 base::RetainedRef(adapter_ptr), device_ptr));
            }
          }));

  ON_CALL(*measurement_interval, StartNotifySession_(_, _))
      .WillByDefault(Invoke(
          [adapter_ptr, device_ptr, measurement_ptr, disconnect, succeeds](
              BluetoothRemoteGattCharacteristic::NotifySessionCallback&
                  callback,
              BluetoothRemoteGattCharacteristic::ErrorCallback&
                  error_callback) {
            base::OnceClosure pending;
            if (succeeds) {
              pending = base::BindOnce(
                  std::move(callback),
                  GetBaseGATTNotifySession(measurement_ptr->GetWeakPtr()));
            } else {
              pending =
                  base::BindOnce(std::move(error_callback),
                                 BluetoothGattService::GattErrorCode::kFailed);
            }
            device_ptr->PushPendingCallback(std::move(pending));
            if (disconnect) {
              device_ptr->SetConnected(false);
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&NotifyDeviceChanged,
                                 base::RetainedRef(adapter_ptr), device_ptr));
            }
          }));

  auto user_descriptor = std::make_unique<NiceMockBluetoothGattDescriptor>(
      measurement_interval.get(), kCharacteristicUserDescription,
      BluetoothUUID(kUserDescriptionUUID),
      device::BluetoothRemoteGattCharacteristic::PROPERTY_READ);

  ON_CALL(*user_descriptor, ReadRemoteDescriptor_(_))
      .WillByDefault(
          Invoke([adapter_ptr, device_ptr, disconnect, succeeds](
                     BluetoothRemoteGattDescriptor::ValueCallback& callback) {
            base::OnceClosure pending;
            if (succeeds) {
              pending = base::BindOnce(std::move(callback),
                                       /*error_code=*/std::nullopt,
                                       std::vector<uint8_t>({1}));
            } else {
              pending =
                  base::BindOnce(std::move(callback),
                                 BluetoothGattService::GattErrorCode::kFailed,
                                 /*value=*/std::vector<uint8_t>());
            }
            device_ptr->PushPendingCallback(std::move(pending));
            if (disconnect) {
              device_ptr->SetConnected(false);
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&NotifyDeviceChanged,
                                 base::RetainedRef(adapter_ptr), device_ptr));
            }
          }));

  ON_CALL(*user_descriptor, WriteRemoteDescriptor_(_, _, _))
      .WillByDefault(Invoke(
          [adapter_ptr, device_ptr, disconnect, succeeds](
              const std::vector<uint8_t>& value, base::OnceClosure& callback,
              BluetoothRemoteGattDescriptor::ErrorCallback& error_callback) {
            base::OnceClosure pending;
            if (succeeds) {
              pending = std::move(callback);
            } else {
              pending =
                  base::BindOnce(std::move(error_callback),
                                 BluetoothGattService::GattErrorCode::kFailed);
            }
            device_ptr->PushPendingCallback(std::move(pending));
            if (disconnect) {
              device_ptr->SetConnected(false);
              base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                  FROM_HERE,
                  base::BindOnce(&NotifyDeviceChanged,
                                 base::RetainedRef(adapter_ptr), device_ptr));
            }
          }));

  measurement_interval->AddMockDescriptor(std::move(user_descriptor));

  health_thermometer->AddMockCharacteristic(std::move(measurement_interval));
  device->AddMockService(std::move(health_thermometer));
  adapter->AddMockDevice(std::move(device));
  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter> WebTestBluetoothAdapterProvider::
    GetStopNotifySessionFinishesAfterReconnectionAdapter(bool disconnect) {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());
  NiceMockBluetoothAdapter* adapter_ptr = adapter.get();

  auto device(GetConnectableDevice(
      adapter_ptr, "GATT Operation finishes after reconnection Device",
      BluetoothDevice::UUIDList({BluetoothUUID(kGenericAccessServiceUUID),
                                 BluetoothUUID(kHealthThermometerUUID)})));
  NiceMockBluetoothDevice* device_ptr = device.get();

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault([adapter_ptr, device_ptr](
                         BluetoothDevice::GattConnectionCallback callback,
                         std::optional<BluetoothUUID> service_uuid) {
        device_ptr->SetConnected(true);
        std::move(callback).Run(
            std::make_unique<NiceMockBluetoothGattConnection>(
                adapter_ptr, device_ptr->GetAddress()),
            /*error_code=*/std::nullopt);
        device_ptr->RunPendingCallbacks();
      });

  device->AddMockService(GetGenericAccessService(device.get()));

  auto health_thermometer(GetBaseGATTService("Health Thermometer", device.get(),
                                             kHealthThermometerUUID));

  // Measurement Interval
  auto measurement_interval(GetBaseGATTCharacteristic(
      "Measurement Interval", health_thermometer.get(),
      kMeasurementIntervalUUID,
      BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));
  NiceMockBluetoothGattCharacteristic* measurement_ptr =
      measurement_interval.get();

  ON_CALL(*measurement_interval, StartNotifySession_(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter_ptr, device_ptr, measurement_ptr, disconnect]() {
            std::unique_ptr<NiceMockBluetoothGattNotifySession> notify_session =
                std::make_unique<NiceMockBluetoothGattNotifySession>(
                    measurement_ptr->GetWeakPtr());

            ON_CALL(*notify_session, Stop_(_))
                .WillByDefault(Invoke([adapter_ptr, device_ptr, disconnect](
                                          base::OnceClosure& callback) {
                  device_ptr->PushPendingCallback(std::move(callback));

                  if (disconnect) {
                    device_ptr->SetConnected(false);
                    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                        FROM_HERE,
                        base::BindOnce(&NotifyDeviceChanged,
                                       base::RetainedRef(adapter_ptr),
                                       device_ptr));
                  }
                }));
            return notify_session;
          }));

  health_thermometer->AddMockCharacteristic(std::move(measurement_interval));
  device->AddMockService(std::move(health_thermometer));
  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetBlocklistTestAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kBlocklistTestServiceUUID));
  uuids.push_back(BluetoothUUID(kDeviceInformationServiceUUID));
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kHeartRateServiceUUID));
  uuids.push_back(BluetoothUUID(kHumanInterfaceDeviceServiceUUID));

  auto device(
      GetConnectableDevice(adapter.get(), "Blocklist Test Device", uuids));

  device->AddMockService(GetBlocklistTestService(device.get()));
  device->AddMockService(GetDeviceInformationService(device.get()));
  device->AddMockService(GetGenericAccessService(device.get()));
  device->AddMockService(GetHeartRateService(adapter.get(), device.get()));
  device->AddMockService(GetBaseGATTService("Human Interface Device",
                                            device.get(),
                                            kHumanInterfaceDeviceServiceUUID));
  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetFailingConnectionsAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  for (int error = 0; error < BluetoothDevice::NUM_CONNECT_ERROR_CODES;
       error++) {
    adapter->AddMockDevice(GetUnconnectableDevice(
        adapter.get(), static_cast<BluetoothDevice::ConnectErrorCode>(error)));
  }

  return adapter;
}

// static
scoped_refptr<NiceMockBluetoothAdapter>
WebTestBluetoothAdapterProvider::GetFailingGATTOperationsAdapter() {
  scoped_refptr<NiceMockBluetoothAdapter> adapter(GetEmptyAdapter());

  const std::string errorsServiceUUID = errorUUID(0xA0);

  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(errorsServiceUUID));

  auto device(GetConnectableDevice(adapter.get(), "Errors Device", uuids));

  device->AddMockService(GetDisconnectingService(adapter.get(), device.get()));

  auto service(
      GetBaseGATTService("Errors Service", device.get(), errorsServiceUUID));

  for (int error =
           static_cast<int>(BluetoothGattService::GattErrorCode::kUnknown);
       error <=
       static_cast<int>(BluetoothGattService::GattErrorCode::kMaxValue);
       error++) {
    service->AddMockCharacteristic(GetErrorCharacteristic(
        service.get(),
        static_cast<BluetoothGattService::GattErrorCode>(error)));
  }

  device->AddMockService(std::move(service));
  adapter->AddMockDevice(std::move(device));

  return adapter;
}

// Devices

// static
std::unique_ptr<NiceMockBluetoothDevice>
WebTestBluetoothAdapterProvider::GetBaseDevice(
    MockBluetoothAdapter* adapter,
    const char* device_name,
    device::BluetoothDevice::UUIDList uuids,
    const std::string& address) {
  auto device = std::make_unique<NiceMockBluetoothDevice>(
      adapter, 0x1F00 /* Bluetooth class */, device_name, address,
      false /* paired */, false /* connected */);

  for (const auto& uuid : uuids) {
    device->AddUUID(uuid);
  }

  // Using Invoke allows the device returned from this method to be futher
  // modified and have more services added to it. The call to ::GetGattServices
  // will invoke ::GetMockServices, returning all services added up to that
  // time.
  ON_CALL(*device, GetGattServices())
      .WillByDefault(
          Invoke(device.get(), &MockBluetoothDevice::GetMockServices));
  // The call to BluetoothDevice::GetGattService will invoke ::GetMockService
  // which returns a service matching the identifier provided if the service
  // was added to the mock.
  ON_CALL(*device, GetGattService(_))
      .WillByDefault(
          Invoke(device.get(), &MockBluetoothDevice::GetMockService));

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(RunOnceCallback<0>(
          /*connection=*/nullptr, BluetoothDevice::ERROR_UNSUPPORTED_DEVICE));

  auto* device_ptr = device.get();
  ON_CALL(*device, Pair(_, _))
      .WillByDefault(
          WithArg<1>([device_ptr](BluetoothDevice::ConnectCallback callback) {
            device_ptr->SetPaired(/*paired=*/true);
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback),
                                          /*error_code=*/std::nullopt));
          }));

  return device;
}

// static
std::unique_ptr<NiceMockBluetoothDevice>
WebTestBluetoothAdapterProvider::GetBatteryDevice(
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kBatteryServiceUUID));

  return GetBaseDevice(adapter, "Battery Device", uuids, makeMACAddress(0x1));
}

// static
std::unique_ptr<NiceMockBluetoothDevice>
WebTestBluetoothAdapterProvider::GetGlucoseDevice(
    MockBluetoothAdapter* adapter) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kGlucoseServiceUUID));
  uuids.push_back(BluetoothUUID(kTxPowerServiceUUID));

  auto device =
      GetBaseDevice(adapter, "Glucose Device", uuids, makeMACAddress(0x2));
  device->SetManufacturerData({{0x0001, {1, 2}}, {0x0002, {3, 4}}});

  return device;
}

// static
std::unique_ptr<NiceMockBluetoothDevice>
WebTestBluetoothAdapterProvider::GetConnectableDevice(
    device::MockBluetoothAdapter* adapter,
    const char* device_name,
    BluetoothDevice::UUIDList uuids,
    const std::string& address) {
  auto device(GetBaseDevice(adapter, device_name, uuids, address));

  MockBluetoothDevice* device_ptr = device.get();

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault([adapter, device_ptr](
                         BluetoothDevice::GattConnectionCallback callback,
                         std::optional<BluetoothUUID> service_uuid) {
        device_ptr->SetConnected(true);
        std::move(callback).Run(
            std::make_unique<NiceMockBluetoothGattConnection>(
                adapter, device_ptr->GetAddress()),
            /*error_code=*/std::nullopt);
      });

  ON_CALL(*device, IsGattServicesDiscoveryComplete())
      .WillByDefault(Return(true));

  return device;
}

// static
std::unique_ptr<NiceMockBluetoothDevice>
WebTestBluetoothAdapterProvider::GetUnconnectableDevice(
    MockBluetoothAdapter* adapter,
    BluetoothDevice::ConnectErrorCode error_code,
    const char* device_name) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(errorUUID(error_code)));

  auto device(
      GetBaseDevice(adapter, device_name, uuids, makeMACAddress(error_code)));

  ON_CALL(*device, CreateGattConnection(_, _))
      .WillByDefault(RunOnceCallback<0>(/*connection=*/nullptr, error_code));

  return device;
}

// static
std::unique_ptr<NiceMockBluetoothDevice>
WebTestBluetoothAdapterProvider::GetHeartRateDevice(
    MockBluetoothAdapter* adapter,
    const char* device_name) {
  BluetoothDevice::UUIDList uuids;
  uuids.push_back(BluetoothUUID(kGenericAccessServiceUUID));
  uuids.push_back(BluetoothUUID(kHeartRateServiceUUID));

  return GetConnectableDevice(adapter, device_name, uuids);
}

// Services

// static
std::unique_ptr<NiceMockBluetoothGattService>
WebTestBluetoothAdapterProvider::GetBaseGATTService(
    const std::string& identifier,
    MockBluetoothDevice* device,
    const std::string& uuid) {
  auto service = std::make_unique<NiceMockBluetoothGattService>(
      device, identifier, BluetoothUUID(uuid), /*is_primary=*/true);

  return service;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
WebTestBluetoothAdapterProvider::GetBlocklistTestService(
    device::MockBluetoothDevice* device) {
  auto blocklist_test_service(
      GetBaseGATTService("Blocklist Test", device, kBlocklistTestServiceUUID));

  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      blocklist_exclude_reads_characteristic(GetBaseGATTCharacteristic(
          "Excluded Reads Characteristic", blocklist_test_service.get(),
          kBlocklistExcludeReadsCharacteristicUUID,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ |
              BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

  // Crash if ReadRemoteCharacteristic called. Not using GoogleMock's Expect
  // because this is used in web tests that may not report a mock expectation
  // error correctly as a web test failure.
  ON_CALL(*blocklist_exclude_reads_characteristic, ReadRemoteCharacteristic_(_))
      .WillByDefault(
          Invoke([](BluetoothRemoteGattCharacteristic::ValueCallback&) {
            NOTREACHED_IN_MIGRATION();
          }));

  // Write response.
  ON_CALL(*blocklist_exclude_reads_characteristic,
          WriteRemoteCharacteristic_(_, _, _, _))
      .WillByDefault(RunCallback<2 /* success callback */>());

  // Write response.
  ON_CALL(*blocklist_exclude_reads_characteristic,
          DeprecatedWriteRemoteCharacteristic_(_, _, _))
      .WillByDefault(RunCallback<1 /* success callback */>());

  blocklist_test_service->AddMockCharacteristic(
      std::move(blocklist_exclude_reads_characteristic));

  return blocklist_test_service;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
WebTestBluetoothAdapterProvider::GetDeviceInformationService(
    device::MockBluetoothDevice* device) {
  auto device_information(GetBaseGATTService("Device Information", device,
                                             kDeviceInformationServiceUUID));

  auto serial_number_string(GetBaseGATTCharacteristic(
      "Serial Number String", device_information.get(), kSerialNumberStringUUID,
      BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  // Crash if ReadRemoteCharacteristic called. Not using GoogleMock's Expect
  // because this is used in web tests that may not report a mock expectation
  // error correctly as a web test failure.
  ON_CALL(*serial_number_string, ReadRemoteCharacteristic_(_))
      .WillByDefault(
          Invoke([](BluetoothRemoteGattCharacteristic::ValueCallback&) {
            NOTREACHED_IN_MIGRATION();
          }));

  device_information->AddMockCharacteristic(std::move(serial_number_string));

  return device_information;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
WebTestBluetoothAdapterProvider::GetGenericAccessService(
    device::MockBluetoothDevice* device) {
  auto generic_access(
      GetBaseGATTService("Generic Access", device, kGenericAccessServiceUUID));

  {  // Device Name:
    auto device_name(GetBaseGATTCharacteristic(
        "Device Name", generic_access.get(), kDeviceNameUUID,
        BluetoothRemoteGattCharacteristic::PROPERTY_READ |
            BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

    // Read response.
    std::vector<uint8_t> device_name_value;
    if (std::optional<std::string> name = device->GetName()) {
      device_name_value.assign(name.value().begin(), name.value().end());
    }
    ON_CALL(*device_name, ReadRemoteCharacteristic_(_))
        .WillByDefault(RunCallbackWithResult<0>(/*error_code=*/std::nullopt,
                                                device_name_value));

    // Write response.
    ON_CALL(*device_name, WriteRemoteCharacteristic_(_, _, _, _))
        .WillByDefault(RunCallback<2 /* success callback */>());

    // Write response.
    ON_CALL(*device_name, DeprecatedWriteRemoteCharacteristic_(_, _, _))
        .WillByDefault(RunCallback<1 /* success callback */>());

    generic_access->AddMockCharacteristic(std::move(device_name));
  }

  {  // Peripheral Privacy Flag:
    std::unique_ptr<NiceMockBluetoothGattCharacteristic>
        peripheral_privacy_flag(GetBaseGATTCharacteristic(
            "Peripheral Privacy Flag", generic_access.get(),
            kPeripheralPrivacyFlagUUID,
            BluetoothRemoteGattCharacteristic::PROPERTY_READ |
                BluetoothRemoteGattCharacteristic::PROPERTY_WRITE));

    // Read response.
    std::vector<uint8_t> value(1);
    value[0] = false;

    ON_CALL(*peripheral_privacy_flag, ReadRemoteCharacteristic_(_))
        .WillByDefault(
            RunCallbackWithResult<0>(/*error_code=*/std::nullopt, value));

    // Crash if WriteRemoteCharacteristic called. Not using GoogleMock's Expect
    // because this is used in web tests that may not report a mock
    // expectation error correctly as a web test failure.
    ON_CALL(*peripheral_privacy_flag, WriteRemoteCharacteristic_(_, _, _, _))
        .WillByDefault(Invoke(
            [](const std::vector<uint8_t>&,
               BluetoothRemoteGattCharacteristic::WriteType, base::OnceClosure&,
               BluetoothRemoteGattCharacteristic::ErrorCallback&) {
              NOTREACHED_IN_MIGRATION();
            }));

    // Crash if WriteRemoteCharacteristic called. Not using GoogleMock's Expect
    // because this is used in web tests that may not report a mock
    // expectation error correctly as a web test failure.
    ON_CALL(*peripheral_privacy_flag,
            DeprecatedWriteRemoteCharacteristic_(_, _, _))
        .WillByDefault(
            Invoke([](const std::vector<uint8_t>&, base::OnceClosure&,
                      BluetoothRemoteGattCharacteristic::ErrorCallback&) {
              NOTREACHED_IN_MIGRATION();
            }));

    generic_access->AddMockCharacteristic(std::move(peripheral_privacy_flag));
  }

  return generic_access;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
WebTestBluetoothAdapterProvider::GetHeartRateService(
    MockBluetoothAdapter* adapter,
    MockBluetoothDevice* device) {
  auto heart_rate(
      GetBaseGATTService("Heart Rate", device, kHeartRateServiceUUID));

  // Heart Rate Measurement
  auto heart_rate_measurement(GetBaseGATTCharacteristic(
      "Heart Rate Measurement", heart_rate.get(), kHeartRateMeasurementUUID,
      BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY));
  NiceMockBluetoothGattCharacteristic* measurement_ptr =
      heart_rate_measurement.get();

  ON_CALL(*heart_rate_measurement, StartNotifySession_(_, _))
      .WillByDefault(RunCallbackWithResult<0 /* success_callback */>(
          [adapter, measurement_ptr]() {
            auto notify_session(
                GetBaseGATTNotifySession(measurement_ptr->GetWeakPtr()));

            std::vector<uint8_t> rate(1 /* size */);
            rate[0] = 60;

            notify_session->StartTestNotifications(adapter, measurement_ptr,
                                                   rate);

            return notify_session;
          }));

  // Body Sensor Location Characteristic (Chest)
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      body_sensor_location_chest(GetBaseGATTCharacteristic(
          "Body Sensor Location Chest", heart_rate.get(), kBodySensorLocation,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  ON_CALL(*body_sensor_location_chest, ReadRemoteCharacteristic_(_))
      .WillByDefault(RunCallbackWithResult<0>(
          /*error_code=*/std::nullopt, std::vector<uint8_t>({1} /* Chest */)));

  // Body Sensor Location Characteristic (Wrist)
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      body_sensor_location_wrist(GetBaseGATTCharacteristic(
          "Body Sensor Location Wrist", heart_rate.get(), kBodySensorLocation,
          BluetoothRemoteGattCharacteristic::PROPERTY_READ));

  ON_CALL(*body_sensor_location_wrist, ReadRemoteCharacteristic_(_))
      .WillByDefault(RunCallbackWithResult<0>(
          /*error_code=*/std::nullopt, std::vector<uint8_t>({2} /* Wrist */)));

  heart_rate->AddMockCharacteristic(std::move(heart_rate_measurement));
  heart_rate->AddMockCharacteristic(std::move(body_sensor_location_chest));
  heart_rate->AddMockCharacteristic(std::move(body_sensor_location_wrist));

  return heart_rate;
}

// static
std::unique_ptr<NiceMockBluetoothGattService>
WebTestBluetoothAdapterProvider::GetDisconnectingService(
    MockBluetoothAdapter* adapter,
    MockBluetoothDevice* device) {
  // Set up a service and a characteristic to disconnect the device when it's
  // written to.
  std::unique_ptr<NiceMockBluetoothGattService> disconnection_service =
      GetBaseGATTService("Disconnection", device,
                         kRequestDisconnectionServiceUUID);
  std::unique_ptr<NiceMockBluetoothGattCharacteristic>
      disconnection_characteristic(GetBaseGATTCharacteristic(
          "Disconnection Characteristic", disconnection_service.get(),
          kRequestDisconnectionCharacteristicUUID,
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITE_WITHOUT_RESPONSE));
  ON_CALL(*disconnection_characteristic, WriteRemoteCharacteristic_(_, _, _, _))
      .WillByDefault(
          Invoke([adapter, device](
                     const std::vector<uint8_t>& value,
                     BluetoothRemoteGattCharacteristic::WriteType write_type,
                     base::OnceClosure& success,
                     BluetoothRemoteGattCharacteristic::ErrorCallback& error) {
            device->SetConnected(false);
            for (auto& observer : adapter->GetObservers())
              observer.DeviceChanged(adapter, device);
            std::move(success).Run();
          }));
  ON_CALL(*disconnection_characteristic,
          DeprecatedWriteRemoteCharacteristic_(_, _, _))
      .WillByDefault(Invoke(
          [adapter, device](
              const std::vector<uint8_t>& value, base::OnceClosure& success,
              BluetoothRemoteGattCharacteristic::ErrorCallback& error) {
            device->SetConnected(false);
            for (auto& observer : adapter->GetObservers())
              observer.DeviceChanged(adapter, device);
            std::move(success).Run();
          }));

  disconnection_service->AddMockCharacteristic(
      std::move(disconnection_characteristic));
  return disconnection_service;
}

// Characteristics

// static
std::unique_ptr<NiceMockBluetoothGattCharacteristic>
WebTestBluetoothAdapterProvider::GetBaseGATTCharacteristic(
    const std::string& identifier,
    MockBluetoothGattService* service,
    const std::string& uuid,
    BluetoothRemoteGattCharacteristic::Properties properties) {
  auto characteristic = std::make_unique<NiceMockBluetoothGattCharacteristic>(
      service, identifier, BluetoothUUID(uuid), properties,
      BluetoothGattCharacteristic::Permission::PERMISSION_NONE);

  ON_CALL(*characteristic, ReadRemoteCharacteristic_(_))
      .WillByDefault(RunCallbackWithResult<0>(
          BluetoothGattService::GattErrorCode::kNotSupported,
          /*value=*/std::vector<uint8_t>()));

  ON_CALL(*characteristic, WriteRemoteCharacteristic_(_, _, _, _))
      .WillByDefault(
          RunCallback<3>(BluetoothGattService::GattErrorCode::kNotSupported));

  ON_CALL(*characteristic, DeprecatedWriteRemoteCharacteristic_(_, _, _))
      .WillByDefault(
          RunCallback<2>(BluetoothGattService::GattErrorCode::kNotSupported));

  ON_CALL(*characteristic, StartNotifySession_(_, _))
      .WillByDefault(
          RunCallback<1>(BluetoothGattService::GattErrorCode::kNotSupported));

  return characteristic;
}

// static
std::unique_ptr<NiceMockBluetoothGattCharacteristic>
WebTestBluetoothAdapterProvider::GetErrorCharacteristic(
    MockBluetoothGattService* service,
    BluetoothGattService::GattErrorCode error_code) {
  // Error UUIDs start at 0xA1.
  uint32_t error_alias = static_cast<int>(error_code) + 0xA1;
  auto characteristic(GetBaseGATTCharacteristic(
      // Use the UUID to generate unique identifiers.
      "Error Characteristic " + errorUUID(error_alias), service,
      errorUUID(error_alias),
      BluetoothRemoteGattCharacteristic::PROPERTY_READ |
          BluetoothRemoteGattCharacteristic::PROPERTY_WRITE |
          BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE));

  // Read response.
  ON_CALL(*characteristic, ReadRemoteCharacteristic_(_))
      .WillByDefault(
          RunCallbackWithResult<0>(error_code,
                                   /*value=*/std::vector<uint8_t>()));

  // Write response.
  ON_CALL(*characteristic, WriteRemoteCharacteristic_(_, _, _, _))
      .WillByDefault(RunCallback<3 /* error_callback */>(error_code));

  // Write response.
  ON_CALL(*characteristic, DeprecatedWriteRemoteCharacteristic_(_, _, _))
      .WillByDefault(RunCallback<2 /* error_callback */>(error_code));

  // StartNotifySession response
  ON_CALL(*characteristic, StartNotifySession_(_, _))
      .WillByDefault(RunCallback<1 /* error_callback */>(error_code));

  // Add error descriptor to |characteristic|
  auto error_descriptor = std::make_unique<NiceMockBluetoothGattDescriptor>(
      characteristic.get(), kCharacteristicUserDescription,
      BluetoothUUID(kUserDescriptionUUID),
      device::BluetoothRemoteGattCharacteristic::PROPERTY_READ);

  ON_CALL(*error_descriptor, ReadRemoteDescriptor_(_))
      .WillByDefault(
          RunCallbackWithResult<0>(error_code,
                                   /*value=*/std::vector<uint8_t>()));

  ON_CALL(*error_descriptor, WriteRemoteDescriptor_(_, _, _))
      .WillByDefault(RunCallback<2 /* error_callback */>(error_code));

  characteristic->AddMockDescriptor(std::move(error_descriptor));

  return characteristic;
}

// Notify sessions

// static
std::unique_ptr<NiceMockBluetoothGattNotifySession>
WebTestBluetoothAdapterProvider::GetBaseGATTNotifySession(
    base::WeakPtr<device::BluetoothRemoteGattCharacteristic> characteristic) {
  auto session =
      std::make_unique<NiceMockBluetoothGattNotifySession>(characteristic);

  ON_CALL(*session, Stop_(_))
      .WillByDefault(testing::DoAll(
          InvokeWithoutArgs(
              session.get(),
              &MockBluetoothGattNotifySession::StopTestNotifications),
          RunCallback<0>()));

  return session;
}

// Helper functions

// static
std::string WebTestBluetoothAdapterProvider::errorUUID(uint32_t alias) {
  return base::StringPrintf("%08x-97e5-4cd7-b9f1-f5a427670c59", alias);
}

// static
std::string WebTestBluetoothAdapterProvider::makeMACAddress(uint64_t addr) {
  return device::CanonicalizeBluetoothAddress(
      base::StringPrintf("%012" PRIx64, addr));
}

}  // namespace content
