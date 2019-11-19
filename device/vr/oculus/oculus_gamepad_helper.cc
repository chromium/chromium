// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/oculus/oculus_gamepad_helper.h"

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/util/xr_standard_gamepad_builder.h"
#include "device/vr/vr_device.h"
#include "third_party/libovr/src/Include/OVR_CAPI.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

float ApplyTriggerDeadzone(float value) {
  // Trigger value should be between 0 and 1.  We apply a deadzone for small
  // values so a loose controller still reports a value of 0 when not in use.
  float kTriggerDeadzone = 0.01f;

  return (value < kTriggerDeadzone) ? 0 : value;
}

device::mojom::XRHandedness OculusToMojomHand(ovrHandType hand) {
  switch (hand) {
    case ovrHand_Left:
      return device::mojom::XRHandedness::LEFT;
    case ovrHand_Right:
      return device::mojom::XRHandedness::RIGHT;
    default:
      return device::mojom::XRHandedness::NONE;
  }
}

class OculusGamepadBuilder : public XRStandardGamepadBuilder {
 public:
  OculusGamepadBuilder(ovrInputState state, ovrHandType hand)
      : XRStandardGamepadBuilder(OculusToMojomHand(hand)),
        state_(state),
        ovr_hand_(hand) {
    switch (ovr_hand_) {
      case ovrHand_Left:
        SetPrimaryButton(GetTouchTriggerButton(ovrTouch_LIndexTrigger,
                                               state_.IndexTrigger[ovr_hand_]));
        SetSecondaryButton(GetTriggerButton(state_.HandTrigger[ovr_hand_]));
        SetThumbstickData(GetThumbstickData(ovrButton_LThumb));
        AddOptionalButtonData(GetStandardButton(ovrButton_X));
        AddOptionalButtonData(GetStandardButton(ovrButton_Y));
        AddOptionalButtonData(GetTouchButton(ovrTouch_LThumbRest));
        break;
      case ovrHand_Right:
        SetPrimaryButton(GetTouchTriggerButton(ovrTouch_RIndexTrigger,
                                               state_.IndexTrigger[ovr_hand_]));
        SetSecondaryButton(GetTriggerButton(state_.HandTrigger[ovr_hand_]));
        SetThumbstickData(GetThumbstickData(ovrButton_RThumb));
        AddOptionalButtonData(GetStandardButton(ovrButton_A));
        AddOptionalButtonData(GetStandardButton(ovrButton_B));
        AddOptionalButtonData(GetTouchButton(ovrTouch_RThumbRest));
        break;
      default:
        DLOG(WARNING) << "Unsupported hand configuration.";
    }
  }

  ~OculusGamepadBuilder() override = default;

 private:
  GamepadButton GetStandardButton(ovrButton id) {
    bool pressed = (state_.Buttons & id) != 0;
    bool touched = (state_.Touches & id) != 0;
    double value = pressed ? 1.0 : 0.0;
    return GamepadButton(pressed, touched, value);
  }

  GamepadButton GetTouchButton(ovrTouch id) {
    bool touched = (state_.Touches & id) != 0;
    return GamepadButton(false, touched, 0.0f);
  }

  GamepadButton GetTriggerButton(float value) {
    value = ApplyTriggerDeadzone(value);
    bool pressed = value != 0;
    bool touched = pressed;
    return GamepadButton(pressed, touched, value);
  }

  GamepadButton GetTouchTriggerButton(ovrTouch id, float value) {
    value = ApplyTriggerDeadzone(value);
    bool pressed = value != 0;
    bool touched = (state_.Touches & id) != 0;
    return GamepadButton(pressed, touched, value);
  }

  GamepadBuilder::ButtonData GetThumbstickData(ovrButton id) {
    GamepadButton button = GetStandardButton(id);
    GamepadBuilder::ButtonData data;
    data.touched = button.touched;
    data.pressed = button.pressed;
    data.value = button.value;

    // Invert the y axis because -1 is up in the Gamepad API but down in Oculus.
    data.type = GamepadBuilder::ButtonData::Type::kThumbstick;
    data.x_axis = state_.Thumbstick[ovr_hand_].x;
    data.y_axis = -state_.Thumbstick[ovr_hand_].y;

    return data;
  }

 private:
  ovrInputState state_;
  ovrHandType ovr_hand_;

  DISALLOW_COPY_AND_ASSIGN(OculusGamepadBuilder);
};

}  // namespace

// Order of buttons 1-4 is dictated by the xr-standard Gamepad mapping.
// Buttons 5-7 are in order of decreasing importance.
// 1) index trigger (primary trigger/button)
// 2) hand trigger (secondary trigger/button)
// 3) EMPTY (no touchpad press)
// 4) thumbstick press
// 5) A or X
// 6) B or Y
// 7) thumbrest touch sensor
//
// Order of axes 1-4 is dictated by the xr-standard Gamepad mapping.
// 1) EMPTY (no touchpad)
// 2) EMPTY (no touchpad)
// 3) thumbstick X
// 4) thumbstick Y
base::Optional<Gamepad> OculusGamepadHelper::CreateGamepad(ovrSession session,
                                                           ovrHandType hand) {
  ovrInputState input_touch;
  bool have_touch = OVR_SUCCESS(
      ovr_GetInputState(session, ovrControllerType_Touch, &input_touch));
  if (!have_touch) {
    return base::nullopt;
  }

  OculusGamepadBuilder touch(input_touch, hand);
  return touch.GetGamepad();
}

}  // namespace device
