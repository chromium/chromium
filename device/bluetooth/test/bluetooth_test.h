// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_
#define DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class BluetoothAdapter;
class BluetoothDevice;
class BluetoothLocalGattCharacteristic;
class BluetoothLocalGattDescriptor;

// A test fixture for Bluetooth that abstracts platform specifics for creating
// and controlling fake low level objects.
//
// Subclasses on each platform implement this, and are then typedef-ed to
// BluetoothTest.
class BluetoothTestBase : public testing::Test {
 public:
  enum class Call { EXPECTED, NOT_EXPECTED };

  enum class Result { SUCCESS, FAILURE };

  // List of devices that can be simulated with
  // SimulateConnectedLowEnergyDevice().
  // GENERIC_DEVICE:
  //   - Name:     kTestDeviceName
  //   - Address:  kTestPeripheralUUID1
  //   - Services: [ kTestUUIDGenericAccess ]
  // HEART_RATE_DEVICE:
  //   - Name:     kTestDeviceName
  //   - Address:  kTestPeripheralUUID2
  //   - Services: [ kTestUUIDGenericAccess, kTestUUIDHeartRate]
  enum class ConnectedDeviceType {
    GENERIC_DEVICE,
    HEART_RATE_DEVICE,
  };

  enum class NotifyValueState {
    NONE,
    NOTIFY,
    INDICATE,
  };

  // Utility struct to simplify simulating a low energy device.
  struct LowEnergyDeviceData {
    LowEnergyDeviceData();
    LowEnergyDeviceData(LowEnergyDeviceData&& data);

    LowEnergyDeviceData(const LowEnergyDeviceData&) = delete;
    LowEnergyDeviceData& operator=(const LowEnergyDeviceData&) = delete;

    ~LowEnergyDeviceData();

    std::optional<std::string> name;
    std::string address;
    int8_t rssi = 0;
    std::optional<uint8_t> flags;
    BluetoothDevice::UUIDList advertised_uuids;
    std::optional<int8_t> tx_power;
    BluetoothDevice::ServiceDataMap service_data;
    BluetoothDevice::ManufacturerDataMap manufacturer_data;
    BluetoothTransport transport = BLUETOOTH_TRANSPORT_LE;
  };

  static const char kTestAdapterName[];
  static const char kTestAdapterAddress[];

  static const char kTestDeviceName[];
  static const char kTestDeviceNameEmpty[];
  static const char kTestDeviceNameU2f[];
  static const char kTestDeviceNameCable[];

  static const char kTestDeviceAddress1[];
  static const char kTestDeviceAddress2[];
  static const char kTestDeviceAddress3[];

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.device.bluetooth.test
  enum class TestRSSI {
    LOWEST = -81,
    LOWER = -61,
    LOW = -41,
    MEDIUM = -21,
    HIGH = -1,
  };

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.device.bluetooth.test
  enum class TestTxPower {
    LOWEST = -40,
    LOWER = -20,
  };

  // Services
  static const char kTestUUIDGenericAccess[];
  static const char kTestUUIDGenericAttribute[];
  static const char kTestUUIDImmediateAlert[];
  static const char kTestUUIDLinkLoss[];
  static const char kTestUUIDHeartRate[];
  static const char kTestUUIDU2f[];
  // Characteristics
  // The following three characteristics are for kTestUUIDGenericAccess.
  static const char kTestUUIDDeviceName[];
  static const char kTestUUIDAppearance[];
  static const char kTestUUIDReconnectionAddress[];
  // This characteristic is for kTestUUIDHeartRate.
  static const char kTestUUIDHeartRateMeasurement[];
  // This characteristic is for kTestUUIDU2f.
  static const char kTestUUIDU2fControlPointLength[];
  // Descriptors
  static const char kTestUUIDCharacteristicUserDescription[];
  static const char kTestUUIDClientCharacteristicConfiguration[];
  static const char kTestUUIDServerCharacteristicConfiguration[];
  static const char kTestUUIDCharacteristicPresentationFormat[];
  static const char kTestUUIDCableAdvertisement[];
  // Manufacturer data
  static const uint16_t kTestManufacturerId;
  // Test ephemeral ID for BLE devices that support cloud-assisted BLE protocol.
  static const uint8_t kTestCableEid[];
  static const char kTestUuidFormattedClientEid[];

  BluetoothTestBase();
  ~BluetoothTestBase() override;

  // Checks that no unexpected calls have been made to callbacks.
  // Overrides of this method should always call the parent's class method.
  void TearDown() override;

  // Calls adapter_->StartDiscoverySessionWithFilter with Low Energy transport,
  // and this fixture's callbacks expecting success.
  // Then RunLoop().RunUntilIdle().
  virtual void StartLowEnergyDiscoverySession();

  // Calls adapter_->StartDiscoverySessionWithFilter with Low Energy transport,
  // and this fixture's callbacks expecting error.
  // Then RunLoop().RunUntilIdle().
  void StartLowEnergyDiscoverySessionExpectedToFail();

  // Check if Low Energy is available.
  virtual bool PlatformSupportsLowEnergy() = 0;

  // Initializes the BluetoothAdapter |adapter_| with the system adapter.
  virtual void InitWithDefaultAdapter() {}

  // Initializes the BluetoothAdapter |adapter_| with the system adapter forced
  // to be ignored as if it did not exist. This enables tests for when an
  // adapter is not present on the system.
  virtual void InitWithoutDefaultAdapter() {}

  // Initializes the BluetoothAdapter |adapter_| with a fake adapter that can be
  // controlled by this test fixture.
  virtual void InitWithFakeAdapter() {}

  // Similar to InitWithFakeAdapter(), but simulates a state where we fail to
  // get access to the underlying radio.
  virtual void InitFakeAdapterWithoutRadio() {}

  // Configures the fake adapter to lack the necessary permissions to scan for
  // devices.  Returns false if the current platform always has permission.
  virtual bool DenyPermission();

  // Simulates a failure during a pending BluetoothAdapter::SetPowered()
  // operation.
  virtual void SimulateAdapterPowerFailure() {}

  // Simulates the Adapter being switched on.
  virtual void SimulateAdapterPoweredOn() {}

  // Simulates the Adapter being switched off.
  virtual void SimulateAdapterPoweredOff() {}

  // Create a fake Low Energy device and discover it.
  // |device_ordinal| with the same device address stands for the same fake
  // device with different properties.
  // For example:
  // SimulateLowEnergyDevice(2); << First call will create a device with address
  // kTestDeviceAddress1
  // SimulateLowEnergyDevice(3); << Second call will update changes to the
  // device of address kTestDeviceAddress1.
  //
  // |device_ordinal| selects between multiple fake device data sets to produce:
  //   1: Name: kTestDeviceName
  //      Address:           kTestDeviceAddress1
  //      RSSI:              TestRSSI::LOWEST
  //      Flags:             0x04
  //      Advertised UUIDs: {kTestUUIDGenericAccess, kTestUUIDGenericAttribute}
  //      Service Data:     {kTestUUIDHeartRate: [1]}
  //      ManufacturerData: {kTestManufacturerId: [1, 2, 3, 4]}
  //      Tx Power:          TestTxPower::LOWEST
  //   2: Name: kTestDeviceName
  //      Address:           kTestDeviceAddress1
  //      RSSI:              TestRSSI::LOWER
  //      Flags:             0x05
  //      Advertised UUIDs: {kTestUUIDImmediateAlert, kTestUUIDLinkLoss}
  //      Service Data:     {kTestUUIDHeartRate: [],
  //                         kTestUUIDImmediateAlert: [0, 2]}
  //      ManufacturerData: {kTestManufacturerId: []}
  //      Tx Power:          TestTxPower::LOWER
  //   3: Name:    kTestDeviceNameEmpty
  //      Address: kTestDeviceAddress1
  //      RSSI:    TestRSSI::LOW
  //      No Flags
  //      No Advertised UUIDs
  //      No Service Data
  //      No Manufacturer Data
  //      No Tx Power
  //   4: Name:    kTestDeviceNameEmpty
  //      Address: kTestDeviceAddress2
  //      RSSI:    TestRSSI::MEDIUM
  //      No Flags
  //      No Advertised UUIDs
  //      No Service Data
  //      No Manufacturer Data
  //      No Tx Power
  //   5: No name device
  //      Address: kTestDeviceAddress1
  //      RSSI:    TestRSSI::HIGH
  //      Flags:   0x06
  //      No Advertised UUIDs
  //      No Service Data
  //      No Tx Power
  //   6: Name:    kTestDeviceName
  //      Address: kTestDeviceAddress2
  //      Flags:   0x18
  //      RSSI:    TestRSSI::LOWEST
  //      No Advertised UUIDs
  //      No Service Data
  //      No Manufacturer Data
  //      No Tx Power
  //      Supports BR/EDR and LE.
  //   7: Name:    kTestDeviceNameU2f
  //      Address: kTestDeviceAddress1
  //      Flags:   0x07
  //      RSSI:    TestRSSI::LOWEST
  //      Advertised UUIDs: {kTestUUIDU2f}
  //      Service Data:     {kTestUUIDU2fControlPointLength: [0, 20]}
  //      No Manufacturer Data
  //      No Tx Power
  //   8: Name: kTestDeviceNameCable;
  //      Address: kTestDeviceAddress1;
  //      Flags: 0x07;
  //      RSSI: static_cast<int>(TestRSSI::LOWEST);
  //      Advertised UUIDs: {BluetoothUUID(kTestUUIDU2f)};
  //      Service Data: {
  //                      {BluetoothUUID(kTestUUIDCableAdvertisement128),
  //                       std::vector<uint8_t>(std::begin(kTestCableEid),
  //                                            std::end(kTestCableEid))}};
  //   9: Name: kTestDeviceNameCable;
  //      Address: kTestDeviceAddress2;
  //      Flags: = 0x07;
  //      RSSI: static_cast<int>(TestRSSI::LOWEST);
  //      Advertised UUIDs: {
  //                          BluetoothUUID(kTestUUIDCableAdvertisement16),
  //                          BluetoothUUID(kTestUuidFormattedClientEid)};

  virtual BluetoothDevice* SimulateLowEnergyDevice(int device_ordinal);

  // Simulates a signal by the OS that an ongoing discovery aborted because of
  // some unexpected error.
  virtual void SimulateLowEnergyDiscoveryFailure();

  // Simulates a connected low energy device. Used before starting a low energy
  // discovey session.
  virtual void SimulateConnectedLowEnergyDevice(
      ConnectedDeviceType device_ordinal) {}

  // Create a fake classic device and discover it. The device will have
  // name kTestDeviceName, no advertised UUIDs and address kTestDeviceAddress3.
  virtual BluetoothDevice* SimulateClassicDevice();

  // Simulates a change in |device|'s pairing state.
  virtual void SimulateDevicePaired(BluetoothDevice* device, bool is_paired) {}

  // Sets |device|'s pairing code to |pin_code|.
  virtual void SimulatePairingPinCode(BluetoothDevice* device,
                                      std::string pin_code) {}

  // Simulates a successful registration of |advertisement|.
  virtual void SimulateAdvertisementStarted(
      BluetoothAdvertisement* advertisement) {}

  // Simulates a successful unregistration of |advertisement|.
  virtual void SimulateAdvertisementStopped(
      BluetoothAdvertisement* advertisement) {}

  // Simulates a failure of either registering or unregistering |advertisement|
  // with error code |error_code|.
  virtual void SimulateAdvertisementError(
      BluetoothAdvertisement* advertisement,
      BluetoothAdvertisement::ErrorCode error_code) {}

  // Remembers |device|'s platform specific object to be used in a
  // subsequent call to methods such as SimulateGattServicesDiscovered that
  // accept a nullptr value to select this remembered characteristic. This
  // enables tests where the platform attempts to reference device
  // objects after the Chrome objects have been deleted, e.g. with DeleteDevice.
  virtual void RememberDeviceForSubsequentAction(BluetoothDevice* device) {}

  // Performs a GATT connection to the given device and returns whether it was
  // successful. The |service_uuid| is passed to
  // |BluetoothDevice::CreateGattConnection|; see the documentation for it
  // there. The callback is called to complete the GATT connection. If not
  // given, |SimulateGattConnection| is called but the callback argument lets
  // one override that.
  bool ConnectGatt(
      BluetoothDevice* device,
      std::optional<BluetoothUUID> service_uuid = std::nullopt,
      std::optional<base::OnceCallback<void(BluetoothDevice*)>> = std::nullopt);

  // GetTargetGattService returns the specific GATT service, if any, that was
  // targeted for discovery, i.e. via the |service_uuid| argument to
  // |CreateGattConnection|.
  virtual std::optional<BluetoothUUID> GetTargetGattService(
      BluetoothDevice* device);

  // Simulates success of implementation details of CreateGattConnection.
  virtual void SimulateGattConnection(BluetoothDevice* device) {}

  // Simulates failure of CreateGattConnection with the given error code.
  virtual void SimulateGattConnectionError(BluetoothDevice* device,
                                           BluetoothDevice::ConnectErrorCode) {}

  // Simulates GattConnection disconnecting.
  virtual void SimulateGattDisconnection(BluetoothDevice* device) {}

  // Simulates Error in GattConnection disconnecting.
  virtual void SimulateGattDisconnectionError(BluetoothDevice* device) {}

  // Simulates an event where the OS breaks the Gatt connection. Defaults to
  // SimulateGattDisconnection(device).
  virtual void SimulateDeviceBreaksConnection(BluetoothDevice* device);

  // Simulates a device changing its name property while a GATT connection is
  // open.
  virtual void SimulateGattNameChange(BluetoothDevice* device,
                                      const std::string& new_name) {}

  // Simulates a connection status change to disconnect.
  virtual void SimulateStatusChangeToDisconnect(BluetoothDevice* device) {}

  // Simulates success of discovering services. |uuids| and |blocked_uuids| are
  // used to create a service for each UUID string. Multiple UUIDs with the same
  // value produce multiple service instances. UUIDs in the |blocked_uuids| list
  // create services which cannot be accessed (WinRT-only).
  virtual void SimulateGattServicesDiscovered(
      BluetoothDevice* device,
      const std::vector<std::string>& uuids,
      const std::vector<std::string>& blocked_uuids = {}) {}

  // Simulates a GATT Services changed event.
  virtual void SimulateGattServicesChanged(BluetoothDevice* device) {}

  // Simulates remove of a |service|.
  virtual void SimulateGattServiceRemoved(BluetoothRemoteGattService* service) {
  }

  // Simulates failure to discover services.
  virtual void SimulateGattServicesDiscoveryError(BluetoothDevice* device) {}

  // Simulates a Characteristic on a service.
  virtual void SimulateGattCharacteristic(BluetoothRemoteGattService* service,
                                          const std::string& uuid,
                                          int properties) {}

  // Simulates remove of a |characteristic| from |service|.
  virtual void SimulateGattCharacteristicRemoved(
      BluetoothRemoteGattService* service,
      BluetoothRemoteGattCharacteristic* characteristic) {}

  // Remembers |characteristic|'s platform specific object to be used in a
  // subsequent call to methods such as SimulateGattCharacteristicRead that
  // accept a nullptr value to select this remembered characteristic. This
  // enables tests where the platform attempts to reference characteristic
  // objects after the Chrome objects have been deleted, e.g. with DeleteDevice.
  virtual void RememberCharacteristicForSubsequentAction(
      BluetoothRemoteGattCharacteristic* characteristic) {}

  // Remembers |characteristic|'s Client Characteristic Configuration (CCC)
  // descriptor's platform specific object to be used in a subsequent call to
  // methods such as SimulateGattNotifySessionStarted. This enables tests where
  // the platform attempts to reference descriptor objects after the Chrome
  // objects have been deleted, e.g. with DeleteDevice.
  virtual void RememberCCCDescriptorForSubsequentAction(
      BluetoothRemoteGattCharacteristic* characteristic) {}

  // Simulates a Characteristic Set Notify success.
  // If |characteristic| is null, acts upon the characteristic & CCC
  // descriptor provided to RememberCharacteristicForSubsequentAction &
  // RememberCCCDescriptorForSubsequentAction.
  virtual void SimulateGattNotifySessionStarted(
      BluetoothRemoteGattCharacteristic* characteristic) {}

  // Simulates a Characteristic Set Notify error.
  // If |characteristic| is null, acts upon the characteristic & CCC
  // descriptor provided to RememberCharacteristicForSubsequentAction &
  // RememberCCCDescriptorForSubsequentAction.
  virtual void SimulateGattNotifySessionStartError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) {}

  // Simulates a Characteristic Stop Notify completed.
  // If |characteristic| is null, acts upon the characteristic & CCC
  // descriptor provided to RememberCharacteristicForSubsequentAction &
  // RememberCCCDescriptorForSubsequentAction.
  virtual void SimulateGattNotifySessionStopped(
      BluetoothRemoteGattCharacteristic* characteristic) {}

  // Simulates a Characteristic Stop Notify error.
  // If |characteristic| is null, acts upon the characteristic & CCC
  // descriptor provided to RememberCharacteristicForSubsequentAction &
  // RememberCCCDescriptorForSubsequentAction.
  virtual void SimulateGattNotifySessionStopError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode error_code) {}

  // Simulates a Characteristic Set Notify operation failing synchronously once
  // for an unknown reason.
  virtual void SimulateGattCharacteristicSetNotifyWillFailSynchronouslyOnce(
      BluetoothRemoteGattCharacteristic* characteristic) {}

  // Simulates a Characteristic Changed operation with updated |value|.
  virtual void SimulateGattCharacteristicChanged(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) {}

  // Simulates a Characteristic Read operation succeeding, returning |value|.
  // If |characteristic| is null, acts upon the characteristic provided to
  // RememberCharacteristicForSubsequentAction.
  virtual void SimulateGattCharacteristicRead(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) {}

  // Simulates a Characteristic Read operation failing with a GattErrorCode.
  virtual void SimulateGattCharacteristicReadError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode) {}

  // Simulates a Characteristic Read operation failing synchronously once for an
  // unknown reason.
  virtual void SimulateGattCharacteristicReadWillFailSynchronouslyOnce(
      BluetoothRemoteGattCharacteristic* characteristic) {}

  // Simulates a Characteristic Write operation succeeding, returning |value|.
  // If |characteristic| is null, acts upon the characteristic provided to
  // RememberCharacteristicForSubsequentAction.
  virtual void SimulateGattCharacteristicWrite(
      BluetoothRemoteGattCharacteristic* characteristic) {}

  // Simulates a Characteristic Write operation failing with a GattErrorCode.
  virtual void SimulateGattCharacteristicWriteError(
      BluetoothRemoteGattCharacteristic* characteristic,
      BluetoothGattService::GattErrorCode) {}

  // Simulates a Characteristic Write operation failing synchronously once for
  // an unknown reason.
  virtual void SimulateGattCharacteristicWriteWillFailSynchronouslyOnce(
      BluetoothRemoteGattCharacteristic* characteristic) {}

  // Simulates a Descriptor on a service.
  virtual void SimulateGattDescriptor(
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::string& uuid) {}

  // Simulates reading a value from a locally hosted GATT characteristic by a
  // remote central device. Returns the value that was read from the local
  // GATT characteristic in the value callback.
  virtual void SimulateLocalGattCharacteristicValueReadRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattCharacteristic* characteristic,
      BluetoothLocalGattService::Delegate::ValueCallback value_callback) {}

  // Simulates write a value to a locally hosted GATT characteristic by a
  // remote central device.
  virtual void SimulateLocalGattCharacteristicValueWriteRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value_to_write,
      base::OnceClosure success_callback,
      base::OnceClosure error_callback) {}

  // Simulates prepare write a value to a locally hosted GATT characteristic by
  // a remote central device.
  virtual void SimulateLocalGattCharacteristicValuePrepareWriteRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value_to_write,
      int offset,
      bool has_subsequent_write,
      base::OnceClosure success_callback,
      base::OnceClosure error_callback) {}

  // Simulates reading a value from a locally hosted GATT descriptor by a
  // remote central device. Returns the value that was read from the local
  // GATT descriptor in the value callback.
  virtual void SimulateLocalGattDescriptorValueReadRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattDescriptor* descriptor,
      BluetoothLocalGattService::Delegate::ValueCallback value_callback) {}

  // Simulates write a value to a locally hosted GATT descriptor by a
  // remote central device.
  virtual void SimulateLocalGattDescriptorValueWriteRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattDescriptor* descriptor,
      const std::vector<uint8_t>& value_to_write,
      base::OnceClosure success_callback,
      base::OnceClosure error_callback) {}

  // Simulates starting or stopping a notification session for a locally
  // hosted GATT characteristic by a remote device. Returns false if we were
  // not able to start or stop notifications.
  virtual bool SimulateLocalGattCharacteristicNotificationsRequest(
      BluetoothDevice* from_device,
      BluetoothLocalGattCharacteristic* characteristic,
      bool start);

  // Returns the value for the last notification that was sent on this
  // characteristic.
  virtual std::vector<uint8_t> LastNotifactionValueForCharacteristic(
      BluetoothLocalGattCharacteristic* characteristic);

  // Remembers |descriptor|'s platform specific object to be used in a
  // subsequent call to methods such as SimulateGattDescriptorRead that
  // accept a nullptr value to select this remembered descriptor. This
  // enables tests where the platform attempts to reference descriptor
  // objects after the Chrome objects have been deleted, e.g. with DeleteDevice.
  virtual void RememberDescriptorForSubsequentAction(
      BluetoothRemoteGattDescriptor* descriptor) {}

  // Simulates a Descriptor Read operation succeeding, returning |value|.
  // If |descriptor| is null, acts upon the descriptor provided to
  // RememberDescriptorForSubsequentAction.
  virtual void SimulateGattDescriptorRead(
      BluetoothRemoteGattDescriptor* descriptor,
      const std::vector<uint8_t>& value) {}

  // Simulates a Descriptor Read operation failing with a GattErrorCode.
  virtual void SimulateGattDescriptorReadError(
      BluetoothRemoteGattDescriptor* descriptor,
      BluetoothGattService::GattErrorCode) {}

  // Simulates a Descriptor Read operation failing synchronously once for an
  // unknown reason.
  virtual void SimulateGattDescriptorReadWillFailSynchronouslyOnce(
      BluetoothRemoteGattDescriptor* descriptor) {}

  // Simulates a Descriptor Write operation succeeding, returning |value|.
  // If |descriptor| is null, acts upon the descriptor provided to
  // RememberDescriptorForSubsequentAction.
  virtual void SimulateGattDescriptorWrite(
      BluetoothRemoteGattDescriptor* descriptor) {}

  // Simulates a Descriptor Write operation failing with a GattErrorCode.
  virtual void SimulateGattDescriptorWriteError(
      BluetoothRemoteGattDescriptor* descriptor,
      BluetoothGattService::GattErrorCode) {}

  // Simulates a Descriptor Update operation failing with a GattErrorCode.
  virtual void SimulateGattDescriptorUpdateError(
      BluetoothRemoteGattDescriptor* descriptor,
      BluetoothGattService::GattErrorCode) {}

  // Simulates a Descriptor Write operation failing synchronously once for
  // an unknown reason.
  virtual void SimulateGattDescriptorWriteWillFailSynchronouslyOnce(
      BluetoothRemoteGattDescriptor* descriptor) {}

  // Tests that functions to change the notify value have been called |attempts|
  // times.
  virtual void ExpectedChangeNotifyValueAttempts(int attempts);

  // Tests that the notify value is |expected_value_state|. The default
  // implementation checks that the correct value has been written to the CCC
  // Descriptor.
  virtual void ExpectedNotifyValue(NotifyValueState expected_value_state);

  // Returns a list of local GATT services registered with the adapter.
  virtual std::vector<BluetoothLocalGattService*> RegisteredGattServices();

  // Removes the device from the adapter and deletes it.
  virtual void DeleteDevice(BluetoothDevice* device);

  // Callbacks that increment |callback_count_|, |error_callback_count_|:
  void Callback(Call expected);
  void CreateAdvertisementCallback(Call expected,
                                   scoped_refptr<BluetoothAdvertisement>);
  void DiscoverySessionCallback(Call expected,
                                std::unique_ptr<BluetoothDiscoverySession>);
  void GattConnectionCallback(Call expected,
                              Result expected_result,
                              std::unique_ptr<BluetoothGattConnection>,
                              std::optional<BluetoothDevice::ConnectErrorCode>);
  void NotifyCallback(Call expected,
                      std::unique_ptr<BluetoothGattNotifySession>);
  void NotifyCheckForPrecedingCalls(
      int num_of_preceding_calls,
      std::unique_ptr<BluetoothGattNotifySession>);
  void StopNotifyCallback(Call expected);
  void StopNotifyCheckForPrecedingCalls(int num_of_preceding_calls);
  void ReadValueCallback(
      Call expected,
      Result expected_result,
      std::optional<BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);
  void ErrorCallback(Call expected);
  void AdvertisementErrorCallback(Call expected,
                                  BluetoothAdvertisement::ErrorCode error_code);
  void OnConnectCallback(Call expected,
                         Result expected_result,
                         std::optional<BluetoothDevice::ConnectErrorCode>);
  void GattErrorCallback(Call expected, BluetoothGattService::GattErrorCode);
  void ReentrantStartNotifySessionSuccessCallback(
      Call expected,
      BluetoothRemoteGattCharacteristic* characteristic,
      std::unique_ptr<BluetoothGattNotifySession> notify_session);
  void ReentrantStartNotifySessionErrorCallback(
      Call expected,
      BluetoothRemoteGattCharacteristic* characteristic,
      bool error_in_reentrant,
      BluetoothGattService::GattErrorCode error_code);

  // Accessors to get callbacks bound to this fixture:
  base::OnceClosure GetCallback(Call expected);
  BluetoothAdapter::CreateAdvertisementCallback GetCreateAdvertisementCallback(
      Call expected);
  BluetoothAdapter::DiscoverySessionCallback GetDiscoverySessionCallback(
      Call expected);
  BluetoothDevice::GattConnectionCallback GetGattConnectionCallback(
      Call expected,
      Result expected_result);
  BluetoothRemoteGattCharacteristic::NotifySessionCallback GetNotifyCallback(
      Call expected);
  BluetoothRemoteGattCharacteristic::NotifySessionCallback
  GetNotifyCheckForPrecedingCalls(int num_of_preceding_calls);
  base::OnceClosure GetStopNotifyCallback(Call expected);
  base::OnceClosure GetStopNotifyCheckForPrecedingCalls(
      int num_of_preceding_calls);
  BluetoothRemoteGattCharacteristic::ValueCallback GetReadValueCallback(
      Call expected,
      Result expected_result);
  BluetoothAdapter::ErrorCallback GetErrorCallback(Call expected);
  BluetoothAdapter::AdvertisementErrorCallback GetAdvertisementErrorCallback(
      Call expected);
  base::OnceCallback<void(BluetoothGattService::GattErrorCode)>
  GetGattErrorCallback(Call expected);
  BluetoothRemoteGattCharacteristic::NotifySessionCallback
  GetReentrantStartNotifySessionSuccessCallback(
      Call expected,
      BluetoothRemoteGattCharacteristic* characteristic);
  base::OnceCallback<void(BluetoothGattService::GattErrorCode)>
  GetReentrantStartNotifySessionErrorCallback(
      Call expected,
      BluetoothRemoteGattCharacteristic* characteristic,
      bool error_in_reentrant);

  // Reset all event count members to 0.
  virtual void ResetEventCounts();

  void RemoveTimedOutDevices();

 protected:
  // The expected/actual counts for tests.
  struct EventCounts {
    int unexpected = 0;
    int expected = 0;
    int actual = 0;
  };

  // Counts for an expected metric being tested.
  struct ResultCounts {
    EventCounts success;
    EventCounts failure;
  };

  // Utility method to simplify creading a low energy device of a given
  // |device_ordinal|.
  LowEnergyDeviceData GetLowEnergyDeviceData(int device_ordinal) const;

  // A TaskEnvironment is required by some implementations that will
  // PostTasks and by base::RunLoop().RunUntilIdle() use in this fixture.
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<BluetoothAdapter> adapter_;
  std::vector<scoped_refptr<BluetoothAdvertisement>> advertisements_;
  std::vector<std::unique_ptr<BluetoothDiscoverySession>> discovery_sessions_;
  std::vector<std::unique_ptr<BluetoothGattConnection>> gatt_connections_;
  BluetoothAdvertisement::ErrorCode last_advertisement_error_code_ =
      BluetoothAdvertisement::INVALID_ADVERTISEMENT_ERROR_CODE;
  enum BluetoothDevice::ConnectErrorCode last_connect_error_code_ =
      BluetoothDevice::ERROR_UNKNOWN;
  std::vector<std::unique_ptr<BluetoothGattNotifySession>> notify_sessions_;
  std::vector<uint8_t> last_read_value_;
  std::vector<uint8_t> last_write_value_;
  BluetoothGattService::GattErrorCode last_gatt_error_code_ =
      BluetoothGattService::GattErrorCode::kUnknown;

  int callback_count_ = 0;
  int error_callback_count_ = 0;
  int gatt_connection_attempts_ = 0;
  int gatt_disconnection_attempts_ = 0;
  int gatt_discovery_attempts_ = 0;
  int gatt_notify_characteristic_attempts_ = 0;
  int gatt_read_characteristic_attempts_ = 0;
  int gatt_write_characteristic_attempts_ = 0;
  int gatt_read_descriptor_attempts_ = 0;
  int gatt_write_descriptor_attempts_ = 0;

  // The following values are used to make sure the correct callbacks
  // have been called. They are not reset when calling ResetEventCounts().
  int expected_success_callback_calls_ = 0;
  int expected_error_callback_calls_ = 0;
  int actual_success_callback_calls_ = 0;
  int actual_error_callback_calls_ = 0;
  bool unexpected_success_callback_ = false;
  bool unexpected_error_callback_ = false;

  EventCounts read_callback_calls_;
  ResultCounts read_results_;

  base::WeakPtrFactory<BluetoothTestBase> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_TEST_BLUETOOTH_TEST_H_
