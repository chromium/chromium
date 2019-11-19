// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_AGENT_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_AGENT_SERVICE_PROVIDER_H_

#include <stdint.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "dbus/object_path.h"
#include "dbus/property.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/dbus/bluetooth_agent_service_provider.h"

namespace bluez {

class FakeBluetoothAgentManagerClient;

// FakeBluetoothAgentServiceProvider simulates the behavior of a local
// Bluetooth agent object and is used both in test cases in place of a
// mock and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothAgentServiceProvider
    : public BluetoothAgentServiceProvider {
 public:
  FakeBluetoothAgentServiceProvider(const dbus::ObjectPath& object_path,
                                    Delegate* delegate);
  ~FakeBluetoothAgentServiceProvider() override;

  // Each of these calls the equivalent BluetoothAgentServiceProvider::Delegate
  // method on the object passed on construction.
  virtual void Release();
  virtual void RequestPinCode(const dbus::ObjectPath& device_path,
                              Delegate::PinCodeCallback callback);
  virtual void DisplayPinCode(const dbus::ObjectPath& device_path,
                              const std::string& pincode);
  virtual void RequestPasskey(const dbus::ObjectPath& device_path,
                              Delegate::PasskeyCallback callback);
  virtual void DisplayPasskey(const dbus::ObjectPath& device_path,
                              uint32_t passkey,
                              int16_t entered);
  virtual void RequestConfirmation(const dbus::ObjectPath& device_path,
                                   uint32_t passkey,
                                   Delegate::ConfirmationCallback callback);
  virtual void RequestAuthorization(const dbus::ObjectPath& device_path,
                                    Delegate::ConfirmationCallback callback);
  virtual void AuthorizeService(const dbus::ObjectPath& device_path,
                                const std::string& uuid,
                                Delegate::ConfirmationCallback callback);
  virtual void Cancel();

 private:
  friend class FakeBluetoothAgentManagerClient;

  // D-Bus object path we are faking.
  dbus::ObjectPath object_path_;

  // All incoming method calls are passed on to the Delegate and a callback
  // passed to generate the reply. |delegate_| is generally the object that
  // owns this one, and must outlive it.
  Delegate* delegate_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_AGENT_SERVICE_PROVIDER_H_
