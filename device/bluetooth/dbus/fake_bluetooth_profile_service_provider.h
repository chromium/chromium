// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_PROFILE_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_PROFILE_SERVICE_PROVIDER_H_

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_profile_service_provider.h"

namespace bluez {

// FakeBluetoothProfileServiceProvider simulates the behavior of a local
// Bluetooth agent object and is used both in test cases in place of a
// mock and on the Linux desktop.
//
// This class is only called from the dbus origin thread and is not thread-safe.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothProfileServiceProvider
    : public BluetoothProfileServiceProvider {
 public:
  FakeBluetoothProfileServiceProvider(const dbus::ObjectPath& object_path,
                                      Delegate* delegate);
  ~FakeBluetoothProfileServiceProvider() override;

  // Each of these calls the equivalent
  // BluetoothProfileServiceProvider::Delegate method on the object passed on
  // construction.
  void Released();
  void NewConnection(const dbus::ObjectPath& device_path,
                     base::ScopedFD fd,
                     const Delegate::Options& options,
                     Delegate::ConfirmationCallback callback);
  void RequestDisconnection(const dbus::ObjectPath& device_path,
                            Delegate::ConfirmationCallback callback);
  void Cancel();

  const dbus::ObjectPath& object_path() const { return object_path_; }

 private:
  friend class FakeBluetoothProfileManagerClient;

  // D-Bus object path we are faking.
  dbus::ObjectPath object_path_;

  // All incoming method calls are passed on to the Delegate and a callback
  // passed to generate the reply. |delegate_| is generally the object that
  // owns this one, and must outlive it.
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(FakeBluetoothProfileServiceProvider);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_PROFILE_SERVICE_PROVIDER_H_
