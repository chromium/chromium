// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/game_controller_data_fetcher_mac.h"

#include <string.h>

#include "base/mac/mac_util.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "device/gamepad/gamepad_standard_mappings.h"

#import <GameController/GameController.h>

namespace device {

namespace {

const int kGCControllerPlayerIndexCount = 4;

// Returns true if |controller| should be enumerated by this data fetcher.
bool IsSupported(GCController* controller) {
  // We only support the extendedGamepad profile, the basic gamepad profile
  // appears to only be for iOS devices.
  if (![controller extendedGamepad])
    return false;

  // In OS X 10.15, Game Controller API added support for Xbox Wireless
  // Controller and Dualshock 4. A productCategory property was added to
  // GCController to allow applications to detect these new categories of
  // devices.
  //
  // In Chrome for Mac, Xbox Wireless Controller and Dualshock 4 are
  // enumerated by XboxDataFetcher and GamepadPlatformDataFetcherMac,
  // respectively. If GameControllerDataFetcherMac also enumerates these
  // devices, it will add duplicate gamepads to the gamepad list.
  //
  // On macOS 10.15 or later, use the productCategory property to distinguish
  // Xbox One and Dualshock 4 and block them from being enumerated by this
  // data fetcher. On 10.14 or earlier, compare the |vendor_name| against
  // known values for these devices. Once Chrome no longer supports 10.14, the
  // |vendor_name| path may be removed.
  if (base::mac::IsAtLeastOS10_15()) {
    NSString* product_category =
        [controller performSelector:@selector(productCategory)];
    if ([product_category isEqualToString:@"Xbox One"])
      return false;
    if ([product_category isEqualToString:@"DualShock 4"])
      return false;
  } else {
    NSString* vendor_name = [controller vendorName];
    if ([vendor_name isEqualToString:@"Xbox Wireless Controller"])
      return false;
    if ([vendor_name isEqualToString:@"Wireless Controller"])
      return false;
  }

  return true;
}

}  // namespace

GameControllerDataFetcherMac::GameControllerDataFetcherMac() {}

GameControllerDataFetcherMac::~GameControllerDataFetcherMac() {}

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

    int player_index = [controller playerIndex];
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

    int player_index = [controller playerIndex];
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
      NSString* vendorName = [controller vendorName];
      NSString* ident =
          [NSString stringWithFormat:@"%@ (STANDARD GAMEPAD)",
                                     vendorName ? vendorName : @"Unknown"];

      pad.mapping = GamepadMapping::kStandard;
      pad.SetID(base::SysNSStringToUTF16(ident));
      pad.axes_length = AXIS_INDEX_COUNT;
      pad.buttons_length = BUTTON_INDEX_COUNT - 1;
      pad.connected = true;
      connected_[player_index] = true;

// In OS X 10.11, the type of the GCController playerIndex member was
// changed from NSInteger to a GCControllerPlayerIndex enum. Once Chrome
// no longer supports OSX 10.10, the integer version can be removed.
#if !defined(MAC_OS_X_VERSION_10_11) || \
    MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_11
      [controller setPlayerIndex:player_index];
#else
      [controller
          setPlayerIndex:static_cast<GCControllerPlayerIndex>(player_index)];
#endif
    }

    pad.timestamp = CurrentTimeInMicroseconds();

    auto extended_gamepad = [controller extendedGamepad];
    pad.axes[AXIS_INDEX_LEFT_STICK_X] =
        [[[extended_gamepad leftThumbstick] xAxis] value];
    pad.axes[AXIS_INDEX_LEFT_STICK_Y] =
        -[[[extended_gamepad leftThumbstick] yAxis] value];
    pad.axes[AXIS_INDEX_RIGHT_STICK_X] =
        [[[extended_gamepad rightThumbstick] xAxis] value];
    pad.axes[AXIS_INDEX_RIGHT_STICK_Y] =
        -[[[extended_gamepad rightThumbstick] yAxis] value];

#define BUTTON(i, b)                      \
  pad.buttons[i].pressed = [b isPressed]; \
  pad.buttons[i].value = [b value];

    BUTTON(BUTTON_INDEX_PRIMARY, [extended_gamepad buttonA]);
    BUTTON(BUTTON_INDEX_SECONDARY, [extended_gamepad buttonB]);
    BUTTON(BUTTON_INDEX_TERTIARY, [extended_gamepad buttonX]);
    BUTTON(BUTTON_INDEX_QUATERNARY, [extended_gamepad buttonY]);
    BUTTON(BUTTON_INDEX_LEFT_SHOULDER, [extended_gamepad leftShoulder]);
    BUTTON(BUTTON_INDEX_RIGHT_SHOULDER, [extended_gamepad rightShoulder]);
    BUTTON(BUTTON_INDEX_LEFT_TRIGGER, [extended_gamepad leftTrigger]);
    BUTTON(BUTTON_INDEX_RIGHT_TRIGGER, [extended_gamepad rightTrigger]);

    // No start, select, or thumbstick buttons

    BUTTON(BUTTON_INDEX_DPAD_UP, [[extended_gamepad dpad] up]);
    BUTTON(BUTTON_INDEX_DPAD_DOWN, [[extended_gamepad dpad] down]);
    BUTTON(BUTTON_INDEX_DPAD_LEFT, [[extended_gamepad dpad] left]);
    BUTTON(BUTTON_INDEX_DPAD_RIGHT, [[extended_gamepad dpad] right]);

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
