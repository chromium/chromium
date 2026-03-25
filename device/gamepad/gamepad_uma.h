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

// Outcome of a gamepad connection attempt for the GameController data fetcher.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GameControllerMacOutcome)
enum class GameControllerMacOutcome {
  kSuccess = 0,
  kNoExtendedGamepad = 1,
  kIsNintendoGamepad = 2,
  kXboxFeatureDisabled = 3,
  kPlayStationFeatureDisabled = 4,
  kIsHidDevice = 5,
  kNoSlotAvailable = 6,
  kAlreadyConnected = 7,
  kMaxValue = kAlreadyConnected,
};
// LINT.ThenChange(//tools/metrics/histograms/others/enums.xml:GameControllerMacOutcome)

// Outcome of a gamepad connection attempt for the GamepadPlatform data fetcher.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GamepadPlatformMacOutcome)
enum class GamepadPlatformMacOutcome {
  kSuccess = 0,
  kHandledByGameController = 1,
  kIsNintendoGamepad = 2,
  kIsMfiGamepad = 3,
  kNoButtonsOrAxes = 4,
  kNoSlotAvailable = 5,
  kAlreadyConnected = 6,
  kMaxValue = kAlreadyConnected,
};
// LINT.ThenChange(//tools/metrics/histograms/others/enums.xml:GamepadPlatformMacOutcome)

// Outcome of a gamepad connection attempt for the specialized Xbox data
// fetcher. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
// LINT.IfChange(XboxMacOutcome)
enum class XboxMacOutcome {
  kSuccess = 0,
  kNoSlotAvailable = 1,
  kAlreadyConnected = 2,
  kMaxValue = kAlreadyConnected,
};
// LINT.ThenChange(//tools/metrics/histograms/others/enums.xml:XboxMacOutcome)

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

// Record the outcome of a gamepad connection attempt for the GameController
// data fetcher.
void RecordGameControllerMacOutcome(GameControllerMacOutcome outcome);

// Record the outcome of a gamepad connection attempt for the GamepadPlatform
// data fetcher.
void RecordGamepadPlatformMacOutcome(GamepadPlatformMacOutcome outcome);

// Record the outcome of a gamepad connection attempt for the Xbox data
// fetcher.
void RecordXboxMacOutcome(XboxMacOutcome outcome);

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_UMA_H_
