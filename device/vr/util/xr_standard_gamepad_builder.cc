// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/util/xr_standard_gamepad_builder.h"

namespace device {

XRStandardGamepadBuilder::XRStandardGamepadBuilder(
    device::mojom::XRHandedness handedness)
    : handedness_(handedness) {}

XRStandardGamepadBuilder::~XRStandardGamepadBuilder() = default;

void XRStandardGamepadBuilder::AddOptionalButtonData(
    const GamepadBuilder::ButtonData& data) {
  optional_button_data_.push_back(data);
  if (data.type != GamepadBuilder::ButtonData::Type::kButton) {
    has_optional_axes_ = true;
  }
}

void XRStandardGamepadBuilder::AddOptionalButtonData(
    const GamepadButton& button) {
  GamepadBuilder::ButtonData data;
  data.touched = button.touched;
  data.pressed = button.pressed;
  data.value = button.value;
  AddOptionalButtonData(data);
}

std::optional<Gamepad> XRStandardGamepadBuilder::GetGamepad() const {
  if (!primary_button_) {
    return std::nullopt;
  }

  GamepadBuilder builder("", GamepadMapping::kXrStandard, handedness_);
  builder.AddButton(primary_button_.value());

  const bool has_optional_buttons = !optional_button_data_.empty();
  if (secondary_button_) {
    builder.AddButton(secondary_button_.value());
  } else if (touchpad_data_ || thumbstick_data_ || has_optional_buttons) {
    builder.AddPlaceholderButton();
  }

  if (touchpad_data_) {
    builder.AddButton(touchpad_data_.value());
  } else if (thumbstick_data_ || has_optional_axes_) {
    builder.AddPlaceholderButton();
    builder.AddPlaceholderAxes();
  } else if (has_optional_buttons) {
    // Only add a placeholder button because there are no more axes.
    builder.AddPlaceholderButton();
  }

  if (thumbstick_data_) {
    builder.AddButton(thumbstick_data_.value());
  } else if (has_optional_axes_) {
    builder.AddPlaceholderButton();
    builder.AddPlaceholderAxes();
  } else if (has_optional_buttons) {
    // Only add a placeholder button because there are no more axes.
    builder.AddPlaceholderButton();
  }

  for (const auto& data : optional_button_data_) {
    builder.AddButton(data);
  }

  return builder.GetGamepad();
}

}  // namespace device
