// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_low_energy_defs_win.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {
namespace win {

//
// Callback function signature for Bluetooth GATT events. This fixes a bug in
// this typedef in the Windows 10.0.10586 SDK which is missing the CALLBACK
// modifier. This corrected typedef should be used throughout Chromium except
// when casting to the 'official' definition when calling Microsoft functions.
// This allows Chromium to build with 10.0.14393 or later SDKs (which have the
// fixed typedef) while doing the correct thing even when built with 10.0.10586.
// The CALLBACK modifier affects how function parameters are cleaned up from the
// stack and having a mismatch can lead to misalignment of the stack pointer.
//
typedef VOID(CALLBACK* PFNBLUETOOTH_GATT_EVENT_CALLBACK_CORRECTED)(
    _In_ BTH_LE_GATT_EVENT_TYPE EventType,
    _In_ PVOID EventOutParameter,
    _In_opt_ PVOID Context);

// Represents a device registry property value
class DEVICE_BLUETOOTH_EXPORT DeviceRegistryPropertyValue {
 public:
  // Creates a property value instance, where |property_type| is one of REG_xxx
  // registry value type (e.g. REG_SZ, REG_DWORD), |value| is a byte array
  // containing the property value and |value_size| is the number of bytes in
  // |value|. Note the returned instance takes ownership of the bytes in
  // |value|.
  static std::unique_ptr<DeviceRegistryPropertyValue> Create(
      DWORD property_type,
      std::unique_ptr<uint8_t[]> value,
      size_t value_size);

  DeviceRegistryPropertyValue(const DeviceRegistryPropertyValue&) = delete;
  DeviceRegistryPropertyValue& operator=(const DeviceRegistryPropertyValue&) =
      delete;

  ~DeviceRegistryPropertyValue();

  // Returns the vaue type a REG_xxx value (e.g. REG_SZ, REG_DWORD, ...)
  DWORD property_type() const { return property_type_; }

  std::string AsString() const;
  DWORD AsDWORD() const;

 private:
  DeviceRegistryPropertyValue(DWORD property_type,
                              std::unique_ptr<uint8_t[]> value);

  DWORD property_type_;
  std::unique_ptr<uint8_t[]> value_;
};

// Represents the value associated to a DEVPROPKEY.
class DEVICE_BLUETOOTH_EXPORT DevicePropertyValue {
 public:
  // Creates a property value instance, where |property_type| is one of
  // DEVPROP_TYPE_xxx value type , |value| is a byte array containing the
  // property value and |value_size| is the number of bytes in |value|. Note the
  // returned instance takes ownership of the bytes in |value|.
  DevicePropertyValue(DEVPROPTYPE property_type,
                      std::unique_ptr<uint8_t[]> value,
                      size_t value_size);

  DevicePropertyValue(const DevicePropertyValue&) = delete;
  DevicePropertyValue& operator=(const DevicePropertyValue&) = delete;

  ~DevicePropertyValue();

  DEVPROPTYPE property_type() const { return property_type_; }

  uint32_t AsUint32() const;

 private:
  DEVPROPTYPE property_type_;
  std::unique_ptr<uint8_t[]> value_;
  size_t value_size_;
};

struct DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyServiceInfo {
  BluetoothLowEnergyServiceInfo();
  ~BluetoothLowEnergyServiceInfo();

  BTH_LE_UUID uuid;
  // Attribute handle uniquely identifies this service on the device.
  USHORT attribute_handle = 0;
};

struct DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyDeviceInfo {
  BluetoothLowEnergyDeviceInfo();
  ~BluetoothLowEnergyDeviceInfo();

  base::FilePath path;
  std::string id;
  absl::optional<std::string> friendly_name;
  BLUETOOTH_ADDRESS address;
  bool visible;
  bool authenticated;
  bool connected;
};

bool DEVICE_BLUETOOTH_EXPORT
ExtractBluetoothAddressFromDeviceInstanceIdForTesting(
    const std::string& instance_id,
    BLUETOOTH_ADDRESS* btha,
    std::string* error);

// Wraps Windows APIs used to access Bluetooth Low Energy devices, providing an
// interface that can be replaced with fakes in tests.
class DEVICE_BLUETOOTH_EXPORT BluetoothLowEnergyWrapper {
 public:
  BluetoothLowEnergyWrapper();
  virtual ~BluetoothLowEnergyWrapper();

  // Returns true only on Windows platforms supporting Bluetooth Low Energy.
  virtual bool IsBluetoothLowEnergySupported();

  // Enumerates the list of known (i.e. already paired) Bluetooth LE devices on
  // this machine. In case of error, returns false and sets |error| with an
  // error message describing the problem.
  // Note: This function returns an error if Bluetooth Low Energy is not
  // supported on this Windows platform.
  virtual bool EnumerateKnownBluetoothLowEnergyDevices(
      std::vector<std::unique_ptr<BluetoothLowEnergyDeviceInfo>>* devices,
      std::string* error);

  // Enumerates the list of known Bluetooth LE GATT service devices on this
  // machine (a Bluetooth LE device usually has more than one GATT
  // services that each of them has a device interface on the machine). In case
  // of error, returns false and sets |error| with an error message describing
  // the problem.
  // Note: This function returns an error if Bluetooth Low Energy is not
  // supported on this Windows platform.
  virtual bool EnumerateKnownBluetoothLowEnergyGattServiceDevices(
      std::vector<std::unique_ptr<BluetoothLowEnergyDeviceInfo>>* devices,
      std::string* error);

  // Enumerates the list of known (i.e. cached) GATT services for a given
  // Bluetooth LE device |device_path| into |services|. In case of error,
  // returns false and sets |error| with an error message describing the
  // problem.
  // Note: This function returns an error if Bluetooth Low Energy is not
  // supported on this Windows platform.
  virtual bool EnumerateKnownBluetoothLowEnergyServices(
      const base::FilePath& device_path,
      std::vector<std::unique_ptr<BluetoothLowEnergyServiceInfo>>* services,
      std::string* error);

  // Reads characteristics of |service| with service device path |service_path|.
  // The result will be stored in |*out_included_characteristics| and
  // |*out_counts|.
  virtual HRESULT ReadCharacteristicsOfAService(
      base::FilePath& service_path,
      const PBTH_LE_GATT_SERVICE service,
      std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC>* out_included_characteristics,
      USHORT* out_counts);

  // Reads included descriptors of |characteristic| in service with service
  // device path |service_path|. The result will be stored in
  // |*out_included_descriptors| and |*out_counts|.
  virtual HRESULT ReadDescriptorsOfACharacteristic(
      base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      std::unique_ptr<BTH_LE_GATT_DESCRIPTOR>* out_included_descriptors,
      USHORT* out_counts);

  // Reads |characteristic| value in service with service device path
  // |service_path|. The result will be stored in |*out_value|.
  virtual HRESULT ReadCharacteristicValue(
      base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      std::unique_ptr<BTH_LE_GATT_CHARACTERISTIC_VALUE>* out_value);

  // Writes |characteristic| value in service with service device path
  // |service_path| to |*new_value|.
  virtual HRESULT WriteCharacteristicValue(
      base::FilePath& service_path,
      const PBTH_LE_GATT_CHARACTERISTIC characteristic,
      PBTH_LE_GATT_CHARACTERISTIC_VALUE new_value,
      ULONG flags);

  // Register GATT events of |event_type| in the service with service device
  // path |service_path|. |event_parameter| is the event's parameter. |callback|
  // is the function to be invoked if the event happened. |context| is the input
  // parameter to be given back through |callback|. |*out_handle| stores the
  // unique handle in OS for this registration.
  virtual HRESULT RegisterGattEvents(
      base::FilePath& service_path,
      BTH_LE_GATT_EVENT_TYPE event_type,
      PVOID event_parameter,
      PFNBLUETOOTH_GATT_EVENT_CALLBACK_CORRECTED callback,
      PVOID context,
      BLUETOOTH_GATT_EVENT_HANDLE* out_handle);
  virtual HRESULT UnregisterGattEvent(BLUETOOTH_GATT_EVENT_HANDLE event_handle);

  // Writes |descriptor| value in service with service device path
  // |service_path| to |*new_value|.
  virtual HRESULT WriteDescriptorValue(base::FilePath& service_path,
                                       const PBTH_LE_GATT_DESCRIPTOR descriptor,
                                       PBTH_LE_GATT_DESCRIPTOR_VALUE new_value);
};

}  // namespace win
}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_LOW_ENERGY_WIN_H_
