// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_INPUT_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_INPUT_CLIENT_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_input_client.h"

namespace bluez {

// FakeBluetoothInputClient simulates the behavior of the Bluetooth Daemon
// input device objects and is used both in test cases in place of a mock and on
// the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothInputClient
    : public BluetoothInputClient {
 public:
  struct Properties : public BluetoothInputClient::Properties {
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet override
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  FakeBluetoothInputClient();

  FakeBluetoothInputClient(const FakeBluetoothInputClient&) = delete;
  FakeBluetoothInputClient& operator=(const FakeBluetoothInputClient&) = delete;

  ~FakeBluetoothInputClient() override;

  // BluetoothInputClient overrides
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  Properties* GetProperties(const dbus::ObjectPath& object_path) override;

  // Simulate device addition/removal
  void AddInputDevice(const dbus::ObjectPath& object_path);
  void RemoveInputDevice(const dbus::ObjectPath& object_path);

 private:
  // Property callback passed when we create Properties* structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  // Static properties we return.
  std::map<const dbus::ObjectPath, std::unique_ptr<Properties>> properties_map_;

  // List of observers interested in event notifications from us.
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_INPUT_CLIENT_H_
