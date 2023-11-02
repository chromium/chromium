// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_UMA_H_
#define DEVICE_GAMEPAD_GAMEPAD_UMA_H_

#include <stddef.h>
#include <stdint.h>

#include "device/gamepad/gamepad_id_list.h"
#include "device/gamepad/gamepad_pad_state_provider.h"

namespace device {

// Compare the |gamepad_id| of a connected USB or Bluetooth device against a
// list of known gaming peripherals. If a match is found, record the GamepadId
// enumeration value corresponding to the device. Does nothing if the device is
// unknown.
//
// To preserve privacy, the vendor and product IDs are not recorded.
void RecordConnectedGamepad(GamepadId gamepad_id);

// Record that the gamepad data fetcher identified by |source| recognized a
// device as a gamepad, but the device is not included on our list of known
// gamepads.
void RecordUnknownGamepad(GamepadSource source);

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_UMA_H_
