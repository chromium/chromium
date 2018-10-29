// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_DBUS_BLUEZ_MANAGER_WRAPPER_LINUX_H_
#define DEVICE_BLUETOOTH_DBUS_DBUS_BLUEZ_MANAGER_WRAPPER_LINUX_H_

#include "base/macros.h"
#include "device/bluetooth/bluetooth_export.h"

namespace bluez {

// This class abstracts the initialization of BluezDBusThreadManager and
// BluezDBusManager. Targets that don't use DBus can provide stub
// implementations.
class DEVICE_BLUETOOTH_EXPORT DBusBluezManagerWrapperLinux {
 public:
  static void Initialize();
  static void Shutdown();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DBusBluezManagerWrapperLinux);
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_DBUS_BLUEZ_MANAGER_WRAPPER_LINUX_H_
