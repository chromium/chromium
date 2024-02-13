// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_UTIL_XR_STANDARD_GAMEPAD_BUILDER_H_
#define DEVICE_VR_UTIL_XR_STANDARD_GAMEPAD_BUILDER_H_

#include <optional>

#include "base/component_export.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/util/gamepad_builder.h"

namespace device {

// Centralizes the logic of properly ordering the buttons and input axes for
// xr-standard Gamepads so that the various platforms don't have to worry about
// it themselves.
class COMPONENT_EXPORT(DEVICE_VR_UTIL) XRStandardGamepadBuilder {
 public:
  XRStandardGamepadBuilder(device::mojom::XRHandedness handedness);

  XRStandardGamepadBuilder(const XRStandardGamepadBuilder&) = delete;
  XRStandardGamepadBuilder& operator=(const XRStandardGamepadBuilder&) = delete;

  virtual ~XRStandardGamepadBuilder();
  void SetPrimaryButton(const GamepadButton& button) {
    primary_button_ = button;
  }
  void SetPrimaryButton(const GamepadBuilder::ButtonData& data) {
    SetPrimaryButton(GamepadButton(data.pressed, data.touched, data.value));
  }
  void SetSecondaryButton(const GamepadButton& button) {
    secondary_button_ = button;
  }
  void SetSecondaryButton(const GamepadBuilder::ButtonData& data) {
    SetSecondaryButton(GamepadButton(data.pressed, data.touched, data.value));
  }
  void SetTouchpadData(const GamepadBuilder::ButtonData& data) {
    touchpad_data_ = data;
  }
  void SetThumbstickData(const GamepadBuilder::ButtonData& data) {
    thumbstick_data_ = data;
  }
  void AddOptionalButtonData(const GamepadBuilder::ButtonData& data);
  void AddOptionalButtonData(const GamepadButton& button);

  std::optional<Gamepad> GetGamepad() const;

  bool HasSecondaryButton() const { return !!secondary_button_; }
  bool HasTouchpad() const { return !!touchpad_data_; }
  bool HasThumbstick() const { return !!thumbstick_data_; }

 private:
  std::optional<GamepadButton> primary_button_;
  std::optional<GamepadButton> secondary_button_;

  std::optional<GamepadBuilder::ButtonData> touchpad_data_;
  std::optional<GamepadBuilder::ButtonData> thumbstick_data_;

  std::vector<GamepadBuilder::ButtonData> optional_button_data_;

  // Has one or more optional buttons that also have associated axes.
  bool has_optional_axes_ = false;

  device::mojom::XRHandedness handedness_;
};

}  // namespace device

#endif  // DEVICE_VR_UTIL_XR_STANDARD_GAMEPAD_BUILDER_H_
