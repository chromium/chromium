// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_MANAGER_CLIENT_H_

#include <map>
#include <set>
#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "dbus/bus.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_gatt_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_application_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_service_provider.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_service_provider.h"

namespace bluez {

class FakeBluetoothGattApplicationServiceProvider;
class FakeBluetoothGattCharacteristicServiceProvider;
class FakeBluetoothGattDescriptorServiceProvider;
class FakeBluetoothGattServiceServiceProvider;

// FakeBluetoothGattManagerClient simulates the behavior of the Bluetooth
// daemon's GATT manager object and is used both in test cases in place of a
// mock and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothGattManagerClient
    : public BluetoothGattManagerClient {
 public:
  FakeBluetoothGattManagerClient();

  FakeBluetoothGattManagerClient(const FakeBluetoothGattManagerClient&) =
      delete;
  FakeBluetoothGattManagerClient& operator=(
      const FakeBluetoothGattManagerClient&) = delete;

  ~FakeBluetoothGattManagerClient() override;

  // DBusClient override.
  void Init(dbus::Bus* bus, const std::string& bluetooth_service_name) override;

  // BluetoothGattManagerClient overrides.
  void RegisterApplication(const dbus::ObjectPath& adapter_object_path,
                           const dbus::ObjectPath& application_path,
                           const Options& options,
                           base::OnceClosure callback,
                           ErrorCallback error_callback) override;
  void UnregisterApplication(const dbus::ObjectPath& adapter_object_path,
                             const dbus::ObjectPath& application_path,
                             base::OnceClosure callback,
                             ErrorCallback error_callback) override;

  // Register, unregister, and retrieve pointers to application service
  // providers. Automatically called from the application provider constructor
  // and destructors.
  void RegisterApplicationServiceProvider(
      FakeBluetoothGattApplicationServiceProvider* provider);
  void RegisterServiceServiceProvider(
      FakeBluetoothGattServiceServiceProvider* provider);
  void RegisterCharacteristicServiceProvider(
      FakeBluetoothGattCharacteristicServiceProvider* provider);
  void RegisterDescriptorServiceProvider(
      FakeBluetoothGattDescriptorServiceProvider* provider);

  void UnregisterApplicationServiceProvider(
      FakeBluetoothGattApplicationServiceProvider* provider);
  void UnregisterServiceServiceProvider(
      FakeBluetoothGattServiceServiceProvider* provider);
  void UnregisterCharacteristicServiceProvider(
      FakeBluetoothGattCharacteristicServiceProvider* provider);
  void UnregisterDescriptorServiceProvider(
      FakeBluetoothGattDescriptorServiceProvider* provider);

  // Return a pointer to the service provider that corresponds to the object
  // path |object_path| if it exists.
  FakeBluetoothGattServiceServiceProvider* GetServiceServiceProvider(
      const dbus::ObjectPath& object_path) const;
  FakeBluetoothGattCharacteristicServiceProvider*
  GetCharacteristicServiceProvider(const dbus::ObjectPath& object_path) const;
  FakeBluetoothGattDescriptorServiceProvider* GetDescriptorServiceProvider(
      const dbus::ObjectPath& object_path) const;

  bool IsServiceRegistered(const dbus::ObjectPath& object_path) const;

 private:
  // The boolean indicates whether this application service provider is
  // registered or not.
  using ApplicationProvider =
      std::pair<FakeBluetoothGattApplicationServiceProvider*, bool>;

  // Mappings for GATT application, service, characteristic, and descriptor
  // service providers. The fake GATT manager stores references to all
  // instances created so that they can be obtained by tests.
  using ApplicationMap = std::map<dbus::ObjectPath, ApplicationProvider>;
  using ServiceMap = std::map<
      dbus::ObjectPath,
      raw_ptr<FakeBluetoothGattServiceServiceProvider, CtnExperimental>>;
  using CharacteristicMap = std::map<
      dbus::ObjectPath,
      raw_ptr<FakeBluetoothGattCharacteristicServiceProvider, CtnExperimental>>;
  using DescriptorMap = std::map<
      dbus::ObjectPath,
      raw_ptr<FakeBluetoothGattDescriptorServiceProvider, CtnExperimental>>;

  // Return a pointer to the Application provider that corresponds to the object
  // path |object_path| if it exists.
  ApplicationProvider* GetApplicationServiceProvider(
      const dbus::ObjectPath& object_path);

  // Find attribute providers in this application.
  std::set<dbus::ObjectPath> FindServiceProviders(
      dbus::ObjectPath application_path);
  std::set<dbus::ObjectPath> FindCharacteristicProviders(
      dbus::ObjectPath application_path);
  std::set<dbus::ObjectPath> FindDescriptorProviders(
      dbus::ObjectPath application_path);

  // Verify that the attribute hierarchy exposed by this application provider
  // is correct. i.e., all descriptors have a characteristic, which all have a
  // service and all the attributes are under this application's object path.
  bool VerifyProviderHierarchy(
      FakeBluetoothGattApplicationServiceProvider* application_provider);

  ApplicationMap application_map_;
  ServiceMap service_map_;
  CharacteristicMap characteristic_map_;
  DescriptorMap descriptor_map_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_MANAGER_CLIENT_H_
