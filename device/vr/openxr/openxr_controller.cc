// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_controller.h"

#include <stdint.h>

#include "base/logging.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/util/xr_standard_gamepad_builder.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/transform_util.h"

namespace device {

namespace {

constexpr char kMicrosoftMotionControllerInteractionProfile[] =
    "/interaction_profiles/microsoft/motion_controller";

const static std::map<std::string, std::vector<std::string>> profile_map = {
    {kMicrosoftMotionControllerInteractionProfile,
     {"windows-mixed-reality", "generic-trigger-squeeze-touchpad-thumbstick"}}};

const char* GetStringFromType(OpenXrHandednessType type) {
  switch (type) {
    case OpenXrHandednessType::kLeft:
      return "left";
    case OpenXrHandednessType::kRight:
      return "right";
    case OpenXrHandednessType::kCount:
      NOTREACHED();
      return "";
  }
}

}  // namespace

OpenXrController::OpenXrController()
    : description_(nullptr),
      type_(OpenXrHandednessType::kCount),  // COUNT refers to invalid.
      instance_(XR_NULL_HANDLE),
      session_(XR_NULL_HANDLE),
      action_set_(XR_NULL_HANDLE),
      grip_pose_action_{XR_NULL_HANDLE},
      grip_pose_space_(XR_NULL_HANDLE) {}

OpenXrController::~OpenXrController() {
  // We don't need to destroy all of the actions because destroying an
  // action set destroys all actions that are part of the set.

  if (action_set_ != XR_NULL_HANDLE) {
    xrDestroyActionSet(action_set_);
  }
  if (grip_pose_space_ != XR_NULL_HANDLE) {
    xrDestroySpace(grip_pose_space_);
  }
}

XrResult OpenXrController::Initialize(
    OpenXrHandednessType type,
    XrInstance instance,
    XrSession session,
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) {
  type_ = type;
  instance_ = instance;
  session_ = session;
  std::string handedness_string = GetStringFromType(type_);

  top_level_user_path_string_ = std::string("/user/hand/") + handedness_string;

  std::string action_set_name = handedness_string + "_action_set";
  XrActionSetCreateInfo action_set_create_info = {
      XR_TYPE_ACTION_SET_CREATE_INFO};

  errno_t error =
      strcpy_s(action_set_create_info.actionSetName, action_set_name.c_str());
  DCHECK(!error);
  error = strcpy_s(action_set_create_info.localizedActionSetName,
                   action_set_name.c_str());
  DCHECK(!error);

  XrResult xr_result;

  RETURN_IF_XR_FAILED(
      xrCreateActionSet(instance_, &action_set_create_info, &action_set_));

  RETURN_IF_XR_FAILED(
      InitializeMicrosoftMotionControllerActions(handedness_string, bindings));

  RETURN_IF_XR_FAILED(InitializeMicrosoftMotionControllerSpaces());

  return xr_result;
}

XrResult OpenXrController::InitializeMicrosoftMotionControllerActions(
    const std::string& type_string,
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) {
  XrResult xr_result;

  const std::string binding_prefix = top_level_user_path_string_ + "/input/";
  const std::string name_prefix = type_string + "_controller_";

  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_BOOLEAN_INPUT,
      kMicrosoftMotionControllerInteractionProfile,
      binding_prefix + "trigger/value", name_prefix + "trigger_button_press",
      &(button_action_map_[OpenXrButtonType::kTrigger].press_action),
      bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_FLOAT_INPUT, kMicrosoftMotionControllerInteractionProfile,
      binding_prefix + "trigger/value", name_prefix + "trigger_button_value",
      &(button_action_map_[OpenXrButtonType::kTrigger].value_action),
      bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_BOOLEAN_INPUT,
      kMicrosoftMotionControllerInteractionProfile,
      binding_prefix + "thumbstick/click",
      name_prefix + "thumbstick_button_press",
      &(button_action_map_[OpenXrButtonType::kThumbstick].press_action),
      bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_BOOLEAN_INPUT,
      kMicrosoftMotionControllerInteractionProfile,
      binding_prefix + "squeeze/click", name_prefix + "squeeze_button_press",
      &(button_action_map_[OpenXrButtonType::kSqueeze].press_action),
      bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_BOOLEAN_INPUT,
      kMicrosoftMotionControllerInteractionProfile,
      binding_prefix + "trackpad/click", name_prefix + "trackpad_button_press",
      &(button_action_map_[OpenXrButtonType::kTrackpad].press_action),
      bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_BOOLEAN_INPUT,
      kMicrosoftMotionControllerInteractionProfile,
      binding_prefix + "trackpad/touch", name_prefix + "trackpad_button_touch",
      &(button_action_map_[OpenXrButtonType::kTrackpad].touch_action),
      bindings));

  RETURN_IF_XR_FAILED(
      CreateAction(XR_ACTION_TYPE_VECTOR2F_INPUT,
                   kMicrosoftMotionControllerInteractionProfile,
                   binding_prefix + "trackpad", name_prefix + "trackpad_axis",
                   &(axis_action_map_[OpenXrAxisType::kTrackpad]), bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_VECTOR2F_INPUT,
      kMicrosoftMotionControllerInteractionProfile,
      binding_prefix + "thumbstick", name_prefix + "thumbstick_axis",
      &(axis_action_map_[OpenXrAxisType::kThumbstick]), bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_POSE_INPUT, kMicrosoftMotionControllerInteractionProfile,
      binding_prefix + "grip", name_prefix + "grip_pose", &grip_pose_action_,
      bindings));

  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_POSE_INPUT, kMicrosoftMotionControllerInteractionProfile,
      binding_prefix + "aim", name_prefix + "aim_pose", &pointer_pose_action_,
      bindings));

  return xr_result;
}

XrResult OpenXrController::InitializeMicrosoftMotionControllerSpaces() {
  XrResult xr_result;

  RETURN_IF_XR_FAILED(CreateActionSpace(grip_pose_action_, &grip_pose_space_));

  RETURN_IF_XR_FAILED(
      CreateActionSpace(pointer_pose_action_, &pointer_pose_space_));

  return xr_result;
}

uint32_t OpenXrController::GetId() const {
  return static_cast<uint32_t>(type_);
}

device::mojom::XRHandedness OpenXrController::GetHandness() const {
  switch (type_) {
    case OpenXrHandednessType::kLeft:
      return device::mojom::XRHandedness::LEFT;
    case OpenXrHandednessType::kRight:
      return device::mojom::XRHandedness::RIGHT;
    case OpenXrHandednessType::kCount:
      // LEFT controller and RIGHT controller are currently the only supported
      // controllers. In the future, other controllers such as sound (which
      // does not have a handedness) will be added here.
      NOTREACHED();
      return device::mojom::XRHandedness::NONE;
  }
}

mojom::XRInputSourceDescriptionPtr OpenXrController::GetDescription(
    XrTime predicted_display_time) {
  // Description only need to be set once unless interaction profiles changes.
  if (!description_) {
    // UpdateInteractionProfile() can not be called inside Initialize() function
    // because XrGetCurrentInteractionProfile can't be called before
    // xrSuggestInteractionProfileBindings getting called.
    if (XR_FAILED(UpdateInteractionProfile())) {
      return nullptr;
    }
    description_ = device::mojom::XRInputSourceDescription::New();
    description_->handedness = GetHandness();
    description_->target_ray_mode = device::mojom::XRTargetRayMode::POINTING;
    description_->profiles = GetProfiles();
  }

  if (!description_->input_from_pointer) {
    description_->input_from_pointer =
        GetPointerFromGripTransform(predicted_display_time);
  }

  return description_.Clone();
}

base::Optional<GamepadButton> OpenXrController::GetButton(
    OpenXrButtonType type) const {
  GamepadButton ret;

  DCHECK(button_action_map_.count(type) == 1);

  XrActionStateBoolean press_state_bool = {XR_TYPE_ACTION_STATE_BOOLEAN};
  if (XR_FAILED(QueryState(button_action_map_.at(type).press_action,
                           &press_state_bool)) ||
      !press_state_bool.isActive) {
    return base::nullopt;
  }
  ret.pressed = press_state_bool.currentState;

  XrActionStateBoolean touch_state_bool = {XR_TYPE_ACTION_STATE_BOOLEAN};
  if (button_action_map_.at(type).touch_action != XR_NULL_HANDLE &&
      XR_SUCCEEDED(QueryState(button_action_map_.at(type).touch_action,
                              &touch_state_bool)) &&
      touch_state_bool.isActive) {
    ret.touched = touch_state_bool.currentState;
  } else {
    ret.touched = ret.pressed;
  }

  XrActionStateFloat value_state_float = {XR_TYPE_ACTION_STATE_FLOAT};
  if (button_action_map_.at(type).value_action != XR_NULL_HANDLE &&
      XR_SUCCEEDED(QueryState(button_action_map_.at(type).value_action,
                              &value_state_float)) &&
      value_state_float.isActive) {
    ret.value = value_state_float.currentState;
  } else {
    ret.value = ret.pressed ? 1.0 : 0.0;
  }

  return ret;
}

std::vector<double> OpenXrController::GetAxis(OpenXrAxisType type) const {
  XrActionStateVector2f axis_state_v2f = {XR_TYPE_ACTION_STATE_VECTOR2F};
  if (XR_FAILED(QueryState(axis_action_map_.at(type), &axis_state_v2f)) ||
      !axis_state_v2f.isActive) {
    return {};
  }

  return {axis_state_v2f.currentState.x, axis_state_v2f.currentState.y};
}

XrResult OpenXrController::UpdateInteractionProfile() {
  XrResult xr_result;

  XrPath top_level_user_path;
  RETURN_IF_XR_FAILED(xrStringToPath(
      instance_, top_level_user_path_string_.c_str(), &top_level_user_path));

  XrInteractionProfileState interaction_profile_state = {
      XR_TYPE_INTERACTION_PROFILE_STATE};
  RETURN_IF_XR_FAILED(xrGetCurrentInteractionProfile(
      session_, top_level_user_path, &interaction_profile_state));

  uint32_t output_size;
  RETURN_IF_XR_FAILED(
      xrPathToString(instance_, interaction_profile_state.interactionProfile, 0,
                     &output_size, nullptr));

  char out_string[output_size];
  RETURN_IF_XR_FAILED(
      xrPathToString(instance_, interaction_profile_state.interactionProfile,
                     output_size, &output_size, out_string));

  interaction_profile_ = out_string;
  return xr_result;
}

base::Optional<gfx::Transform> OpenXrController::GetMojoFromGripTransform(
    XrTime predicted_display_time,
    XrSpace local_space) const {
  return GetTransformFromSpaces(predicted_display_time, grip_pose_space_,
                                local_space);
}

base::Optional<gfx::Transform> OpenXrController::GetPointerFromGripTransform(
    XrTime predicted_display_time) const {
  return GetTransformFromSpaces(predicted_display_time, pointer_pose_space_,
                                grip_pose_space_);
}

base::Optional<gfx::Transform> OpenXrController::GetTransformFromSpaces(
    XrTime predicted_display_time,
    XrSpace target,
    XrSpace origin) const {
  XrSpaceLocation location = {XR_TYPE_SPACE_LOCATION};
  if (FAILED(
          xrLocateSpace(target, origin, predicted_display_time, &location)) ||
      !(location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) ||
      !(location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
    return base::nullopt;
  }

  // Convert the orientation and translation given by runtime into a
  // transformation matrix.
  gfx::DecomposedTransform decomp;
  decomp.quaternion =
      gfx::Quaternion(location.pose.orientation.x, location.pose.orientation.y,
                      location.pose.orientation.z, location.pose.orientation.w);
  decomp.translate[0] = location.pose.position.x;
  decomp.translate[1] = location.pose.position.y;
  decomp.translate[2] = location.pose.position.z;

  return gfx::ComposeTransform(decomp);
}

std::vector<std::string> OpenXrController::GetProfiles() const {
  // TODO(crbug.com/1006072):
  // Query  USB vendor and product ID From OpenXR.
  DCHECK(profile_map.count(interaction_profile_) == 1);
  return profile_map.at(interaction_profile_);
}

XrActionSet OpenXrController::GetActionSet() const {
  return action_set_;
}

XrResult OpenXrController::CreateAction(
    XrActionType type,
    const char* interaction_profile_name,
    const std::string& binding_string,
    const std::string& action_name,
    XrAction* action,
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) {
  XrResult xr_result;

  XrActionCreateInfo action_create_info = {XR_TYPE_ACTION_CREATE_INFO};
  action_create_info.actionType = type;

  errno_t error = strcpy_s(action_create_info.actionName, action_name.data());
  DCHECK(error == 0);
  error = strcpy_s(action_create_info.localizedActionName, action_name.data());
  DCHECK(error == 0);

  RETURN_IF_XR_FAILED(xrCreateAction(action_set_, &action_create_info, action));

  XrPath profile_path, action_path;
  RETURN_IF_XR_FAILED(
      xrStringToPath(instance_, interaction_profile_name, &profile_path));
  RETURN_IF_XR_FAILED(
      xrStringToPath(instance_, binding_string.c_str(), &action_path));
  (*bindings)[profile_path].push_back({*action, action_path});

  return xr_result;
}

XrResult OpenXrController::CreateActionSpace(XrAction action, XrSpace* space) {
  XrActionSpaceCreateInfo action_space_create_info = {};
  action_space_create_info.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
  action_space_create_info.action = action;
  action_space_create_info.subactionPath = XR_NULL_PATH;
  action_space_create_info.poseInActionSpace = PoseIdentity();
  return xrCreateActionSpace(session_, &action_space_create_info, space);
}

}  // namespace device
