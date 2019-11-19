// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/util/gamepad_builder.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"

namespace device {

namespace {
constexpr double kThumbstickDeadzone = 0.16;

GamepadHand MojoToGamepadHandedness(device::mojom::XRHandedness handedness) {
  switch (handedness) {
    case device::mojom::XRHandedness::LEFT:
      return GamepadHand::kLeft;
    case device::mojom::XRHandedness::RIGHT:
      return GamepadHand::kRight;
    case device::mojom::XRHandedness::NONE:
      return GamepadHand::kNone;
  }

  NOTREACHED();
}

}  // anonymous namespace

GamepadBuilder::GamepadBuilder(const std::string& gamepad_id,
                               GamepadMapping mapping,
                               device::mojom::XRHandedness handedness) {
  DCHECK_LT(gamepad_id.size(), Gamepad::kIdLengthCap);

  gamepad_.connected = true;
  gamepad_.timestamp = base::TimeTicks::Now().since_origin().InMicroseconds();
  gamepad_.mapping = mapping;
  gamepad_.hand = MojoToGamepadHandedness(handedness);
  gamepad_.SetID(base::UTF8ToUTF16(gamepad_id));
}

GamepadBuilder::~GamepadBuilder() = default;

bool GamepadBuilder::IsValid() const {
  switch (GetMapping()) {
    case GamepadMapping::kXrStandard:
      // Just a single primary button is sufficient for the xr-standard mapping.
      return gamepad_.buttons_length > 0;
    case GamepadMapping::kStandard:
    case GamepadMapping::kNone:
      // Neither standard requires any buttons to be set, and all other data
      // is set in the constructor.
      return true;
  }

  NOTREACHED();
}

base::Optional<Gamepad> GamepadBuilder::GetGamepad() {
  if (IsValid())
    return gamepad_;

  return base::nullopt;
}

void GamepadBuilder::AddButton(const GamepadButton& button) {
  DCHECK_LT(gamepad_.buttons_length, Gamepad::kButtonsLengthCap);
  gamepad_.buttons[gamepad_.buttons_length++] = button;
}

void GamepadBuilder::AddButton(const ButtonData& data) {
  AddButton(GamepadButton(data.pressed, data.touched, data.value));
  if (data.type != ButtonData::Type::kButton)
    AddAxes(data);
}

void GamepadBuilder::AddAxis(double value, double deadzone) {
  DCHECK_LT(gamepad_.axes_length, Gamepad::kAxesLengthCap);
  gamepad_.axes[gamepad_.axes_length++] =
      std::fabs(value) < deadzone ? 0.0 : value;
}

void GamepadBuilder::AddAxes(const ButtonData& data) {
  DCHECK_NE(data.type, ButtonData::Type::kButton);

  if (data.type == ButtonData::Type::kTouchpad && !data.touched) {
    // Untouched touchpads must have axes set to 0.
    AddPlaceholderAxes();
    return;
  }

  const double deadzone =
      data.type == ButtonData::Type::kThumbstick ? kThumbstickDeadzone : 0.0;
  AddAxis(data.x_axis, deadzone);
  AddAxis(data.y_axis, deadzone);
}

void GamepadBuilder::AddPlaceholderButton() {
  AddButton(GamepadButton());
}

void GamepadBuilder::RemovePlaceholderButton() {
  // Since this is a member array, it actually is full of default constructed
  // buttons, so all we have to do to remove a button is decrement the length
  // variable.  However, we should check before we do so that we actually have
  // a length and that there's not any data that's been set in the alleged
  // placeholder button.
  DCHECK_GT(gamepad_.buttons_length, 0u);
  GamepadButton button = gamepad_.buttons[gamepad_.buttons_length - 1];
  DCHECK(!button.pressed && !button.touched && button.value == 0);
  gamepad_.buttons_length--;
}

void GamepadBuilder::AddPlaceholderAxes() {
  AddAxis(0.0);
  AddAxis(0.0);
}

}  // namespace device
