// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_DEVICE_CLIENT_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluez/bluetooth_service_record_bluez.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace bluez {

// BluetoothDeviceClient is used to communicate with objects representing
// remote Bluetooth Devices.
class DEVICE_BLUETOOTH_EXPORT BluetoothDeviceClient : public BluezDBusClient {
 public:
  using ServiceRecordList = std::vector<BluetoothServiceRecordBlueZ>;

  // This callback invoked for a successful GetConnInfo API call with the
  // RSSI, TX power, and maximum TX power of the current connection.
  using ConnInfoCallback = base::OnceCallback<
      void(int16_t rssi, int16_t transmit_power, int16_t max_transmit_power)>;

  // This callback invoked for a successful GetServiceRecords API call with the
  // currently discovered service records.
  using ServiceRecordsCallback =
      base::OnceCallback<void(const ServiceRecordList&)>;

  // The ErrorCallback is used by device methods to indicate failure.
  // It receives two arguments: the name of the error in |error_name| and
  // an optional message in |error_message|.
  using ErrorCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;

  // Structure of properties associated with bluetooth devices.
  struct Properties : public dbus::PropertySet {
    // The Bluetooth device address of the device. Read-only.
    dbus::Property<std::string> address;

    // The Bluetooth address type of the device. Read-only.
    dbus::Property<std::string> address_type;

    // The Bluetooth friendly name of the device. Read-only, to give a
    // different local name, use the |alias| property.
    dbus::Property<std::string> name;

    // Proposed icon name for the device according to the freedesktop.org
    // icon naming specification. Read-only.
    dbus::Property<std::string> icon;

    // The Bluetooth class of the device. Read-only.
    dbus::Property<uint32_t> bluetooth_class;

    // The transport type of the device. Read-only.
    dbus::Property<std::string> type;

    // The GAP external appearance of the device. Read-only.
    dbus::Property<uint16_t> appearance;

    // Unique numeric identifier for the vendor of the device. Read-only.
    dbus::Property<uint16_t> vendor;

    // List of 128-bit UUIDs that represent the available remote services.
    // Read-only.
    dbus::Property<std::vector<std::string>> uuids;

    // Transmitted power level. This field is avaliable only for LE devices
    // that include this field in AD. Read-only.
    dbus::Property<int16_t> tx_power;

    // Indicates that the device is currently paired. Read-only.
    dbus::Property<bool> paired;

    // Indicates that the device is currently bonded. Read-only.
    dbus::Property<bool> bonded;

    // Indicates that the device is currently connected via any transports.
    // Read-only.
    dbus::Property<bool> connected;

    // Indicates that the device is currently connected via BLE. Read-only.
    dbus::Property<bool> connected_le;

    // Whether the device is trusted, and connections should be always
    // accepted and attempted when the device is visible.
    dbus::Property<bool> trusted;

    // Whether the device is blocked, connections will be always rejected
    // and the device will not be visible.
    dbus::Property<bool> blocked;

    // Local alias for the device, if not set, is equal to |name|.
    dbus::Property<std::string> alias;

    // Object path of the adapter the device belongs to. Read-only.
    dbus::Property<dbus::ObjectPath> adapter;

    // Indicates whether the device is likely to only support pre-2.1
    // PIN Code pairing rather than 2.1 Secure Simple Pairing, this can
    // give false positives. Read-only.
    dbus::Property<bool> legacy_pairing;

    // Remote Device ID information in Linux kernel modalias format. Read-only.
    dbus::Property<std::string> modalias;

    // Received signal strength indicator that is set when the device is
    // discovered during inquiry. Read-only.
    dbus::Property<int16_t> rssi;

    // Manufacturer specific advertisement data. Keys are 16 bits Manufacturer
    // ID followed by its byte array value. Read-only.
    dbus::Property<std::map<uint16_t, std::vector<uint8_t>>> manufacturer_data;

    // Service advertisement data. Keys are the UUIDs in string format followed
    // by its byte array value. Read-only.
    dbus::Property<std::map<std::string, std::vector<uint8_t>>> service_data;

    // Indicate whether or not service discovery has been resolved. Read-only.
    dbus::Property<bool> services_resolved;

    // The Advertising Data Flags of the remote device. Read-only.
    dbus::Property<std::vector<uint8_t>> advertising_data_flags;

    // The MTU used in ATT communication with the remote device. Read-only.
    dbus::Property<uint16_t> mtu;

    // The EIR advertised by the remote device. Read-only.
    dbus::Property<std::vector<uint8_t>> eir;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;
  };

  // Connection parameters that can be passed to SetLEConnectionParameters().
  struct ConnectionParameters {
    // The lower bound to request for the connection interval.
    // In units of 1.25ms.
    uint16_t min_connection_interval;

    // The upper bound to request for the connection interval.
    // In units of 1.25ms.
    uint16_t max_connection_interval;
  };

  // Interface for observing changes from a remote bluetooth device.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the remote device with object path |object_path| is added
    // to the set of known devices.
    virtual void DeviceAdded(const dbus::ObjectPath& object_path) {}

    // Called when the remote device with object path |object_path| is removed
    // from the set of known devices.
    virtual void DeviceRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the device with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                                       const std::string& property_name) {}
  };

  BluetoothDeviceClient(const BluetoothDeviceClient&) = delete;
  BluetoothDeviceClient& operator=(const BluetoothDeviceClient&) = delete;

  ~BluetoothDeviceClient() override;

  // Adds and removes observers for events on all remote bluetooth
  // devices. Check the |object_path| parameter of observer methods to
  // determine which device is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of device object paths associated with the given adapter
  // identified by the D-Bus object path |adapter_path|.
  virtual std::vector<dbus::ObjectPath> GetDevicesForAdapter(
      const dbus::ObjectPath& adapter_path) = 0;

  // Obtain the properties for the device with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Connects to the device with object path |object_path|, connecting any
  // profiles that can be connected to and have been flagged as auto-connected;
  // may be used to connect additional profiles for an already connected device,
  // and succeeds if at least one profile is connected.
  virtual void Connect(const dbus::ObjectPath& object_path,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) = 0;

  // Connects to the device with object path |object_path| via classic
  // Bluetooth, connecting any profiles that can be connected to and have been
  // flagged as auto-connected; may be used to connect additional profiles for
  // an already connected device, and succeeds if at least one profile is
  // connected.
  virtual void ConnectClassic(const dbus::ObjectPath& object_path,
                              base::OnceClosure callback,
                              ErrorCallback error_callback) = 0;

  // Connects to the device with object path |object_path| via BLE,
  // connecting any profiles that can be connected to and have been flagged as
  // auto-connected; may be used to connect additional profiles for an already
  // connected device, and succeeds if at least one profile is connected.
  virtual void ConnectLE(const dbus::ObjectPath& object_path,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) = 0;

  // Disconnects all connections to the device with object path |object_path|,
  // terminating the low-level ACL connection and any profiles using it.
  virtual void Disconnect(const dbus::ObjectPath& object_path,
                          base::OnceClosure callback,
                          ErrorCallback error_callback) = 0;

  // Disconnects the BLE connection to the device with object path
  // |object_path|, terminating the low-level ACL connection and any profiles
  // using it.
  virtual void DisconnectLE(const dbus::ObjectPath& object_path,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) = 0;

  // Connects to the profile |uuid| on the device with object path
  // |object_path|, provided that the profile has been registered with a
  // handler on the local device.
  virtual void ConnectProfile(const dbus::ObjectPath& object_path,
                              const std::string& uuid,
                              base::OnceClosure callback,
                              ErrorCallback error_callback) = 0;

  // Disconnects from the profile |uuid| on the device with object path
  // |object_path|.
  virtual void DisconnectProfile(const dbus::ObjectPath& object_path,
                                 const std::string& uuid,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) = 0;

  // Initiates pairing with the device with object path |object_path| and
  // retrieves all SDP records or GATT primary services. An agent must be
  // registered to handle the pairing request.
  virtual void Pair(const dbus::ObjectPath& object_path,
                    base::OnceClosure callback,
                    ErrorCallback error_callback) = 0;

  // Cancels an in-progress pairing with the device with object path
  // |object_path| initiated by Pair().
  virtual void CancelPairing(const dbus::ObjectPath& object_path,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) = 0;

  // Returns the RSSI, TX power, and maximum TX power of a connection to the
  // device with object path |object_path|. If the device is not connected, then
  // an error will be returned.
  virtual void GetConnInfo(const dbus::ObjectPath& object_path,
                           ConnInfoCallback callback,
                           ErrorCallback error_callback) = 0;

  // Sets the connection parameters (e.g. connection interval) for the device.
  virtual void SetLEConnectionParameters(
      const dbus::ObjectPath& object_path,
      const ConnectionParameters& conn_params,
      base::OnceClosure callback,
      ErrorCallback error_callback) = 0;

  // Returns the currently discovered service records for the device with
  // object path |object_path|. If the device is not connected, then an error
  // will be returned.
  virtual void GetServiceRecords(const dbus::ObjectPath& object_path,
                                 ServiceRecordsCallback callback,
                                 ErrorCallback error_callback) = 0;

  // Executes all the privous prepare writes in a reliable write session.
  virtual void ExecuteWrite(const dbus::ObjectPath& object_path,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) = 0;

  // Aborts all the privous prepare writes in a reliable write session.
  virtual void AbortWrite(const dbus::ObjectPath& object_path,
                          base::OnceClosure callback,
                          ErrorCallback error_callback) = 0;

  // Creates the instance.
  static BluetoothDeviceClient* Create();

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];
  static const char kUnknownDeviceError[];

  // Strings used by BlueZ for the transport type of the remote device.
  // See https://chromium-review.googlesource.com/c/351512/
  static const char kTypeBredr[];
  static const char kTypeLe[];
  static const char kTypeDual[];

 protected:
  BluetoothDeviceClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_DEVICE_CLIENT_H_
