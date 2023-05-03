// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_input_helper.h"

#include "device/gamepad/public/cpp/gamepad.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/util/xr_standard_gamepad_builder.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

namespace {
absl::optional<GamepadBuilder::ButtonData> GetAxisButtonData(
    OpenXrAxisType openxr_button_type,
    absl::optional<GamepadButton> button_data,
    std::vector<double> axis) {
  GamepadBuilder::ButtonData data;
  if (!button_data || axis.size() != 2) {
    return absl::nullopt;
  }

  switch (openxr_button_type) {
    case OpenXrAxisType::kThumbstick:
      data.type = GamepadBuilder::ButtonData::Type::kThumbstick;
      break;
    case OpenXrAxisType::kTrackpad:
      data.type = GamepadBuilder::ButtonData::Type::kTouchpad;
      break;
  }
  data.touched = button_data->touched;
  data.pressed = button_data->pressed;
  data.value = button_data->value;
  // Invert the y axis because -1 is up in the Gamepad API, but down in
  // OpenXR.
  data.x_axis = axis.at(0);
  data.y_axis = -axis.at(1);
  return data;
}

absl::optional<Gamepad> GetXrStandardGamepad(
    const OpenXrController& controller) {
  XRStandardGamepadBuilder builder(controller.GetHandness());

  absl::optional<GamepadButton> trigger_button =
      controller.GetButton(OpenXrButtonType::kTrigger);
  if (!trigger_button)
    return absl::nullopt;
  builder.SetPrimaryButton(trigger_button.value());

  absl::optional<GamepadButton> squeeze_button =
      controller.GetButton(OpenXrButtonType::kSqueeze);
  if (squeeze_button)
    builder.SetSecondaryButton(squeeze_button.value());

  absl::optional<GamepadButton> trackpad_button =
      controller.GetButton(OpenXrButtonType::kTrackpad);
  std::vector<double> trackpad_axis =
      controller.GetAxis(OpenXrAxisType::kTrackpad);
  absl::optional<GamepadBuilder::ButtonData> trackpad_button_data =
      GetAxisButtonData(OpenXrAxisType::kTrackpad, trackpad_button,
                        trackpad_axis);
  if (trackpad_button_data)
    builder.SetTouchpadData(trackpad_button_data.value());

  absl::optional<GamepadButton> thumbstick_button =
      controller.GetButton(OpenXrButtonType::kThumbstick);
  std::vector<double> thumbstick_axis =
      controller.GetAxis(OpenXrAxisType::kThumbstick);
  absl::optional<GamepadBuilder::ButtonData> thumbstick_button_data =
      GetAxisButtonData(OpenXrAxisType::kThumbstick, thumbstick_button,
                        thumbstick_axis);
  if (thumbstick_button_data)
    builder.SetThumbstickData(thumbstick_button_data.value());

  absl::optional<GamepadButton> x_button =
      controller.GetButton(OpenXrButtonType::kButton1);
  if (x_button)
    builder.AddOptionalButtonData(x_button.value());

  absl::optional<GamepadButton> y_button =
      controller.GetButton(OpenXrButtonType::kButton2);
  if (y_button)
    builder.AddOptionalButtonData(y_button.value());

  absl::optional<GamepadButton> thumbrest_button =
      controller.GetButton(OpenXrButtonType::kThumbrest);
  if (thumbrest_button)
    builder.AddOptionalButtonData(thumbrest_button.value());

  absl::optional<GamepadButton> grasp_button =
      controller.GetButton(OpenXrButtonType::kGrasp);
  if (grasp_button)
    builder.AddOptionalButtonData(grasp_button.value());

  absl::optional<GamepadButton> shoulder_button =
      controller.GetButton(OpenXrButtonType::kShoulder);
  if (shoulder_button)
    builder.AddOptionalButtonData(shoulder_button.value());

  return builder.GetGamepad();
}

}  // namespace

XrResult OpenXRInputHelper::CreateOpenXRInputHelper(
    XrInstance instance,
    XrSystemId system,
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace local_space,
    std::unique_ptr<OpenXRInputHelper>* helper) {
  std::unique_ptr<OpenXRInputHelper> new_helper =
      std::make_unique<OpenXRInputHelper>(session, local_space);

  RETURN_IF_XR_FAILED(
      new_helper->Initialize(instance, system, extension_helper));
  *helper = std::move(new_helper);
  return XR_SUCCESS;
}

OpenXRInputHelper::OpenXRInputHelper(XrSession session, XrSpace local_space)
    : session_(session),
      local_space_(local_space),
      path_helper_(std::make_unique<OpenXRPathHelper>()) {}

OpenXRInputHelper::~OpenXRInputHelper() = default;

XrResult OpenXRInputHelper::Initialize(
    XrInstance instance,
    XrSystemId system,
    const OpenXrExtensionHelper& extension_helper) {
  RETURN_IF_XR_FAILED(path_helper_->Initialize(instance, system));

  // This map is used to store bindings for different kinds of interaction
  // profiles. This allows the runtime to choose a different input sources based
  // on availability.
  std::map<XrPath, std::vector<XrActionSuggestedBinding>> bindings;

  for (size_t i = 0; i < controller_states_.size(); i++) {
    RETURN_IF_XR_FAILED(controller_states_[i].controller.Initialize(
        static_cast<OpenXrHandednessType>(i), instance, session_,
        path_helper_.get(), extension_helper, &bindings));
    controller_states_[i].primary_button_pressed = false;
    controller_states_[i].squeeze_button_pressed = false;
  }

  for (auto it = bindings.begin(); it != bindings.end(); it++) {
    XrInteractionProfileSuggestedBinding profile_suggested_bindings = {
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    profile_suggested_bindings.interactionProfile = it->first;
    profile_suggested_bindings.suggestedBindings = it->second.data();
    profile_suggested_bindings.countSuggestedBindings = it->second.size();

    RETURN_IF_XR_FAILED(xrSuggestInteractionProfileBindings(
        instance, &profile_suggested_bindings));
  }

  std::vector<XrActionSet> action_sets(controller_states_.size());
  for (size_t i = 0; i < controller_states_.size(); i++) {
    action_sets[i] = controller_states_[i].controller.action_set();
  }

  XrSessionActionSetsAttachInfo attach_info = {
      XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
  attach_info.countActionSets = action_sets.size();
  attach_info.actionSets = action_sets.data();
  RETURN_IF_XR_FAILED(xrAttachSessionActionSets(session_, &attach_info));

  return XR_SUCCESS;
}

std::vector<mojom::XRInputSourceStatePtr> OpenXRInputHelper::GetInputState(
    bool hand_input_enabled,
    XrTime predicted_display_time) {
  std::vector<mojom::XRInputSourceStatePtr> input_states;
  if (XR_FAILED(SyncActions(predicted_display_time))) {
    for (OpenXrControllerState& state : controller_states_) {
      state.primary_button_pressed = false;
      state.squeeze_button_pressed = false;
    }
    return input_states;
  }

  for (uint32_t i = 0; i < controller_states_.size(); i++) {
    device::OpenXrController* controller = &controller_states_[i].controller;

    absl::optional<GamepadButton> primary_button =
        controller->GetButton(OpenXrButtonType::kTrigger);
    absl::optional<GamepadButton> squeeze_button =
        controller->GetButton(OpenXrButtonType::kSqueeze);

    // Having a trigger button is the minimum for an webxr input.
    // No trigger button indicates input is not connected.
    if (!primary_button) {
      continue;
    }

    device::mojom::XRInputSourceStatePtr state =
        device::mojom::XRInputSourceState::New();

    // ID 0 will cause a DCHECK in the hash table used on the blink side.
    // To ensure that we don't have any collisions with other ids, increment
    // all of the ids by one.
    state->source_id = i + 1;
    state->description = controller->GetDescription(predicted_display_time);
    if (!state->description) {
      continue;
    }

    state->mojo_from_input = controller->GetMojoFromGripTransform(
        predicted_display_time, local_space_, &state->emulated_position);
    state->primary_input_pressed = primary_button.value().pressed;
    state->primary_input_clicked =
        controller_states_[i].primary_button_pressed &&
        !state->primary_input_pressed;
    controller_states_[i].primary_button_pressed = state->primary_input_pressed;
    if (squeeze_button) {
      state->primary_squeeze_pressed = squeeze_button.value().pressed;
      state->primary_squeeze_clicked =
          controller_states_[i].squeeze_button_pressed &&
          !state->primary_squeeze_pressed;
      controller_states_[i].squeeze_button_pressed =
          state->primary_squeeze_pressed;
    }

    state->gamepad = GetWebXRGamepad(*controller);

    // Return hand state if controller is a hand and the hand tracking feature
    // was requested for the session
    if (hand_input_enabled) {
      state->hand_tracking_data =
          controller->GetHandTrackingData(local_space_, predicted_display_time);
    }

    input_states.push_back(std::move(state));
  }

  return input_states;
}

XrResult OpenXRInputHelper::OnInteractionProfileChanged() {
  for (OpenXrControllerState& controller_state : controller_states_) {
    RETURN_IF_XR_FAILED(controller_state.controller.UpdateInteractionProfile());
  }
  return XR_SUCCESS;
}

absl::optional<Gamepad> OpenXRInputHelper ::GetWebXRGamepad(
    const OpenXrController& controller) {
  OpenXrInteractionProfileType cur_type = controller.interaction_profile();
  for (auto& it : GetOpenXrControllerInteractionProfiles()) {
    if (it.type == cur_type) {
      if (it.mapping == GamepadMapping::kXrStandard) {
        return GetXrStandardGamepad(controller);
      } else {
        // if mapping is kNone
        return absl::nullopt;
      }
    }
  }

  return absl::nullopt;
}

XrResult OpenXRInputHelper::SyncActions(XrTime predicted_display_time) {
  std::vector<XrActiveActionSet> active_action_sets(controller_states_.size());

  for (size_t i = 0; i < controller_states_.size(); i++) {
    active_action_sets[i].actionSet =
        controller_states_[i].controller.action_set();
    active_action_sets[i].subactionPath = XR_NULL_PATH;
  }

  XrActionsSyncInfo sync_info = {XR_TYPE_ACTIONS_SYNC_INFO};
  sync_info.countActiveActionSets = active_action_sets.size();
  sync_info.activeActionSets = active_action_sets.data();
  return xrSyncActions(session_, &sync_info);
}

}  // namespace device
