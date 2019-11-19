// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_client.h"

namespace bluez {

// FakeBluetoothGattDescriptorClient simulates the behavior of the Bluetooth
// Daemon GATT characteristic descriptor objects and is used in test cases in
// place of a mock and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothGattDescriptorClient
    : public BluetoothGattDescriptorClient {
 public:
  struct Properties : public BluetoothGattDescriptorClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet override
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  FakeBluetoothGattDescriptorClient();
  ~FakeBluetoothGattDescriptorClient() override;

  // DBusClient override.
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;

  // BluetoothGattDescriptorClient overrides.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::vector<dbus::ObjectPath> GetDescriptors() override;
  Properties* GetProperties(const dbus::ObjectPath& object_path) override;
  void ReadValue(const dbus::ObjectPath& object_path,
                 ValueCallback callback,
                 ErrorCallback error_callback) override;
  void WriteValue(const dbus::ObjectPath& object_path,
                  const std::vector<uint8_t>& value,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;

  // Makes the descriptor with the UUID |uuid| visible under the characteristic
  // with object path |characteristic_path|. Descriptor object paths are
  // hierarchical to their characteristics. |uuid| must belong to a descriptor
  // for which there is a constant defined below, otherwise this method has no
  // effect. Returns the object path of the created descriptor. In the no-op
  // case, returns an invalid path.
  dbus::ObjectPath ExposeDescriptor(const dbus::ObjectPath& characteristic_path,
                                    const std::string& uuid);
  void HideDescriptor(const dbus::ObjectPath& descriptor_path);

  // Object path components and UUIDs of GATT characteristic descriptors.
  static const char kClientCharacteristicConfigurationPathComponent[];
  static const char kClientCharacteristicConfigurationUUID[];

 private:
  // Property callback passed when we create Properties structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // Notifies observers.
  void NotifyDescriptorAdded(const dbus::ObjectPath& object_path);
  void NotifyDescriptorRemoved(const dbus::ObjectPath& object_path);

  // Mapping from object paths to Properties structures.
  struct DescriptorData {
    DescriptorData();
    ~DescriptorData();

    std::unique_ptr<Properties> properties;
  };
  typedef std::map<dbus::ObjectPath, DescriptorData*> PropertiesMap;
  PropertiesMap properties_;

  // List of observers interested in event notifications from us.
  base::ObserverList<Observer>::Unchecked observers_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeBluetoothGattDescriptorClient> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothGattDescriptorClient);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_CLIENT_H_
