// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluez_dbus_client.h"

namespace dbus {
class ObjectProxy;
}

namespace bluez {

class BluetoothServiceRecordBlueZ;

// BluetoothAdapterClient is used to communicate with objects representing
// local Bluetooth Adapters.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterClient : public BluezDBusClient {
 public:
  enum AddressType {
    kPublic,
    kRandom,
  };

  // A DiscoveryFilter represents a filter passed to the SetDiscoveryFilter
  // method.
  struct DiscoveryFilter {
    DiscoveryFilter();

    DiscoveryFilter(const DiscoveryFilter&) = delete;
    DiscoveryFilter& operator=(const DiscoveryFilter&) = delete;

    ~DiscoveryFilter();

    // Copy content of |filter| into this filter
    void CopyFrom(const DiscoveryFilter& filter);

    std::unique_ptr<std::vector<std::string>> uuids;
    std::unique_ptr<int16_t> rssi;
    std::unique_ptr<uint16_t> pathloss;
    std::unique_ptr<std::string> transport;
  };

  // Represent an error sent through DBus.
  struct Error {
    Error(const std::string& name, const std::string& message);

    std::string name;
    std::string message;
  };

  // Structure of properties associated with bluetooth adapters.
  struct Properties : public dbus::PropertySet {
    // The Bluetooth device address of the adapter. Read-only.
    dbus::Property<std::string> address;

    // The Bluetooth system name, generally derived from the hostname.
    dbus::Property<std::string> name;

    // The Bluetooth friendly name of the adapter, unlike remote devices,
    // this property can be changed to change the presentation for when
    // the adapter is discoverable.
    dbus::Property<std::string> alias;

    // The Bluetooth class of the adapter device. Read-only.
    dbus::Property<uint32_t> bluetooth_class;

    // Whether the adapter radio is powered.
    dbus::Property<bool> powered;

    // Whether the adapter is discoverable by other Bluetooth devices.
    // |discovering_timeout| is used to automatically disable after a time
    // period.
    dbus::Property<bool> discoverable;

    // Whether the adapter accepts incoming pairing requests from other
    // Bluetooth devices. |pairable_timeout| is used to automatically disable
    // after a time period.
    dbus::Property<bool> pairable;

    // The timeout in seconds to cease accepting incoming pairing requests
    // after |pairable| is set to true. Zero means adapter remains pairable
    // forever.
    dbus::Property<uint32_t> pairable_timeout;

    // The timeout in seconds to cease the adapter being discoverable by
    // other Bluetooth devices after |discoverable| is set to true. Zero
    // means adapter remains discoverable forever.
    dbus::Property<uint32_t> discoverable_timeout;

    // Indicates that the adapter is discovering other Bluetooth Devices.
    // Read-only. Use StartDiscovery() to begin discovery.
    dbus::Property<bool> discovering;

    // List of 128-bit UUIDs that represent the available local services.
    // Read-only.
    dbus::Property<std::vector<std::string>> uuids;

    // Local Device ID information in Linux kernel modalias format. Read-only.
    dbus::Property<std::string> modalias;

    // List of roles supported by the adapter. Read-only.
    dbus::Property<std::vector<std::string>> roles;

    Properties(dbus::ObjectProxy* object_proxy,
               const std::string& interface_name,
               const PropertyChangedCallback& callback);
    ~Properties() override;
  };

  // Interface for observing changes from a local bluetooth adapter.
  class Observer {
   public:
    virtual ~Observer() {}

    // Called when the adapter with object path |object_path| is added to the
    // system.
    virtual void AdapterAdded(const dbus::ObjectPath& object_path) {}

    // Called when the adapter with object path |object_path| is removed from
    // the system.
    virtual void AdapterRemoved(const dbus::ObjectPath& object_path) {}

    // Called when the adapter with object path |object_path| has a
    // change in value of the property named |property_name|.
    virtual void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                                        const std::string& property_name) {}
  };

  BluetoothAdapterClient(const BluetoothAdapterClient&) = delete;
  BluetoothAdapterClient& operator=(const BluetoothAdapterClient&) = delete;

  ~BluetoothAdapterClient() override;

  // Adds and removes observers for events on all local bluetooth
  // adapters. Check the |object_path| parameter of observer methods to
  // determine which adapter is issuing the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Returns the list of adapter object paths known to the system.
  virtual std::vector<dbus::ObjectPath> GetAdapters() = 0;

  // Obtain the properties for the adapter with object path |object_path|,
  // any values should be copied if needed.
  virtual Properties* GetProperties(const dbus::ObjectPath& object_path) = 0;

  // Callback used to send back the handle of a created service record.
  using ServiceRecordCallback = base::OnceCallback<void(uint32_t)>;

  // Callback used to send back the device resulting from ConnectDevice().
  using ConnectDeviceCallback =
      base::OnceCallback<void(const dbus::ObjectPath& device_path)>;

  // The ErrorCallback is used by adapter methods to indicate failure.
  // It receives two arguments: the name of the error in |error_name| and
  // an optional message in |error_message|.
  using ErrorCallback =
      base::OnceCallback<void(const std::string& error_name,
                              const std::string& error_message)>;

  // Callback used by adapter methods to indicate that a response was
  // received with an optional Error in case an error occurred.
  using ResponseCallback =
      base::OnceCallback<void(const std::optional<Error>&)>;

  // Starts a device discovery on the adapter with object path |object_path|.
  virtual void StartDiscovery(const dbus::ObjectPath& object_path,
                              ResponseCallback callback) = 0;
  // DEPRECATED: Use StartDiscovery() above.
  void StartDiscovery(const dbus::ObjectPath& object_path,
                      base::OnceClosure callback,
                      ErrorCallback error_callback);

  // Cancels any previous device discovery on the adapter with object path
  // |object_path|.
  virtual void StopDiscovery(const dbus::ObjectPath& object_path,
                             ResponseCallback callback) = 0;
  // DEPRECATED: Use StopDiscovery() above.
  void StopDiscovery(const dbus::ObjectPath& object_path,
                     base::OnceClosure callback,
                     ErrorCallback error_callback);

  // Removes from the adapter with object path |object_path| the remote
  // device with object path |object_path| from the list of known devices
  // and discards any pairing information.
  virtual void RemoveDevice(const dbus::ObjectPath& object_path,
                            const dbus::ObjectPath& device_path,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) = 0;

  // Sets the device discovery filter on the adapter with object path
  // |object_path|. When this method is called with no filter parameter, filter
  // is removed.
  // SetDiscoveryFilter can be called before StartDiscovery. It is useful when
  // client will create first discovery session, to ensure that proper scan
  // will be started right after call to StartDiscovery.
  virtual void SetDiscoveryFilter(const dbus::ObjectPath& object_path,
                                  const DiscoveryFilter& discovery_filter,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) = 0;

  // Creates the service record |record| on the adapter with the object path
  // |object_path|.
  virtual void CreateServiceRecord(const dbus::ObjectPath& object_path,
                                   const BluetoothServiceRecordBlueZ& record,
                                   ServiceRecordCallback callback,
                                   ErrorCallback error_callback) = 0;

  // Removes the service record with the uuid |uuid| on the adapter with the
  // object path |object_path|.
  virtual void RemoveServiceRecord(const dbus::ObjectPath& object_path,
                                   uint32_t handle,
                                   base::OnceClosure callback,
                                   ErrorCallback error_callback) = 0;

  // Connects to specified device, even if the device has not been discovered,
  // on the adapter with the object path |object_path|. Not providing an
  // |address_type| will create a BR/EDR device.
  virtual void ConnectDevice(const dbus::ObjectPath& object_path,
                             const std::string& address,
                             const std::optional<AddressType>& address_type,
                             ConnectDeviceCallback callback,
                             ErrorCallback error_callback) = 0;

  // Creates the instance.
  static BluetoothAdapterClient* Create();

  // Constants used to indicate exceptional error conditions.
  static const char kNoResponseError[];
  static const char kUnknownAdapterError[];

 protected:
  BluetoothAdapterClient();
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADAPTER_CLIENT_H_
