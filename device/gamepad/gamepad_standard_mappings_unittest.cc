// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_pad_state_provider.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class TestGamepadPadStateProvider final : public GamepadPadStateProvider {
 public:
  Gamepad MapGamepadData(const Gamepad& input) {
    PadState pad_state;
    pad_state.data = input;

    Gamepad output;
    MapAndSanitizeGamepadData(&pad_state, &output, /*sanitize=*/false);
    return output;
  }

 private:
  void DisconnectUnrecognizedGamepad(GamepadSource source,
                                     int source_id) override {}
};

TEST(GamepadStandardMappingsTest, StandardButtonTypesSkipNullButtons) {
  TestGamepadPadStateProvider provider;
  Gamepad gamepad;
  gamepad.connected = true;
  gamepad.mapping = GamepadMapping::kStandard;
  gamepad.buttons_length = BUTTON_INDEX_COUNT;

  gamepad.buttons[BUTTON_INDEX_PRIMARY] = GamepadButton(false, false, 0.0);
  gamepad.buttons[BUTTON_INDEX_SECONDARY] = GamepadButton();

  Gamepad mapped = provider.MapGamepadData(gamepad);
  EXPECT_EQ(GamepadButtonType::kStandard,
            mapped.buttons[BUTTON_INDEX_PRIMARY].type);
  EXPECT_EQ(GamepadButtonType::kNonStandard,
            mapped.buttons[BUTTON_INDEX_SECONDARY].type);
}

TEST(GamepadStandardMappingsTest, StandardButtonTypesSkipExtendedButtons) {
  TestGamepadPadStateProvider provider;
  Gamepad gamepad;
  gamepad.connected = true;
  gamepad.mapping = GamepadMapping::kStandard;
  gamepad.buttons_length = BUTTON_INDEX_COUNT + 1;

  for (size_t i = 0; i < gamepad.buttons_length; ++i) {
    gamepad.buttons[i] = GamepadButton(false, false, 0.0);
  }

  Gamepad mapped = provider.MapGamepadData(gamepad);
  EXPECT_EQ(GamepadButtonType::kStandard,
            mapped.buttons[BUTTON_INDEX_COUNT - 1].type);
  EXPECT_EQ(GamepadButtonType::kNonStandard,
            mapped.buttons[BUTTON_INDEX_COUNT].type);
}

}  // namespace

}  // namespace device
