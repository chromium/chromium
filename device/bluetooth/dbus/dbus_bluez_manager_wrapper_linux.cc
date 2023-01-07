// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/dbus_bluez_manager_wrapper_linux.h"

#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/bluez_dbus_thread_manager.h"

namespace bluez {

// static
void DBusBluezManagerWrapperLinux::Initialize() {
  BluezDBusManager::Initialize(nullptr /* system_bus */);
}

// static
void DBusBluezManagerWrapperLinux::Shutdown() {
  bluez::BluezDBusManager::Shutdown();
  bluez::BluezDBusThreadManager::Shutdown();
}

}  // namespace bluez
