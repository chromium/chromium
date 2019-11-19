// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openvr/openvr_gamepad_helper.h"
#include "device/vr/openvr/openvr_api_wrapper.h"

#include <memory>
#include <unordered_set>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/util/gamepad_builder.h"
#include "device/vr/util/xr_standard_gamepad_builder.h"
#include "device/vr/vr_device.h"
#include "third_party/openvr/src/headers/openvr.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

constexpr double kJoystickDeadzone = 0.16;

bool TryGetGamepadButton(const vr::VRControllerState_t& controller_state,
                         uint64_t supported_buttons,
                         vr::EVRButtonId button_id,
                         GamepadButton* button) {
  uint64_t button_mask = vr::ButtonMaskFromId(button_id);
  if ((supported_buttons & button_mask) != 0) {
    bool button_pressed = (controller_state.ulButtonPressed & button_mask) != 0;
    bool button_touched = (controller_state.ulButtonTouched & button_mask) != 0;
    button->touched = button_touched || button_pressed;
    button->pressed = button_pressed;
    button->value = button_pressed ? 1.0 : 0.0;
    return true;
  }

  return false;
}

vr::EVRButtonId GetAxisId(uint32_t axis_offset) {
  return static_cast<vr::EVRButtonId>(vr::k_EButton_Axis0 + axis_offset);
}

std::map<vr::EVRButtonId, GamepadBuilder::ButtonData> GetAxesButtons(
    vr::IVRSystem* vr_system,
    const vr::VRControllerState_t& controller_state,
    uint64_t supported_buttons,
    uint32_t controller_id) {
  std::map<vr::EVRButtonId, GamepadBuilder::ButtonData> button_data_map;

  for (uint32_t j = 0; j < vr::k_unControllerStateAxisCount; ++j) {
    int32_t axis_type = vr_system->GetInt32TrackedDeviceProperty(
        controller_id,
        static_cast<vr::TrackedDeviceProperty>(vr::Prop_Axis0Type_Int32 + j));

    GamepadBuilder::ButtonData button_data;

    // Invert the y axis because -1 is up in the Gamepad API but down in OpenVR.
    double x_axis = controller_state.rAxis[j].x;
    double y_axis = -controller_state.rAxis[j].y;

    if (axis_type == vr::k_eControllerAxis_Joystick) {
      button_data.type = GamepadBuilder::ButtonData::Type::kThumbstick;

      // We only want to apply the deadzone to joysticks, since various
      // runtimes may not have already done that, but touchpads should
      // be fine.
      x_axis = std::fabs(x_axis) < kJoystickDeadzone ? 0 : x_axis;
      y_axis = std::fabs(y_axis) < kJoystickDeadzone ? 0 : y_axis;
    } else if (axis_type == vr::k_eControllerAxis_TrackPad) {
      button_data.type = GamepadBuilder::ButtonData::Type::kTouchpad;
    }

    switch (axis_type) {
      case vr::k_eControllerAxis_Joystick:
      case vr::k_eControllerAxis_TrackPad: {
        button_data.x_axis = x_axis;
        button_data.y_axis = y_axis;
        vr::EVRButtonId button_id = GetAxisId(j);

        // Even if the button associated with the axis isn't supported, if we
        // have valid axis data, we should still send that up.  Since the spec
        // expects buttons with axes, then we will add a dummy button to match
        // the axes.
        GamepadButton button;
        if (TryGetGamepadButton(controller_state, supported_buttons, button_id,
                                &button)) {
          button_data.touched = button.touched;
          button_data.pressed = button.pressed;
          button_data.value = button.value;
        } else {
          button_data.pressed = false;
          button_data.value = 0.0;
          button_data.touched =
              (std::fabs(x_axis) > 0 || std::fabs(y_axis) > 0);
        }

        button_data_map[button_id] = button_data;
      } break;
      case vr::k_eControllerAxis_Trigger: {
        GamepadButton button;
        GamepadBuilder::ButtonData button_data;
        vr::EVRButtonId button_id = GetAxisId(j);
        if (TryGetGamepadButton(controller_state, supported_buttons, button_id,
                                &button)) {
          button_data.touched = button.touched;
          button_data.pressed = button.pressed;
          button_data.value = x_axis;
          button_data_map[button_id] = button_data;
        }
      } break;
    }
  }

  return button_data_map;
}

constexpr std::array<vr::EVRButtonId, 5> kWebXRButtonOrder = {
    vr::k_EButton_A,          vr::k_EButton_DPad_Left, vr::k_EButton_DPad_Up,
    vr::k_EButton_DPad_Right, vr::k_EButton_DPad_Down,
};

// To make sure this string fits the requirements of the WebXR spec, separate
// words/tokens are separated by "-" instead of whitespace and convert it to
// lowercase.
std::string FixupProfileString(const std::string& name) {
  std::vector<std::string> tokens =
      base::SplitString(name, base::kWhitespaceASCII, base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  std::string result = base::JoinString(tokens, "-");
  return base::ToLowerASCII(result);
}

}  // namespace

// Helper classes and WebXR Getters
class OpenVRGamepadBuilder : public XRStandardGamepadBuilder {
 public:
  enum class AxesRequirement {
    kOptional = 0,
    kRequireBoth = 1,
  };

  OpenVRGamepadBuilder(vr::IVRSystem* vr_system,
                       uint32_t controller_id,
                       vr::VRControllerState_t controller_state,
                       mojom::XRHandedness handedness)
      : XRStandardGamepadBuilder(handedness),
        controller_state_(controller_state) {
    supported_buttons_ = vr_system->GetUint64TrackedDeviceProperty(
        controller_id, vr::Prop_SupportedButtons_Uint64);

    axes_data_ = GetAxesButtons(vr_system, controller_state_,
                                supported_buttons_, controller_id);

    base::Optional<GamepadBuilder::ButtonData> primary_button =
        TryGetAxesOrTriggerButton(vr::k_EButton_SteamVR_Trigger);

    if (!primary_button) {
      return;
    }

    SetPrimaryButton(primary_button.value());

    base::Optional<GamepadButton> secondary_button =
        TryGetButton(vr::k_EButton_Grip);
    if (secondary_button) {
      SetSecondaryButton(secondary_button.value());
    }

    base::Optional<GamepadBuilder::ButtonData> touchpad_data =
        TryGetNextUnusedButtonOfType(
            GamepadBuilder::ButtonData::Type::kTouchpad);
    if (touchpad_data) {
      SetTouchpadData(touchpad_data.value());
    }

    base::Optional<GamepadBuilder::ButtonData> thumbstick_data =
        TryGetNextUnusedButtonOfType(
            GamepadBuilder::ButtonData::Type::kThumbstick);
    if (thumbstick_data) {
      SetThumbstickData(thumbstick_data.value());
    }

    // Now that all of the xr-standard reserved buttons have been filled in, we
    // add the rest of the buttons in order of decreasing importance.
    // First add regular buttons.
    for (const auto& id : kWebXRButtonOrder) {
      base::Optional<GamepadButton> button = TryGetButton(id);
      if (button) {
        AddOptionalButtonData(button.value());
      }
    }

    // Finally, add any remaining axis buttons (triggers/josysticks/touchpads)
    AddRemainingTriggersAndAxes();

    // Find out the model and manufacturer names in case the caller wants this
    // information for the input profiles array.
    std::string model =
        GetOpenVRString(vr_system, vr::Prop_ModelNumber_String, controller_id);
    std::string manufacturer = GetOpenVRString(
        vr_system, vr::Prop_ManufacturerName_String, controller_id);

    UpdateProfiles(manufacturer, model);
  }

  ~OpenVRGamepadBuilder() override = default;

  std::vector<std::string> GetProfiles() const { return profiles_; }

 private:
  void UpdateProfiles(const std::string& manufacturer,
                      const std::string& model) {
    // Per the WebXR spec, the first entry in the profiles array should be the
    // most specific one.
    std::string name =
        FixupProfileString(manufacturer) + "-" + FixupProfileString(model);
    profiles_.push_back(name);

    // Also record information about what this controller actually does in a
    // more general sense. The controller is guaranteed to at least have a
    // trigger if we get here.
    std::string capabilities = "generic-trigger";
    if (HasSecondaryButton()) {
      capabilities += "-squeeze";
    }
    if (HasTouchpad()) {
      capabilities += "-touchpad";
    }
    if (HasThumbstick()) {
      capabilities += "-thumbstick";
    }
    profiles_.push_back(capabilities);
  }

  base::Optional<GamepadBuilder::ButtonData> TryGetAxesOrTriggerButton(
      vr::EVRButtonId button_id,
      AxesRequirement requirement = AxesRequirement::kOptional) {
    if (!IsInAxesData(button_id))
      return base::nullopt;

    bool require_axes = (requirement == AxesRequirement::kRequireBoth);
    if (require_axes &&
        axes_data_[button_id].type == GamepadBuilder::ButtonData::Type::kButton)
      return base::nullopt;

    used_axes_.insert(button_id);
    return axes_data_[button_id];
  }

  base::Optional<GamepadBuilder::ButtonData> TryGetNextUnusedButtonOfType(
      GamepadBuilder::ButtonData::Type type) {
    for (const auto& axes_data_pair : axes_data_) {
      vr::EVRButtonId button_id = axes_data_pair.first;
      if (IsUsed(button_id))
        continue;

      if (axes_data_pair.second.type != type)
        continue;

      return TryGetAxesOrTriggerButton(button_id,
                                       AxesRequirement::kRequireBoth);
    }

    return base::nullopt;
  }

  base::Optional<GamepadButton> TryGetButton(vr::EVRButtonId button_id) {
    GamepadButton button;
    if (TryGetGamepadButton(controller_state_, supported_buttons_, button_id,
                            &button)) {
      return button;
    }

    return base::nullopt;
  }

  // This will add any remaining unused values from axes_data to the gamepad.
  // Returns a bool indicating whether any additional axes were added.
  void AddRemainingTriggersAndAxes() {
    for (const auto& axes_data_pair : axes_data_) {
      if (!IsUsed(axes_data_pair.first)) {
        AddOptionalButtonData(axes_data_pair.second);
      }
    }
  }

  bool IsUsed(vr::EVRButtonId button_id) {
    auto it = used_axes_.find(button_id);
    return it != used_axes_.end();
  }

  bool IsInAxesData(vr::EVRButtonId button_id) {
    auto it = axes_data_.find(button_id);
    return it != axes_data_.end();
  }

  const vr::VRControllerState_t controller_state_;
  uint64_t supported_buttons_;
  std::map<vr::EVRButtonId, GamepadBuilder::ButtonData> axes_data_;
  std::unordered_set<vr::EVRButtonId> used_axes_;
  std::vector<std::string> profiles_;

  DISALLOW_COPY_AND_ASSIGN(OpenVRGamepadBuilder);
};

OpenVRInputSourceData::OpenVRInputSourceData() = default;
OpenVRInputSourceData::~OpenVRInputSourceData() = default;
OpenVRInputSourceData::OpenVRInputSourceData(
    const OpenVRInputSourceData& other) = default;

OpenVRInputSourceData OpenVRGamepadHelper::GetXRInputSourceData(
    vr::IVRSystem* vr_system,
    uint32_t controller_id,
    vr::VRControllerState_t controller_state,
    mojom::XRHandedness handedness) {
  OpenVRGamepadBuilder builder(vr_system, controller_id, controller_state,
                               handedness);

  OpenVRInputSourceData data;
  data.gamepad = builder.GetGamepad();
  data.profiles = builder.GetProfiles();
  return data;
}

}  // namespace device
