// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_SERVICE_SERVICE_PROVIDER_IMPL_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_SERVICE_SERVICE_PROVIDER_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_gatt_service_service_provider.h"

namespace bluez {

// The BluetoothGattServiceServiceProvider implementation used in production.
class DEVICE_BLUETOOTH_EXPORT BluetoothGattServiceServiceProviderImpl
    : public BluetoothGattServiceServiceProvider {
 public:
  // Use nullptr for |bus| to create for testing.
  BluetoothGattServiceServiceProviderImpl(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      const std::string& uuid,
      bool is_primary,
      const std::vector<dbus::ObjectPath>& includes);

  ~BluetoothGattServiceServiceProviderImpl() override;

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread();

  // Called by dbus:: when the Bluetooth daemon fetches a single property of
  // the service.
  void Get(dbus::MethodCall* method_call,
           dbus::ExportedObject::ResponseSender response_sender);

  // Called by dbus:: when the Bluetooth daemon sets a single property of the
  // service.
  void Set(dbus::MethodCall* method_call,
           dbus::ExportedObject::ResponseSender response_sender);

  // Called by dbus:: when the Bluetooth daemon fetches all properties of the
  // service.
  void GetAll(dbus::MethodCall* method_call,
              dbus::ExportedObject::ResponseSender response_sender);

  void WriteProperties(dbus::MessageWriter* writer) override;

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  const dbus::ObjectPath& object_path() const override;

  // Origin thread (i.e. the UI thread in production).
  base::PlatformThreadId origin_thread_id_;

  // 128-bit service UUID of this object.
  std::string uuid_;

  // Flag indicating that this is a primary service.
  bool is_primary_;

  // List of object paths that represent other exported GATT services that are
  // included from this service.
  std::vector<dbus::ObjectPath> includes_;

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
  base::WeakPtrFactory<BluetoothGattServiceServiceProviderImpl>
      weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothGattServiceServiceProviderImpl);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_GATT_SERVICE_SERVICE_PROVIDER_IMPL_H_
