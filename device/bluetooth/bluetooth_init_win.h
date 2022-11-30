// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_INIT_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_INIT_WIN_H_

// windows.h needs to be included before bluetoothapis.h.
#include <windows.h>

#include <bluetoothapis.h>
#include <delayimp.h>

// ws2def.h needs to be included before ws2bth.h.
#include <ws2def.h>

#include <ws2bth.h>

#include "device/bluetooth/bluetooth_export.h"

namespace device {
namespace bluetooth_init_win {

// Returns true if the machine has a bluetooth stack available. The first call
// to this function will involve file IO, so it should be done on an appropriate
// thread. This function is not thread-safe.
bool DEVICE_BLUETOOTH_EXPORT HasBluetoothStack();

}  // namespace bluetooth_init_win
}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_INIT_WIN_H_
