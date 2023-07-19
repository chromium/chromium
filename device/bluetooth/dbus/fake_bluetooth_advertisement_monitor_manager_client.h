// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_advertisement_monitor_application_service_provider.h"

namespace bluez {

// The BluetoothAdvertisementMonitorManagerClient simulates the behavior of the
// Bluetooth daemon's Advertisement Monitor Manager object and is used in
// test case.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothAdvertisementMonitorManagerClient
    final : public BluetoothAdvertisementMonitorManagerClient {
 public:
  FakeBluetoothAdvertisementMonitorManagerClient();
  ~FakeBluetoothAdvertisementMonitorManagerClient() override;
  FakeBluetoothAdvertisementMonitorManagerClient(
      const FakeBluetoothAdvertisementMonitorManagerClient&) = delete;
  FakeBluetoothAdvertisementMonitorManagerClient& operator=(
      const FakeBluetoothAdvertisementMonitorManagerClient&) = delete;

  // DBusClient override:
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;

  // BluetoothAdvertisementMonitorManagerClient override.
  void RegisterMonitor(const dbus::ObjectPath& application,
                       const dbus::ObjectPath& adapter,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override;
  void UnregisterMonitor(const dbus::ObjectPath& application,
                         const dbus::ObjectPath& adapter,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override;
  Properties* GetProperties(const dbus::ObjectPath& object_path) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  void RegisterApplicationServiceProvider(
      FakeBluetoothAdvertisementMonitorApplicationServiceProvider* provider);

  // This allows tests to control whether GetProperties() will return nullptr or
  // a valid object.
  void InitializeProperties();
  void RemoveProperties();

  FakeBluetoothAdvertisementMonitorApplicationServiceProvider*
  application_provider() {
    return application_provider_;
  }

 private:
  // Property callback passed when we create Properties structures.
  void OnPropertyChanged(const dbus::ObjectPath& object_path,
                         const std::string& property_name);

  raw_ptr<FakeBluetoothAdvertisementMonitorApplicationServiceProvider,
          AcrossTasksDanglingUntriaged>
      application_provider_ = nullptr;
  std::unique_ptr<Properties> properties_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<FakeBluetoothAdvertisementMonitorManagerClient>
      weak_ptr_factory_{this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_ADVERTISEMENT_MONITOR_MANAGER_CLIENT_H_
