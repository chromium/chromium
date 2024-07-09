// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_input_helper.h"

#include "device/gamepad/public/cpp/gamepad.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

XrResult OpenXRInputHelper::CreateOpenXRInputHelper(
    XrInstance instance,
    XrSystemId system,
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace local_space,
    bool hand_input_enabled,
    std::unique_ptr<OpenXRInputHelper>* helper) {
  std::unique_ptr<OpenXRInputHelper> new_helper =
      std::make_unique<OpenXRInputHelper>(session, local_space,
                                          hand_input_enabled);

  RETURN_IF_XR_FAILED(
      new_helper->Initialize(instance, system, extension_helper));
  *helper = std::move(new_helper);
  return XR_SUCCESS;
}

OpenXRInputHelper::OpenXRInputHelper(XrSession session,
                                     XrSpace local_space,
                                     bool hand_input_enabled)
    : session_(session),
      local_space_(local_space),
      path_helper_(std::make_unique<OpenXRPathHelper>()),
      hand_input_enabled_(hand_input_enabled) {}

OpenXRInputHelper::~OpenXRInputHelper() = default;

bool OpenXRInputHelper::IsHandTrackingEnabled() const {
  // As long as we have at least one controller that can supply hand tracking
  // data, then hand tracking is enabled.
  return base::ranges::any_of(controller_states_,
                              [](const OpenXrControllerState& state) {
                                return state.controller.IsHandTrackingEnabled();
                              });
}

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
        path_helper_.get(), extension_helper, hand_input_enabled_, &bindings));
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

    std::optional<GamepadButton> menu_button =
        controller->GetButton(OpenXrButtonType::kMenu);

    // Pressing a menu buttons is treated as a signal to exit the WebXR session.
    if (menu_button && menu_button.value().pressed) {
      OnExitGesture();
    }

    std::optional<GamepadButton> primary_button =
        controller->GetButton(OpenXrButtonType::kTrigger);
    std::optional<GamepadButton> squeeze_button =
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

    state->gamepad = controller->GetWebXRGamepad();

    // This will return null if hand tracking isn't possible/enabled.
    state->hand_tracking_data = controller->GetHandTrackingData();

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
  RETURN_IF_XR_FAILED(xrSyncActions(session_, &sync_info));

  for (auto& controller_state : controller_states_) {
    RETURN_IF_XR_FAILED(controller_state.controller.Update(
        local_space_, predicted_display_time));
  }

  return XR_SUCCESS;
}

}  // namespace device
