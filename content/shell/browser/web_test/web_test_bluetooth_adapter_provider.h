// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_characteristic.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_notify_session.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_service.h"

namespace content {

// Implements fake adapters with named mock data set for use in tests as a
// result of web tests calling testRunner.setBluetoothFakeAdapter.

// An adapter named 'FooAdapter' in
// https://webbluetoothcg.github.io/web-bluetooth/tests/ is provided by a
// corresponding 'GetFooAdapter' function, and re-documented here to capture
// differences between our implementations.

class WebTestBluetoothAdapterProvider {
 public:
  // Returns a BluetoothAdapter. Its behavior depends on |fake_adapter_name|.
  static scoped_refptr<device::BluetoothAdapter> GetBluetoothAdapter(
      const std::string& fake_adapter_name);

 private:
  // Adapters

  // |BaseAdapter|
  // Devices added:
  //  None.
  // Mock Functions:
  //  - GetDevices:
  //      Returns a list of devices added to the adapter.
  //  - GetDevice:
  //      Returns a device matching the address provided if the device was
  //      added to the adapter.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetBaseAdapter();

  // |PresentAdapter|
  // Inherits from |BaseAdapter|
  // Devices added:
  //  None.
  // Mock Functions:
  //  - IsPresent: Returns true
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetPresentAdapter();

  // |PoweredAdapter|
  // Inherits from |PresentAdapter|
  // Devices added:
  //  None.
  // Mock Functions:
  //  - IsPowered: Returns true
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetPoweredAdapter();

  // |ScanFilterCheckingAdapter|
  // Inherits from |PoweredAdapter|
  // BluetoothAdapter that asserts that its StartDiscoverySessionWithFilter()
  // method is called with a filter consisting of the standard battery, heart
  // rate, and glucose services.
  // Devices added:
  //  - |BatteryDevice|
  // Mock Functions:
  //  - StartDiscoverySessionWithFilter:
  //      - With correct arguments: Run success callback.
  //      - With incorrect arguments: Mock complains that function with
  //        correct arguments was never called and error callback is called.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetScanFilterCheckingAdapter();

  // |FailStartDiscoveryAdapter|
  // Inherits from |PoweredAdapter|
  // Devices added:
  //  None.
  // Mock Functions:
  //  - StartDiscoverySessionWithFilter:
  //      Run error callback.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetFailStartDiscoveryAdapter();

  // |EmptyAdapter|
  // Inherits from |PoweredAdapter|
  // Devices Added:
  //  None.
  // Mock Functions:
  //  - StartDiscoverySessionWithFilter:
  //      Run success callback with |DiscoverySession|.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetEmptyAdapter();

  // |GlucoseHeartRateAdapter|
  // Inherits from |EmptyAdapter|
  // Devices added:
  //  - |GlucoseDevice|
  //  - |HeartRateDevice|
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetGlucoseHeartRateAdapter();

  // |SecondDiscoveryFindsHeartRateAdapter|
  // Inherits from |PoweredAdapter|
  // Mock Functions:
  //  - StartDiscoverySessionWithFilter:
  //      Run success callback with |DiscoverySession|.
  //      After the first call, adds a |HeartRateDevice|.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetSecondDiscoveryFindsHeartRateAdapter();

  // |DeviceEventAdapter|
  // Inherits from |PoweredAdapter|
  // Internal Structure:
  //  - Connected Heart Rate Device
  //      - IsGattConnected: Returns true.
  //      - UUIDs:
  //         - Heart Rate UUID (0x180d)
  //  - Changing Battery Device
  //      - IsGattConnected: Returns false.
  //      - No UUIDs (A Battery UUID (0x180f) is added by
  //        StartDiscoverySessionWithFilter).
  //  - Non Connected Tx Power Device
  //      - IsGattConnected: Returns false.
  //      - UUIDs:
  //         - Tx Power (0x1804)
  //  - Discovery Generic Access Device
  //      - IsGattConnected: Returns true.
  //      - No UUIDs (A Generic Access UUID (0x1800) is added by
  //        StartDiscoverySessionWithFilter).
  // Mock Functions:
  //  - StartDiscoverySessionWithFilter: Performs the following steps the first
  //    time is called:
  //      1. Post a task to add New Glucose Device (Contains a single
  //         Glucose UUID (0x1808) and no services).
  //      2. Adds a Battery UUID to Changing Battery Device and posts a task
  //         that notifies observers that the device changed.
  //      3. Adds a Generic Access UUID to Discovery Generic Access Device and
  //         posts a task to Notify its services have been discovered.
  //      4. Return a discovery session.
  //    Successive calls just return a discovery session.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetDeviceEventAdapter();

  // |DevicesRemovedAdapter|
  // Inherits from |PoweredAdapter|
  // Internal Structure:
  //  - Connected Heart Rate Device
  //    - IsGattConnected: Returns true.
  //    - UUIDs:
  //      - Heart Rate UUID (0x180d)
  // Mock Functions:
  //  - StartDiscoverySessionWithFilter: Performs the following steps the first
  //    time is called:
  //     1. Post a task to add New Glucose Device (Contains a single
  //        Glucose UUID (0x1808) and no services).
  //     2. Post a task to remove Connected Heart Rate Device.
  //     3. Post a task to remove New Glucose Device.
  //    Successive calls just return a discovery session.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetDevicesRemovedAdapter();

  // |MissingServiceHeartRateAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - UUIDs:
  //         - Generic Access UUID (0x1800)
  //         - Heart Rate UUID (0x180d)
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetMissingServiceHeartRateAdapter();

  // |MissingCharacteristicHeartRateAdapter|
  // Inherits from |EmptyAdapter|
  // The services in this adapter do not contain any characteristics.
  // Internal Structure:
  //   - Heart Rate Device
  //      - UUIDs:
  //         - Generic Access UUID (0x1800)
  //         - Heart Rate UUID (0x180d)
  //      - Services:
  //         - Generic Access Service
  //         - Heart Rate Service
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetMissingCharacteristicHeartRateAdapter();

  // |HeartRateAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - UUIDs:
  //         - Generic Access UUID (0x1800)
  //         - Heart Rate UUID (0x180d)
  //      - Services:
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //         - Heart Rate Service - Characteristics as described in
  //           GetHeartRateService.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetHeartRateAdapter();

  // |GetNoNameDeviceAdapter|
  // Inherits from |EmptyAdapter|
  // Contains a single device with no name and no UUIDs.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetNoNameDeviceAdapter();

  // |GetEmptyNameHeartRateAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - UUIDs:
  //         - Generic Access UUID (0x1800)
  //         - Heart Rate UUID (0x180d)
  //      - Services:
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //            - gap.device_name returns an empty string.
  //         - Heart Rate Service - Characteristics as described in
  //           GetHeartRateService.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetEmptyNameHeartRateAdapter();

  // |GetNoNameHeartRateAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - GetName returns base::null_opt.
  //      - UUIDs:
  //         - Generic Access UUID (0x1800)
  //         - Heart Rate UUID (0x180d)
  //      - Services:
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //            - gap.device_name returns an empty string.
  //         - Heart Rate Service - Characteristics as described in
  //           GetHeartRateService.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetNoNameHeartRateAdapter();

  // |TwoHeartRateServicesAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - UUIDs:
  //         - Generic Access UUID (0x1800)
  //         - Heart Rate UUID (0x180d)
  //         - Heart Rate UUID (0x180d)
  //      - Services:
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //         - Heart Rate Service - Heart Rate Measurement (0x2a37) & Body
  //           Sensor Location (0x2a38).
  //         - Heart Rate Service - Body Sensor Location (0x2a38).
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetTwoHeartRateServicesAdapter();

  // |DisconnectingHeartRateAdapter|
  // Inherits from |HeartRateAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - UUIDs:
  //         - Generic Access UUID (0x1800)
  //         - Heart Rate UUID (0x180d)
  //      - Services:
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //         - Heart Rate Service - Characteristics as described in
  //           GetHeartRateService.
  //         - Request Disconnection Service: - Characteristics as described in
  //           GetDisconnectingService
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetDisconnectingHeartRateAdapter();

  // |DisconnectingHealthThermometerAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //  - Disconnecting Health Thermometer Device
  //    - UUIDs:
  //       - Generic Access UUID (0x1800)
  //       - Health Thermometer UUID (0x1809)
  //    - Services:
  //       - Generic Access Service - Characteristics as described in
  //         GetGenericAccessService.
  //       - Request Disconnection Service: - Characteristics as described in
  //         GetDisconnectingService
  //       - Health Thermometer:
  //         - Measurement Interval (0x2a21):
  //           - Read: Calls GattCharacteristicValueChanged and success
  //               callback with [1].
  //           - Write: Calls success callback.
  //           - StartNotifySession: Run success callback.
  //           - GetProperties: Returns
  //               BluetoothRemoteGattCharacteristic::PROPERTY_READ
  //           - Descriptors (if |addDescriptors| input is true)
  //             - User Description (2901)
  //                 - Mock Functions:
  //                   - Read: Calls success callback with
  //                           "gatt.characteristic_user_description".
  //                   - Write: Calls success callback.
  //             - Client Characteristic Configuration (2902)
  //                 Note: This descriptor is blocklisted for writes.
  //             - bad2ddcf-60db-45cd-bef9-fd72b153cf7c
  //                 A test descriptor that is blocklisted.
  //             - bad3ec61-3cc3-4954-9702-7977df514114
  //                 A test descriptor that is exclude read.

  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetDisconnectingHealthThermometer(bool add_descriptors);

  // |ServicesDiscoveredAfterReconnectionAdapter|(disconnect)
  // Inherits from |HeartRateAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - UUIDs:
  //         - Generic Access UUID (0x1800)
  //         - Heart Rate UUID (0x180d)
  //      - Services:
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //         - Heart Rate Service - Characteristics as described in
  //           GetHeartRateService.
  //      - CreateGattConnection: When called before IsGattDiscoveryComplete,
  //          runs success callback with a new Gatt connection. When called
  //          after IsGattDiscoveryComplete runs success callback with a new
  //          Gatt connection and notifies of services discovered.
  //      - IsGattDiscoveryComplete: The first time this function is called,
  //          it adds two services (Generic Access and Heart Rate) and
  //          if |disconnect| is true disconnects the device and returns false.
  //          After that it just returns true.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetServicesDiscoveredAfterReconnectionAdapter(bool disconnect);

  // |GATTOperationFinishesAfterReconnectionAdapter|(disconnect, succeeds)
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Health Thermometer Device
  //      - UUIDs:
  //         - Generic Access UUID (0x1800)
  //         - Health Thermometer UUID (0x1809)
  //      - Services:
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //         - Health Thermometer
  //            - Measurement Interval:
  //               - Read: If |succeeds| is true, saves a succeeding callback,
  //                 otherwise it saves a failing callback. This callback
  //                 is run during CreateGattConnection. If |disconnect| is true
  //                 disconnects the device.
  //               - Write: If |succeeds| is true, saves a succeeding callback,
  //                 otherwise it saves a failing callback. This callback is run
  //                 during CreateGattConnection. If |disconnect| is true
  //                 disconnects the device.
  //               - StartNotifySession: If |succeeds| is true, saves a
  //                 succeeding callback, otherwise it saves a failing callback.
  //                 This calback is run during CreateGattConnection. If
  //                 |disconnect| is true disconnects the device.
  //               - user_descriptor
  //                 - Operations read / write nearly identical to the read and
  //                   write methods of the characteristic.
  //                 - Read: If |succeeds| is true, saves a succeeding callback,
  //                   otherwise it saves a failing callback.
  //                 - Write: If |succeeds| is true, saves a succeeding callback
  //                   otherwise it saves a failing callback.
  //         - CreateGattConnection: Runs success callback with a new GATT
  //           connection and runs any pending GATT operation callbacks.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetGATTOperationFinishesAfterReconnectionAdapter(bool disconnect,
                                                   bool succeeds);

  // |StopNotifySessionFinishesAfterReconnection|(disconnect)
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Health Thermometer Device
  //      - UUIDs:
  //         - Generic Access UUID (0x1800)
  //         - Health Thermometer UUID (0x1809)
  //      - Services:
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //         - Health Thermometer
  //            - Measurement Interval:
  //               - StartNotifySession: Calls the success callback with a
  //                 NotifySession whose Stop function: saves a callback and
  //                 if |disconnect| is true disconnects the device.
  //         - CreateGattConnection: Runs success callback with a new GATT
  //           connection and runs any pending GATT operation callbacks.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetStopNotifySessionFinishesAfterReconnectionAdapter(bool disconnect);

  // |BlocklistTestAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - |ConnectableDevice|(adapter, "Blocklist Test Device", uuids)
  //      - UUIDs:
  //         - Blocklist Test Service UUID
  //           (611c954a-263b-4f4a-aab6-01ddb953f985)
  //         - Device Information UUID (0x180a)
  //         - Generic Access UUID (0x1800)
  //         - Heart Rate UUID (0x180d)
  //         - Human Interface Device UUID (0x1812) (a blocklisted service)
  //      - Services:
  //         - Blocklist Test Service - Characteristics as described in
  //           GetBlocklistTestService.
  //         - Device Information Service - Characteristics as described in
  //           GetDeviceInformationService.
  //         - Generic Access Service - Characteristics as described in
  //           GetGenericAccessService.
  //         - Heart Rate Service - Characteristics as described in
  //           GetHeartRateService.
  //         - Human Interface Device Service - No characteristics needed
  //           because the service is blocklisted.
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetBlocklistTestAdapter();

  // |DelayedServicesDiscoveryAdapter|
  // Inherits from |EmptyAdapter|
  // Internal Structure:
  //   - Heart Rate Device
  //      - Generic Access UUID (0x1800)
  //      - Heart Rate UUID (0x180D)
  //      - Heart Rate Service (No services will be returned the first time
  //                            GetServices is called. Subsequent calls will
  //                            return the Heart Rate Service)
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetDelayedServicesDiscoveryAdapter();

  // |FailingConnectionsAdapter|
  // Inherits from |EmptyAdapter|
  // FailingConnectionsAdapter holds a device for each type of connection error
  // that can occur. This way we don’t need to create an adapter for each type
  // of error. Each of the devices has a service with a different UUID so that
  // they can be accessed by using different filters.
  // See connectErrorUUID() declaration below.
  // Internal Structure:
  //  - UnconnectableDevice(BluetoothDevice::ERROR_AUTH_CANCELED)
  //    connectErrorUUID(0x0)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_AUTH_FAILED)
  //    connectErrorUUID(0x1)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_AUTH_REJECTED)
  //    connectErrorUUID(0x2)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_AUTH_TIMEOUT)
  //    connectErrorUUID(0x3)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_FAILED)
  //    connectErrorUUID(0x4)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_INPROGRESS)
  //    connectErrorUUID(0x5)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_UNKNOWN)
  //    connectErrorUUID(0x6)
  //  - UnconnectableDevice(BluetoothDevice::ERROR_UNSUPPORTED_DEVICE)
  //    connectErrorUUID(0x7)
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetFailingConnectionsAdapter();

  // |FailingGATTOperationsAdapter|
  // Inherits from |EmptyAdapter|
  // FailingGATTOperationsAdapter holds a device with one
  // service: ErrorsService. This service contains a characteristic for each
  // type of GATT Error that can be thrown. Trying to write or read from these
  // characteristics results in the corresponding error being returned.
  // GetProperties returns the following for all characteristics:
  // (BluetoothRemoteGattCharacteristic::PROPERTY_READ |
  // BluetoothRemoteGattCharacteristic::PROPERTY_WRITE |
  // BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE)
  // Internal Structure:
  //   - ErrorsDevice
  //      - ErrorsService errorUUID(0xA0)
  //          - ErrorCharacteristic(
  //              BluetoothRemoteGattService::GATT_ERROR_UNKNOWN)
  //              errorUUID(0xA1)
  //          - ErrorCharacteristic(
  //              BluetoothRemoteGattService::GATT_ERROR_FAILED)
  //              errorUUID(0xA2)
  //          - ErrorCharacteristic(
  //              BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS)
  //              errorUUID(0xA3)
  //          - ErrorCharacteristic(
  //              BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH)
  //              errorUUID(0xA4)
  //          - ErrorCharacteristic(
  //              BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED)
  //              errorUUID(0xA5)
  //          - ErrorCharacteristic(
  //              BluetoothRemoteGattService::GATT_ERROR_NOT_AUTHORIZED)
  //              errorUUID(0xA6)
  //          - ErrorCharacteristic(
  //              BluetoothRemoteGattService::GATT_ERROR_NOT_PAIRED)
  //              errorUUID(0xA7)
  //          - ErrorCharacteristic(
  //              BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED)
  //              errorUUID(0xA8)
  //      - Request Disconnection Service: - Characteristics as described in
  //          GetDisconnectingService
  static scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  GetFailingGATTOperationsAdapter();

  // Devices

  // |BaseDevice|
  // UUIDs added:
  // None.
  // Services added:
  // None.
  // MockFunctions:
  //  - GetUUIDs:
  //      Returns uuids
  //  - GetGattServices:
  //      Returns a list of all services added to the device.
  //  - GetGattService:
  //      Return a service matching the identifier provided if the service was
  //      added to the mock.
  //  - GetAddress:
  //      Returns: address
  //  - GetName:
  //      Returns: device_name.
  //  - IsPaired:
  //      Returns false.
  //  - IsConnected:
  //      Returns false.
  //  - IsGattConnected:
  //      Returns false.
  //  - ConnectGatt:
  //      Calls error callback with
  //      BluetoothDevice::ConnectErrorCode::ERROR_UNSUPPORTED_DEVICE.
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetBaseDevice(device::MockBluetoothAdapter* adapter,
                const char* device_name = "Base Device",
                device::BluetoothDevice::UUIDList uuids =
                    device::BluetoothDevice::UUIDList(),
                const std::string& address = "00:00:00:00:00:00");

  // |BatteryDevice|
  // Inherits from |BaseDevice|(adapter, "Battery Device", uuids,
  //                            "00:00:00:00:00:01")
  // UUIDs added:
  //   - Battery Service UUID (0x180F)
  // Services added:
  // None.
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetBatteryDevice(device::MockBluetoothAdapter* adapter);

  // |GlucoseDevice|
  // Inherits from |BaseDevice|(adapter, "Glucose Device", uuids,
  //                            "00:00:00:00:00:02")
  // UUIDs added:
  //   - Generic Access (0x1800)
  //   - Glucose UUID (0x1808)
  //   - Tx Power (0x1804)
  // Services added:
  // None.
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetGlucoseDevice(device::MockBluetoothAdapter* adapter);

  // |ConnectableDevice|
  // Inherits from |BaseDevice|(adapter, device_name)
  // UUIDs added:
  // None.
  // Services added:
  // None.
  // Mock Functions:
  //   - CreateGattConnection:
  //       - Run success callback with BaseGATTConnection
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetConnectableDevice(
      device::MockBluetoothAdapter* adapter,
      const char* device_name = "Connectable Device",
      device::BluetoothDevice::UUIDList = device::BluetoothDevice::UUIDList(),
      const std::string& address = "00:00:00:00:00:00");

  // |UnconnectableDevice|
  // Inherits from |BaseDevice|(adapter, device_name)
  // UUIDs added:
  //  - errorUUID(error_code)
  // Services added:
  // None.
  // Mock Functions:
  //  - CreateGATTConnection:
  //      - Run error callback with error_type
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetUnconnectableDevice(device::MockBluetoothAdapter* adapter,
                         device::BluetoothDevice::ConnectErrorCode error_code,
                         const char* device_name = "Unconnectable Device");

  // |HeartRateDevice|
  // Inherits from |ConnectableDevice|(adapter, "Heart Rate Device", uuids)
  // UUIDs added:
  //   - Generic Access (0x1800)
  //   - Heart Rate UUID (0x180D)
  // Services added:
  // None. Each user of the HeartRateDevice is in charge of adding the
  // relevant services, characteristics and descriptors.
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothDevice>>
  GetHeartRateDevice(device::MockBluetoothAdapter* adapter,
                     const char* device_name = "Heart Rate Device");

  // Services

  // |BaseGATTService|(identifier, device, uuid)
  // Characteristics added:
  // None.
  // Mock Functions:
  //   - GetCharacteristics:
  //       Returns a list with all the characteristics added to the service
  //   - GetCharacteristic:
  //       Returns a characteristic matching the identifier provided if the
  //       characteristic was added to the mock.
  //   - GetIdentifier:
  //       Returns: identifier
  //   - GetUUID:
  //       Returns: uuid
  //   - IsLocal:
  //       Returns: false
  //   - IsPrimary:
  //       Returns: true
  //   - GetDevice:
  //       Returns: device
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetBaseGATTService(const std::string& identifier,
                     device::MockBluetoothDevice* device,
                     const std::string& uuid);

  // |DeviceInformationService|
  // Internal Structure:
  //  - Characteristics:
  //     - Serial Number String: (0x2a25) (a blocklisted characteristic)
  //        - Mock Functions:
  //           - Read: Fails test.
  //           - GetProperties: Returns
  //               BluetoothRemoteGattCharacteristic::PROPERTY_READ
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetDeviceInformationService(device::MockBluetoothDevice* device);

  // |BlocklistTestService|
  // Internal Structure:
  //  - Characteristics:
  //     - Blocklist Exclude Reads Characteristic:
  //       (bad1c9a2-9a5b-4015-8b60-1579bbbf2135)
  //        - Mock Functions:
  //           - Read: Fails test.
  //           - Write: Calls success callback.
  //           - GetProperties: Returns
  //               BluetoothRemoteGattCharacteristic::PROPERTY_READ |
  //               BluetoothRemoteGattCharacteristic::PROPERTY_WRITE
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetBlocklistTestService(device::MockBluetoothDevice* device);

  // |GenericAccessService|
  // Internal Structure:
  //  - Characteristics:
  //     - Device Name: (0x2A00)
  //        - Mock Functions:
  //           - Read: Calls success callback with device's name.
  //           - Write: Calls success callback.
  //           - GetProperties: Returns
  //               BluetoothRemoteGattCharacteristic::PROPERTY_READ |
  //               BluetoothRemoteGattCharacteristic::PROPERTY_WRITE
  //     - Peripheral Privacy Flag: (0x2A02) (blocklisted for writes)
  //        - Mock Functions:
  //           - Read: Calls success callback with boolean value 'false'.
  //           - Write: Fails test.
  //           - GetProperties: Returns
  //               BluetoothRemoteGattCharacteristic::PROPERTY_READ |
  //               BluetoothRemoteGattCharacteristic::PROPERTY_WRITE
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetGenericAccessService(device::MockBluetoothDevice* device);

  // |HeartRateService|
  // Internal Structure:
  //  - Characteristics:
  //     - Heart Rate Measurement (0x2a37)
  //        - Mock Functions:
  //           - StartNotifySession: Sets a timer to call
  //               GattCharacteristicValueChanged every 10ms and calls
  //               success callback with a
  //               BaseGATTNotifySession(characteristic_instance_id)
  //               TODO: Instead of a timer we should be able to tell the fake
  //               to call GattCharacteristicValueChanged from js.
  //               https://crbug.com/543884
  //           - GetProperties: Returns
  //               BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY
  //     - Body Sensor Location (0x2a38)
  //        - Mock Functions:
  //           - Read: Calls GattCharacteristicValueChanged and success
  //               callback with [1] which corresponds to chest.
  //           - GetProperties: Returns
  //               BluetoothRemoteGattCharacteristic::PROPERTY_READ
  //     - Body Sensor Location (0x2a38)
  //        - Mock Functions:
  //           - Read: Calls GattCharacteristicValueChanged and success
  //               callback with [2] which corresponds to wrist.
  //           - GetProperties: Returns
  //               BluetoothRemoteGattCharacteristic::PROPERTY_READ
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetHeartRateService(device::MockBluetoothAdapter* adapter,
                      device::MockBluetoothDevice* device);

  // |DisconnectingService|
  // Internal Structure:
  //  - Characteristics:
  //     - Request Disconnection Characteristic (
  //         01d7d889-7451-419f-aeb8-d65e7b9277af)
  //       - Write: Sets the device to disconnected and calls DeviceChanged.
  static std::unique_ptr<testing::NiceMock<device::MockBluetoothGattService>>
  GetDisconnectingService(device::MockBluetoothAdapter* adapter,
                          device::MockBluetoothDevice* device);

  // Characteristics

  // |BaseCharacteristic|(identifier, service, uuid)
  // Descriptors added:
  // None.
  // Mock Functions:
  //   - TODO(ortuno): http://crbug.com/483347 GetDescriptors:
  //       Returns: all descriptors added to the characteristic
  //   - TODO(ortuno): http://crbug.com/483347 GetDescriptor:
  //       Returns the descriptor matching the identifier provided if the
  //       descriptor was added to the characteristic.
  //   - GetIdentifier:
  //       Returns: identifier
  //   - GetUUID:
  //       Returns: uuid
  //   - IsLocal:
  //       Returns: false
  //   - GetService:
  //       Returns: service
  //   - GetProperties:
  //       Returns: NULL
  //   - GetPermissions:
  //       Returns: NULL
  //   - ReadRemoteCharacteristic:
  //       Calls error callback with GATT_ERROR_NOT_SUPPORTED.
  //   - WriteRemoteCharacteristic:
  //       Calls error callback with GATT_ERROR_NOT_SUPPORTED.
  //   - StartNotifySession:
  //       Calls error callback with GATT_ERROR_NOT_SUPPORTED.
  static std::unique_ptr<
      testing::NiceMock<device::MockBluetoothGattCharacteristic>>
  GetBaseGATTCharacteristic(
      const std::string& identifier,
      device::MockBluetoothGattService* service,
      const std::string& uuid,
      device::BluetoothRemoteGattCharacteristic::Properties properties);

  // |ErrorCharacteristic|(service, error_type)
  // Inherits from BaseCharacteristic(service, errorUUID(error_type + 0xA1))
  // Descriptors added:
  // None.
  // Mock Functions:
  //   - ReadRemoteCharacteristic:
  //       Run error callback with error_type
  //   - WriteRemoteCharacteristic:
  //       Run error callback with error_type
  //   - StartNotifySession:
  //       Run error callback with error_type
  static std::unique_ptr<
      testing::NiceMock<device::MockBluetoothGattCharacteristic>>
  GetErrorCharacteristic(
      device::MockBluetoothGattService* service,
      device::BluetoothRemoteGattService::GattErrorCode error_code);

  // Notify Sessions

  // |BaseGATTNotifySession|(characteristic_identifier)
  // Mock Functions:
  //   - GetCharacteristicIdentifier:
  //       Returns: characteristic_identifier
  //   - IsActive:
  //       Returns: true
  //   - Stop:
  //       Stops calling GattCharacteristicValueChanged and runs callback.
  static std::unique_ptr<
      testing::NiceMock<device::MockBluetoothGattNotifySession>>
  GetBaseGATTNotifySession(
      base::WeakPtr<device::BluetoothRemoteGattCharacteristic> characteristic);

  // Helper functions:

  // errorUUID(alias) returns a UUID with the top 32 bits of
  // "00000000-97e5-4cd7-b9f1-f5a427670c59" replaced with the bits of |alias|.
  // For example, errorUUID(0xDEADBEEF) returns
  // "deadbeef-97e5-4cd7-b9f1-f5a427670c59". The bottom 96 bits of error UUIDs
  // were generated as a type 4 (random) UUID.
  static std::string errorUUID(uint32_t alias);

  // Function to turn an integer into an MAC address of the form
  // XX:XX:XX:XX:XX:XX. For example makeMACAddress(0xdeadbeef)
  // returns "00:00:DE:AD:BE:EF".
  static std::string makeMACAddress(uint64_t addr);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_BLUETOOTH_ADAPTER_PROVIDER_H_
