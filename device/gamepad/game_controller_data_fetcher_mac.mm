// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/gamepad/game_controller_data_fetcher_mac.h"

#import <GameController/GameController.h>
#include <string.h>

#include <string>

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "device/gamepad/gamepad_standard_mappings.h"

namespace device {

namespace {

const int kGCControllerPlayerIndexCount = 4;

// Returns true if |controller| should be enumerated by this data fetcher.
bool IsSupported(GCController* controller) {
  // We only support the extendedGamepad profile.
  if (!controller.extendedGamepad) {
    return false;
  }

  // In macOS 10.15, support for some console gamepads was added to the Game
  // Controller framework and a productCategory property was added to enable
  // applications to detect the new devices. These gamepads are already
  // supported in Chrome through other data fetchers and must be blocked here to
  // avoid double-enumeration.
  NSString* product_category = controller.productCategory;
  if ([product_category isEqualToString:@"HID"] ||
      [product_category isEqualToString:@"Xbox One"] ||
      [product_category isEqualToString:@"DualShock 4"] ||
      [product_category isEqualToString:@"DualSense"] ||
      [product_category isEqualToString:@"Switch Pro Controller"] ||
      [product_category isEqualToString:@"Nintendo Switch JoyCon (L/R)"]) {
    return false;
  }

  return true;
}

}  // namespace

GameControllerDataFetcherMac::GameControllerDataFetcherMac() = default;

GameControllerDataFetcherMac::~GameControllerDataFetcherMac() = default;

GamepadSource GameControllerDataFetcherMac::source() {
  return Factory::static_source();
}

void GameControllerDataFetcherMac::GetGamepadData(bool) {
  NSArray* controllers = [GCController controllers];

  // In the first pass, record which player indices are still in use so unused
  // indices can be assigned to newly connected gamepads.
  bool player_indices[Gamepads::kItemsLengthCap];
  std::fill(player_indices, player_indices + Gamepads::kItemsLengthCap, false);
  for (GCController* controller in controllers) {
    if (!IsSupported(controller))
      continue;

    int player_index = controller.playerIndex;
    if (player_index != GCControllerPlayerIndexUnset)
      player_indices[player_index] = true;
  }

  for (size_t i = 0; i < Gamepads::kItemsLengthCap; ++i) {
    if (connected_[i] && !player_indices[i])
      connected_[i] = false;
  }

  // In the second pass, assign indices to newly connected gamepads and fetch
  // the gamepad state.
  for (GCController* controller in controllers) {
    if (!IsSupported(controller))
      continue;

    int player_index = controller.playerIndex;
    if (player_index == GCControllerPlayerIndexUnset) {
      player_index = NextUnusedPlayerIndex();
      if (player_index == GCControllerPlayerIndexUnset)
        continue;
    }

    PadState* state = GetPadState(player_index);
    if (!state)
      continue;

    Gamepad& pad = state->data;

    // This first time we encounter a gamepad, set its name, mapping, and
    // axes/button counts. This information is static, so it only needs to be
    // done once.
    if (!state->is_initialized) {
      state->is_initialized = true;
      NSString* vendorName = controller.vendorName;
      NSString* ident =
          [NSString stringWithFormat:@"%@ (STANDARD GAMEPAD)",
                                     vendorName ? vendorName : @"Unknown"];

      pad.mapping = GamepadMapping::kStandard;
      pad.SetID(base::SysNSStringToUTF16(ident));
      pad.axes_length = AXIS_INDEX_COUNT;
      pad.buttons_length = BUTTON_INDEX_COUNT - 1;
      pad.connected = true;
      connected_[player_index] = true;

      controller.playerIndex =
          static_cast<GCControllerPlayerIndex>(player_index);
    }

    pad.timestamp = CurrentTimeInMicroseconds();

    auto extended_gamepad = [controller extendedGamepad];
    pad.axes[AXIS_INDEX_LEFT_STICK_X] =
        extended_gamepad.leftThumbstick.xAxis.value;
    pad.axes[AXIS_INDEX_LEFT_STICK_Y] =
        -extended_gamepad.leftThumbstick.yAxis.value;
    pad.axes[AXIS_INDEX_RIGHT_STICK_X] =
        extended_gamepad.rightThumbstick.xAxis.value;
    pad.axes[AXIS_INDEX_RIGHT_STICK_Y] =
        -extended_gamepad.rightThumbstick.yAxis.value;

#define BUTTON(i, b)                      \
  pad.buttons[i].pressed = [b isPressed]; \
  pad.buttons[i].value = [b value];

    BUTTON(BUTTON_INDEX_PRIMARY, extended_gamepad.buttonA);
    BUTTON(BUTTON_INDEX_SECONDARY, extended_gamepad.buttonB);
    BUTTON(BUTTON_INDEX_TERTIARY, extended_gamepad.buttonX);
    BUTTON(BUTTON_INDEX_QUATERNARY, extended_gamepad.buttonY);
    BUTTON(BUTTON_INDEX_LEFT_SHOULDER, extended_gamepad.leftShoulder);
    BUTTON(BUTTON_INDEX_RIGHT_SHOULDER, extended_gamepad.rightShoulder);
    BUTTON(BUTTON_INDEX_LEFT_TRIGGER, extended_gamepad.leftTrigger);
    BUTTON(BUTTON_INDEX_RIGHT_TRIGGER, extended_gamepad.rightTrigger);

    // No start, select, or thumbstick buttons

    BUTTON(BUTTON_INDEX_DPAD_UP, extended_gamepad.dpad.up);
    BUTTON(BUTTON_INDEX_DPAD_DOWN, extended_gamepad.dpad.down);
    BUTTON(BUTTON_INDEX_DPAD_LEFT, extended_gamepad.dpad.left);
    BUTTON(BUTTON_INDEX_DPAD_RIGHT, extended_gamepad.dpad.right);

#undef BUTTON
  }
}

int GameControllerDataFetcherMac::NextUnusedPlayerIndex() {
  for (int i = 0; i < kGCControllerPlayerIndexCount; ++i) {
    if (!connected_[i])
      return i;
  }
  return GCControllerPlayerIndexUnset;
}

}  // namespace device
