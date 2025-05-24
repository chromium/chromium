// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_ADAPTER_PROFILE_BLUEZ_H_
#define DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_ADAPTER_PROFILE_BLUEZ_H_

#include <stddef.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluez/bluetooth_adapter_bluez.h"
#include "device/bluetooth/dbus/bluetooth_profile_manager_client.h"
#include "device/bluetooth/dbus/bluetooth_profile_service_provider.h"

namespace device {
class BluetoothUUID;
}  // namespace device

namespace bluez {

// The BluetoothAdapterProfileBlueZ class implements a multiplexing
// profile for custom Bluetooth services managed by a BluetoothAdapter.
// Maintains a list of delegates which may serve the profile.
// One delegate is allowed for each device.
//
// This class is not thread-safe, but is only called from the dbus origin
// thread.
//
// BluetoothAdapterProfileBlueZ objects are owned by the
// BluetoothAdapterBlueZ and allocated through Register()
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterProfileBlueZ
    : public bluez::BluetoothProfileServiceProvider::Delegate {
 public:
  using ProfileRegisteredCallback = base::OnceCallback<void(
      std::unique_ptr<BluetoothAdapterProfileBlueZ> profile)>;

  // Registers a profile with the BlueZ server for |uuid| with the
  // options |options|.  |success_callback| is provided with a newly
  // allocated profile if registration is successful, otherwise |error_callback|
  // will be called.
  static void Register(
      const device::BluetoothUUID& uuid,
      const bluez::BluetoothProfileManagerClient::Options& options,
      ProfileRegisteredCallback success_callback,
      bluez::BluetoothProfileManagerClient::ErrorCallback error_callback);

  BluetoothAdapterProfileBlueZ(const BluetoothAdapterProfileBlueZ&) = delete;
  BluetoothAdapterProfileBlueZ& operator=(const BluetoothAdapterProfileBlueZ&) =
      delete;

  ~BluetoothAdapterProfileBlueZ() override;

  // The object path of the profile.
  const dbus::ObjectPath& object_path() const { return object_path_; }

  // Returns the UUID of the profile
  const device::BluetoothUUID& uuid() const { return *uuid_; }

  // Add a delegate for a device associated with this profile.
  // An empty |device_path| indicates a local listening service.
  // Returns true if the delegate was set, and false if the |device_path|
  // already had a delegate set.
  bool SetDelegate(const dbus::ObjectPath& device_path,
                   bluez::BluetoothProfileServiceProvider::Delegate* delegate);

  // Remove the delegate for a device. |unregistered_callback| will be called
  // if this unregisters the profile.
  void RemoveDelegate(const dbus::ObjectPath& device_path,
                      base::OnceClosure unregistered_callback);

  // Returns the number of delegates for this profile.
  size_t DelegateCount() const { return delegates_.size(); }

 private:
  BluetoothAdapterProfileBlueZ(const device::BluetoothUUID& uuid);

  // bluez::BluetoothProfileServiceProvider::Delegate:
  void Released() override;
  void NewConnection(
      const dbus::ObjectPath& device_path,
      base::ScopedFD fd,
      const bluez::BluetoothProfileServiceProvider::Delegate::Options& options,
      ConfirmationCallback callback) override;
  void RequestDisconnection(const dbus::ObjectPath& device_path,
                            ConfirmationCallback callback) override;
  void Cancel() override;

  // Called by dbus:: on completion of the D-Bus method to unregister a profile.
  void OnUnregisterProfileError(base::OnceClosure unregistered_callback,
                                const std::string& error_name,
                                const std::string& error_message);

  // List of delegates which this profile is multiplexing to.
  std::map<std::string,
           raw_ptr<bluez::BluetoothProfileServiceProvider::Delegate,
                   CtnExperimental>>
      delegates_;

  // The UUID that this profile represents.
  const raw_ref<const device::BluetoothUUID, AcrossTasksDanglingUntriaged>
      uuid_;

  // Registered dbus object path for this profile.
  dbus::ObjectPath object_path_;

  // Profile dbus object for receiving profile method calls from BlueZ
  std::unique_ptr<bluez::BluetoothProfileServiceProvider> profile_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterProfileBlueZ> weak_ptr_factory_{this};
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_BLUEZ_BLUETOOTH_ADAPTER_PROFILE_BLUEZ_H_
