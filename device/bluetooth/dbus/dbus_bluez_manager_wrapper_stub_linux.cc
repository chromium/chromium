// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/dbus/dbus_bluez_manager_wrapper_linux.h"

namespace bluez {

// Stub implementation for targets that don't use DBus.
// static
void DBusBluezManagerWrapperLinux::Initialize() {}

// Stub implementation for targets that don't use DBus.
// static
void DBusBluezManagerWrapperLinux::Shutdown() {}

}  // namespace bluez
