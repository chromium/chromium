// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_SERVICE_PROVIDER_IMPL_H_
#define DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_SERVICE_PROVIDER_IMPL_H_

#include <string>

#include "base/containers/span.h"
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
#include "device/bluetooth/dbus/bluetooth_advertisement_monitor_service_provider.h"

namespace bluez {

// The BluetoothAdvertisementMonitorServiceProvider implementation used in
// production.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdvertisementMonitorServiceProviderImpl
    : public BluetoothAdvertisementMonitorServiceProvider {
 public:
  BluetoothAdvertisementMonitorServiceProviderImpl(
      dbus::Bus* bus,
      const dbus::ObjectPath& object_path,
      std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter,
      base::WeakPtr<Delegate> delegate);

  ~BluetoothAdvertisementMonitorServiceProviderImpl() override;
  BluetoothAdvertisementMonitorServiceProviderImpl(
      const BluetoothAdvertisementMonitorServiceProviderImpl&) = delete;
  BluetoothAdvertisementMonitorServiceProviderImpl& operator=(
      const BluetoothAdvertisementMonitorServiceProviderImpl&) = delete;

 private:
  // Returns true if the current thread is on the origin thread.
  bool OnOriginThread() const;

  void Release(dbus::MethodCall* method_call,
               dbus::ExportedObject::ResponseSender response_sender);
  FRIEND_TEST_ALL_PREFIXES(BluetoothAdvertisementMonitorServiceProviderImplTest,
                           Release);

  void Activate(dbus::MethodCall* method_call,
                dbus::ExportedObject::ResponseSender response_sender);
  FRIEND_TEST_ALL_PREFIXES(BluetoothAdvertisementMonitorServiceProviderImplTest,
                           Activate);

  void DeviceFound(dbus::MethodCall* method_call,
                   dbus::ExportedObject::ResponseSender response_sender);
  FRIEND_TEST_ALL_PREFIXES(BluetoothAdvertisementMonitorServiceProviderImplTest,
                           DeviceFound);
  FRIEND_TEST_ALL_PREFIXES(BluetoothAdvertisementMonitorServiceProviderImplTest,
                           DeviceFoundFailure);

  void DeviceLost(dbus::MethodCall* method_call,
                  dbus::ExportedObject::ResponseSender response_sender);
  FRIEND_TEST_ALL_PREFIXES(BluetoothAdvertisementMonitorServiceProviderImplTest,
                           DeviceLost);
  FRIEND_TEST_ALL_PREFIXES(BluetoothAdvertisementMonitorServiceProviderImplTest,
                           DeviceLostFailure);

  void WriteProperties(dbus::MessageWriter* writer) override;

  // Called by dbus:: when a method is exported.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  bool success);

  void WritePattern(dbus::MessageWriter* pattern_array_writer,
                    uint8_t start_pos,
                    uint8_t ad_data_type,
                    base::span<const uint8_t> pattern);

  const dbus::ObjectPath& object_path() const override;

  // Origin thread (i.e. the UI thread in production).
  const base::PlatformThreadId origin_thread_id_;

  // D-Bus bus object is exported on, not owned by this object and must
  // outlive it.
  raw_ptr<dbus::Bus> bus_;

  // D-Bus object path of object we are exporting, kept so we can unregister
  // again in our destructor.
  const dbus::ObjectPath object_path_;

  std::unique_ptr<device::BluetoothLowEnergyScanFilter> filter_;

  base::WeakPtr<BluetoothAdvertisementMonitorServiceProvider::Delegate>
      delegate_;

  // D-Bus object we are exporting, owned by this object.
  scoped_refptr<dbus::ExportedObject> exported_object_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdvertisementMonitorServiceProviderImpl>
      weak_ptr_factory_{this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_BLUETOOTH_ADVERTISEMENT_MONITOR_SERVICE_PROVIDER_IMPL_H_
