// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_APPLICATION_SERVICE_PROVIDER_IMPL_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_APPLICATION_SERVICE_PROVIDER_IMPL_H_

#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/platform_thread.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_application_service_provider.h"
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider.h"

namespace bluez {

// The BluetoothAdvertisementMonitorApplicationServiceProvider implementation
// used in production.
class DEVICE_BLUETOOTH_EXPORT
    BluetoothAdvertisementMonitorApplicationServiceProviderImpl
    : public BluetoothAdvertisementMonitorApplicationServiceProvider {
 public:
  // Use nullptr for |bus| to create for testing.
  BluetoothAdvertisementMonitorApplicationServiceProviderImpl(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path);

  BluetoothAdvertisementMonitorApplicationServiceProviderImpl(
      const BluetoothAdvertisementMonitorApplicationServiceProviderImpl&) =
      delete;
  BluetoothAdvertisementMonitorApplicationServiceProviderImpl& operator=(
      const BluetoothAdvertisementMonitorApplicationServiceProviderImpl&) =
      delete;

  ~BluetoothAdvertisementMonitorApplicationServiceProviderImpl() override;

  // BluetoothAdvertisementMonitorApplicationServiceProvider overrides:
  void AddMonitor(std::unique_ptr<BluetoothAdvertisementMonitorServiceProvider>
                      advertisement_monitor_service_provider) override;
  void RemoveMonitor(const dbus::ObjectPath& monitor_path) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(
      BluetoothAdvertisementMonitorApplicationServiceProviderImplTest,
      AddMonitorThenRemoveMonitor);

  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() const;

  void WriteObjectDict(dbus::MessageWriter* writer,
                       const std::string& attribute_interface,
                       BluetoothAdvertisementMonitorServiceProvider*
                           advertisement_monitor_service_provider);

  void WriteInterfaceDict(dbus::MessageWriter* writer,
                          const std::string& attribute_interface,
                          BluetoothAdvertisementMonitorServiceProvider*
                              advertisement_monitor_service_provider);

  // Called by dbus:: when the Bluetooth daemon wants to fetch all the objects
  // managed by this object manager.
  void GetManagedObjects(dbus::MethodCall* method_call,
                         dbus::ExportedObject::ResponseSender response_sender);

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  // Origin thread (i.e. the UI thread in production).
  const base::PlatformThreadId origin_thread_id_;

  // D-Bus bus object is exported on, not owned by this object and must
  // outlive it.
  raw_ptr<dbus::Bus> bus_;

  // D-Bus object path of object we are exporting, kept so we can unregister
  // again in our destructor.
  const dbus::ObjectPath object_path_;

  // D-Bus object we are exporting, owned by this object.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Key is the object path of the AdvertisementMonitorServiceProvider
  std::map<std::string,
           std::unique_ptr<BluetoothAdvertisementMonitorServiceProvider>>
      advertisement_monitor_providers_;

  base::WeakPtrFactory<
      BluetoothAdvertisementMonitorApplicationServiceProviderImpl>
      weak_ptr_factory_{this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_APPLICATION_SERVICE_PROVIDER_IMPL_H_
