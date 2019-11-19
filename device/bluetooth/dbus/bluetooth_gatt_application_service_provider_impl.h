// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_APPLICATION_SERVICE_PROVIDER_IMPL_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_APPLICATION_SERVICE_PROVIDER_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_application_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_characteristic_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_service_provider.h"

namespace bluez {

class BluetoothLocalGattServiceBlueZ;

// The BluetoothGattApplicationServiceProvider implementation used in
// production.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattApplicationServiceProviderImpl
    : public BluetoothGattApplicationServiceProvider {
 public:
  // Use nullptr for |bus| to create for testing.
  BluetoothGattApplicationServiceProviderImpl(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      const std::map<dbus::ObjectPath, BluetoothLocalGattServiceBlueZ*>&
          services);
  ~BluetoothGattApplicationServiceProviderImpl() override;

 private:
  friend class BluetoothGattApplicationServiceProviderTest;
  FRIEND_TEST_ALL_PREFIXES(BluetoothGattApplicationServiceProviderTest,
                           GetManagedObjects);

  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread();

  template <typename Attribute>
  void WriteObjectDict(dbus::MessageWriter* writer,
                       const std::string& attribute_interface,
                       Attribute* attribute);
  template <typename Attribute>
  void WriteInterfaceDict(dbus::MessageWriter* writer,
                          const std::string& attribute_interface,
                          Attribute* attribute);

  void WriteAttributeProperties(
      dbus::MessageWriter* writer,
      BluetoothGattServiceServiceProvider* service_provider);
  void WriteAttributeProperties(
      dbus::MessageWriter* writer,
      BluetoothGattCharacteristicServiceProvider* characteristic_provider);
  void WriteAttributeProperties(
      dbus::MessageWriter* writer,
      BluetoothGattDescriptorServiceProvider* descriptor_provider);

  // Called by dbus:: when the Bluetooth daemon wants to fetch all the objects
  // managed by this object manager.
  void GetManagedObjects(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Origin thread (i.e. the UI thread in production).
  base::PlatformThreadId origin_thread_id_;

  // D-Bus bus object is exported on, not owned by this object and must
  // outlive it.
  dbus::Bus* bus_;

  // D-Bus object path of object we are exporting, kept so we can unregister
  // again in our destructor.
  dbus::ObjectPath object_path_;

  // D-Bus object we are exporting, owned by this object.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothGattApplicationServiceProviderImpl>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattApplicationServiceProviderImpl);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_APPLICATION_SERVICE_PROVIDER_IMPL_H_
