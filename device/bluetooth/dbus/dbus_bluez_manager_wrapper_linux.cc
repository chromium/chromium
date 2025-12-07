// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/dbus_bluez_manager_wrapper_linux.h"

#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/bus.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"

namespace bluez {

// static
void DBusBluezManagerWrapperLinux::Initialize() {
  BluezDBusManager::Initialize(dbus_thread_linux::GetSharedSystemBus().get());
}

// static
void DBusBluezManagerWrapperLinux::Shutdown() {
  bluez::BluezDBusManager::Shutdown();
}

}  // namespace bluez
