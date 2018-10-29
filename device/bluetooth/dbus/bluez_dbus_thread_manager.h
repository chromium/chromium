// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_BLUEZ_DBUS_THREAD_MANAGER_H_
#define DEVICE_BLUETOOTH_DBUS_BLUEZ_DBUS_THREAD_MANAGER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_export.h"

namespace base {
class Thread;
}  // namespace base

namespace dbus {
class Bus;
}  // namespace dbus

namespace bluez {

// BluezDBusThreadManager manages the D-Bus thread, the thread dedicated to
// handling asynchronous D-Bus operations.
class DEVICE_BLUETOOTH_EXPORT BluezDBusThreadManager {
 public:
  // Sets the global instance. Must be called before any calls to Get().
  // We explicitly initialize and shut down the global object, rather than
  // making it a Singleton, to ensure clean startup and shutdown.
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static BluezDBusThreadManager* Get();

  // Returns various D-Bus bus instances, owned by BluezDBusThreadManager.
  dbus::Bus* GetSystemBus();

 private:
  explicit BluezDBusThreadManager();
  ~BluezDBusThreadManager();

  std::unique_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> system_bus_;

  DISALLOW_COPY_AND_ASSIGN(BluezDBusThreadManager);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_DBUS_THREAD_MANAGER_LINUX_H_
