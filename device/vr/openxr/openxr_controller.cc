// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_controller.h"

#include <stdint.h>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/util/xr_standard_gamepad_builder.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

namespace {

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

std::string GetTopLevelUserPath(OpenXrHandednessType type) {
  return std::string("/user/hand/") + GetStringFromType(type);
}

static constexpr mojom::XRHandJoint OpenXRHandJointToMojomJoint(
    XrHandJointEXT openxr_joint) {
  DCHECK_NE(openxr_joint, XR_HAND_JOINT_PALM_EXT);
  // The OpenXR joints have palm at 0, but from that point are the same as the
  // mojom joints. Hence they are offset by 1.
  return static_cast<mojom::XRHandJoint>(openxr_joint - 1);
}

// Enforce that the conversion is correct at compilation time.
// The mojom hand joints must match the WebXR spec. If these are ever out of
// sync, this mapping will need to be updated.
static_assert(mojom::XRHandJoint::kWrist ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_WRIST_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kThumbMetacarpal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_THUMB_METACARPAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kThumbPhalanxProximal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_THUMB_PROXIMAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kThumbPhalanxDistal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_THUMB_DISTAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kThumbTip ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_THUMB_TIP_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kIndexFingerMetacarpal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_INDEX_METACARPAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kIndexFingerPhalanxProximal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_INDEX_PROXIMAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kIndexFingerPhalanxIntermediate ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_INDEX_INTERMEDIATE_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kIndexFingerPhalanxDistal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_INDEX_DISTAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kIndexFingerTip ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_INDEX_TIP_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kMiddleFingerMetacarpal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_MIDDLE_METACARPAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kMiddleFingerPhalanxProximal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_MIDDLE_PROXIMAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kMiddleFingerPhalanxIntermediate ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_MIDDLE_INTERMEDIATE_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kMiddleFingerPhalanxDistal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_MIDDLE_DISTAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kMiddleFingerTip ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_MIDDLE_TIP_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kRingFingerMetacarpal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_RING_METACARPAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kRingFingerPhalanxProximal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_RING_PROXIMAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kRingFingerPhalanxIntermediate ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_RING_INTERMEDIATE_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kRingFingerPhalanxDistal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_RING_DISTAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kRingFingerTip ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_RING_TIP_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kPinkyFingerMetacarpal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_LITTLE_METACARPAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kPinkyFingerPhalanxProximal ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_LITTLE_PROXIMAL_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(
    mojom::XRHandJoint::kPinkyFingerPhalanxIntermediate ==
        OpenXRHandJointToMojomJoint(XR_HAND_JOINT_LITTLE_INTERMEDIATE_EXT),
    "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kPinkyFingerPhalanxDistal ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_LITTLE_DISTAL_EXT),
              "WebXR - OpenXR joint enum value mismatch");
static_assert(mojom::XRHandJoint::kPinkyFingerTip ==
                  OpenXRHandJointToMojomJoint(XR_HAND_JOINT_LITTLE_TIP_EXT),
              "WebXR - OpenXR joint enum value mismatch");

}  // namespace

OpenXrController::OpenXrController()
    : description_(nullptr),
      type_(OpenXrHandednessType::kCount),  // COUNT refers to invalid.
      instance_(XR_NULL_HANDLE),
      session_(XR_NULL_HANDLE),
      action_set_(XR_NULL_HANDLE),
      grip_pose_action_{XR_NULL_HANDLE},
      grip_pose_space_(XR_NULL_HANDLE),
      pointer_pose_action_(XR_NULL_HANDLE),
      pointer_pose_space_(XR_NULL_HANDLE),
      interaction_profile_(OpenXrInteractionProfileType::kCount) {}

OpenXrController::~OpenXrController() {
  // We don't need to destroy all of the actions because destroying an
  // action set destroys all actions that are part of the set.

  if (action_set_ != XR_NULL_HANDLE) {
    xrDestroyActionSet(action_set_);
  }
  if (grip_pose_space_ != XR_NULL_HANDLE) {
    xrDestroySpace(grip_pose_space_);
  }
  if (pointer_pose_space_ != XR_NULL_HANDLE) {
    xrDestroySpace(pointer_pose_space_);
  }
  if (hand_tracker_ != XR_NULL_HANDLE) {
    extension_helper_->ExtensionMethods().xrDestroyHandTrackerEXT(
        hand_tracker_);
  }
}
XrResult OpenXrController::Initialize(
    OpenXrHandednessType type,
    XrInstance instance,
    XrSession session,
    const OpenXRPathHelper* path_helper,
    const OpenXrExtensionHelper& extension_helper,
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) {
  DCHECK(bindings);
  type_ = type;
  instance_ = instance;
  session_ = session;
  path_helper_ = path_helper;
  extension_helper_ = &extension_helper;

  std::string action_set_name =
      std::string(GetStringFromType(type_)) + "_action_set";

  XrActionSetCreateInfo action_set_create_info = {
      XR_TYPE_ACTION_SET_CREATE_INFO};

  size_t dest_size = std::size(action_set_create_info.actionSetName);
  size_t src_size = base::strlcpy(action_set_create_info.actionSetName,
                                  action_set_name.c_str(), dest_size);
  DCHECK_LT(src_size, dest_size);

  dest_size = std::size(action_set_create_info.localizedActionSetName);
  src_size = base::strlcpy(action_set_create_info.localizedActionSetName,
                           action_set_name.c_str(), dest_size);
  DCHECK_LT(src_size, dest_size);

  RETURN_IF_XR_FAILED(
      xrCreateActionSet(instance_, &action_set_create_info, &action_set_));

  RETURN_IF_XR_FAILED(InitializeControllerActions());

  SuggestBindings(bindings);
  RETURN_IF_XR_FAILED(InitializeControllerSpaces());

  return XR_SUCCESS;
}

XrResult OpenXrController::InitializeControllerActions() {
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kTrigger));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kSqueeze));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kTrackpad));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kThumbstick));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kThumbrest));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kButton1));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kButton2));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kGrasp));
  RETURN_IF_XR_FAILED(CreateActionsForButton(OpenXrButtonType::kShoulder));

  const std::string type_string = GetStringFromType(type_);
  const std::string name_prefix = type_string + "_controller_";
  // Axis Actions
  RETURN_IF_XR_FAILED(
      CreateAction(XR_ACTION_TYPE_VECTOR2F_INPUT, name_prefix + "trackpad_axis",
                   &(axis_action_map_[OpenXrAxisType::kTrackpad])));
  RETURN_IF_XR_FAILED(CreateAction(
      XR_ACTION_TYPE_VECTOR2F_INPUT, name_prefix + "thumbstick_axis",
      &(axis_action_map_[OpenXrAxisType::kThumbstick])));

  // Generic Pose Actions
  RETURN_IF_XR_FAILED(CreateAction(XR_ACTION_TYPE_POSE_INPUT,
                                   name_prefix + "grip_pose",
                                   &grip_pose_action_));
  RETURN_IF_XR_FAILED(CreateAction(XR_ACTION_TYPE_POSE_INPUT,
                                   name_prefix + "aim_pose",
                                   &pointer_pose_action_));

  return XR_SUCCESS;
}

XrResult OpenXrController::SuggestBindingsForButtonMaps(
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings,
    const std::vector<OpenXrButtonPathMap>& button_maps,
    XrPath interaction_profile_path,
    const std::string& binding_prefix) const {
  for (const auto& cur_button_map : button_maps) {
    OpenXrButtonType button_type = cur_button_map.type;

    for (const auto& cur_action_map : cur_button_map.action_maps) {
      RETURN_IF_XR_FAILED(SuggestActionBinding(
          bindings, interaction_profile_path,
          button_action_map_.at(button_type).at(cur_action_map.type),
          binding_prefix + cur_action_map.path));
    }
  }

  return XR_SUCCESS;
}

XrResult OpenXrController::SuggestBindings(
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) const {
  const std::string binding_prefix = GetTopLevelUserPath(type_);

  for (const auto& interaction_profile :
       GetOpenXrControllerInteractionProfiles()) {
    // If the interaction profile is defined by an extension, check it here,
    // otherwise continue
    if (!interaction_profile.required_extension.empty() &&
        !extension_helper_->ExtensionEnumeration()->ExtensionSupported(
            interaction_profile.required_extension.c_str())) {
      continue;
    }

    XrPath interaction_profile_path =
        path_helper_->GetInteractionProfileXrPath(interaction_profile.type);
    RETURN_IF_XR_FAILED(SuggestActionBinding(
        bindings, interaction_profile_path, grip_pose_action_,
        binding_prefix + "/input/grip/pose"));
    RETURN_IF_XR_FAILED(SuggestActionBinding(
        bindings, interaction_profile_path, pointer_pose_action_,
        binding_prefix + "/input/aim/pose"));

    RETURN_IF_XR_FAILED(SuggestBindingsForButtonMaps(
        bindings, interaction_profile.common_button_maps,
        interaction_profile_path, binding_prefix));

    switch (type_) {
      case OpenXrHandednessType::kLeft:
        RETURN_IF_XR_FAILED(SuggestBindingsForButtonMaps(
            bindings, interaction_profile.left_button_maps,
            interaction_profile_path, binding_prefix));
        break;
      case OpenXrHandednessType::kRight:
        RETURN_IF_XR_FAILED(SuggestBindingsForButtonMaps(
            bindings, interaction_profile.right_button_maps,
            interaction_profile_path, binding_prefix));
        break;
      case OpenXrHandednessType::kCount:
        NOTREACHED() << "Controller can only be left or right";
        return XR_ERROR_VALIDATION_FAILURE;
    }

    for (const auto& cur_axis_map : interaction_profile.axis_maps) {
      RETURN_IF_XR_FAILED(
          SuggestActionBinding(bindings, interaction_profile_path,
                               axis_action_map_.at(cur_axis_map.type),
                               binding_prefix + cur_axis_map.path));
    }
  }

  return XR_SUCCESS;
}

XrResult OpenXrController::InitializeControllerSpaces() {
  RETURN_IF_XR_FAILED(CreateActionSpace(grip_pose_action_, &grip_pose_space_));

  RETURN_IF_XR_FAILED(
      CreateActionSpace(pointer_pose_action_, &pointer_pose_space_));

  return XR_SUCCESS;
}

XrResult OpenXrController::InitializeHandTracking() {
  XrHandTrackerCreateInfoEXT create_info{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
  create_info.hand = type_ == OpenXrHandednessType::kRight ? XR_HAND_RIGHT_EXT
                                                           : XR_HAND_LEFT_EXT;
  create_info.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
  return extension_helper_->ExtensionMethods().xrCreateHandTrackerEXT(
      session_, &create_info, &hand_tracker_);
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
    description_->profiles =
        path_helper_->GetInputProfiles(interaction_profile_);
  }

  description_->input_from_pointer =
      GetPointerFromGripTransform(predicted_display_time);

  return description_.Clone();
}

absl::optional<GamepadButton> OpenXrController::GetButton(
    OpenXrButtonType type) const {
  GamepadButton ret;
  // Button should at least have one of the three actions;
  bool has_value = false;

  DCHECK(button_action_map_.count(type) == 1);
  auto button = button_action_map_.at(type);
  XrActionStateBoolean press_state_bool = {XR_TYPE_ACTION_STATE_BOOLEAN};
  if (XR_SUCCEEDED(QueryState(button[OpenXrButtonActionType::kPress],
                              &press_state_bool)) &&
      press_state_bool.isActive) {
    ret.pressed = press_state_bool.currentState;
    has_value = true;
  } else {
    ret.pressed = false;
  }

  XrActionStateBoolean touch_state_bool = {XR_TYPE_ACTION_STATE_BOOLEAN};
  if (XR_SUCCEEDED(QueryState(button[OpenXrButtonActionType::kTouch],
                              &touch_state_bool)) &&
      touch_state_bool.isActive) {
    ret.touched = touch_state_bool.currentState;
    has_value = true;
  } else {
    ret.touched = ret.pressed;
  }

  XrActionStateFloat value_state_float = {XR_TYPE_ACTION_STATE_FLOAT};
  if (XR_SUCCEEDED(QueryState(button[OpenXrButtonActionType::kValue],
                              &value_state_float)) &&
      value_state_float.isActive) {
    ret.value = value_state_float.currentState;
    has_value = true;
  } else {
    ret.value = ret.pressed ? 1.0 : 0.0;
  }

  if (!has_value) {
    return absl::nullopt;
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
  XrPath top_level_user_path;

  std::string top_level_user_path_string = GetTopLevelUserPath(type_);
  RETURN_IF_XR_FAILED(xrStringToPath(
      instance_, top_level_user_path_string.c_str(), &top_level_user_path));

  XrInteractionProfileState interaction_profile_state = {
      XR_TYPE_INTERACTION_PROFILE_STATE};
  RETURN_IF_XR_FAILED(xrGetCurrentInteractionProfile(
      session_, top_level_user_path, &interaction_profile_state));
  interaction_profile_ = path_helper_->GetInputProfileType(
      interaction_profile_state.interactionProfile);

  if (description_) {
    description_->profiles =
        path_helper_->GetInputProfiles(interaction_profile_);
  }
  return XR_SUCCESS;
}

mojom::XRHandTrackingDataPtr OpenXrController::GetHandTrackingData(
    XrSpace mojo_space,
    XrTime predicted_display_time) {
  // Lazy init hand tracking as we only need it if the app requests it.
  if (hand_tracker_ == XR_NULL_HANDLE) {
    if (XR_FAILED(InitializeHandTracking())) {
      return nullptr;
    }
  }

  XrHandJointLocationEXT joint_locations_buffer[XR_HAND_JOINT_COUNT_EXT];
  XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
  locations.jointCount = std::extent<decltype(joint_locations_buffer)>::value;
  locations.jointLocations = joint_locations_buffer;

  XrHandJointsLocateInfoEXT locate_info{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
  locate_info.baseSpace = mojo_space;
  locate_info.time = predicted_display_time;

  if (XR_FAILED(extension_helper_->ExtensionMethods().xrLocateHandJointsEXT(
          hand_tracker_, &locate_info, &locations)) ||
      !locations.isActive) {
    return nullptr;
  }

  mojom::XRHandTrackingDataPtr hand_tracking_data =
      device::mojom::XRHandTrackingData::New();
  hand_tracking_data->hand_joint_data =
      std::vector<mojom::XRHandJointDataPtr>{};

  constexpr unsigned kNumWebXRJoints =
      static_cast<unsigned>(mojom::XRHandJoint::kMaxValue) + 1u;

  // WebXR has one less joint than OpenXR. WebXR lacks the PALM joint which is
  // the first joint in OpenXR
  DCHECK_EQ(kNumWebXRJoints, XR_HAND_JOINT_COUNT_EXT - 1u);
  hand_tracking_data->hand_joint_data.reserve(kNumWebXRJoints);
  for (uint32_t i = 0; i < XR_HAND_JOINT_COUNT_EXT; i++) {
    // We skip the palm joint as WebXR does not support it. All other joints are
    // supported
    if (i == XR_HAND_JOINT_PALM_EXT) {
      static_assert(XR_HAND_JOINT_PALM_EXT == 0u,
                    "OpenXR palm joint expected to be the 0th joint");
      continue;
    }

    mojom::XRHandJointDataPtr joint_data =
        device::mojom::XRHandJointData::New();
    joint_data->joint =
        OpenXRHandJointToMojomJoint(static_cast<XrHandJointEXT>(i));
    joint_data->mojo_from_joint =
        XrPoseToGfxTransform(joint_locations_buffer[i].pose);
    joint_data->radius = joint_locations_buffer[i].radius;
    hand_tracking_data->hand_joint_data.push_back(std::move(joint_data));
  }

  return hand_tracking_data;
}

absl::optional<gfx::Transform> OpenXrController::GetMojoFromGripTransform(
    XrTime predicted_display_time,
    XrSpace local_space,
    bool* emulated_position) const {
  return GetTransformFromSpaces(predicted_display_time, grip_pose_space_,
                                local_space, emulated_position);
}

absl::optional<gfx::Transform> OpenXrController::GetPointerFromGripTransform(
    XrTime predicted_display_time) const {
  bool emulated_position;
  return GetTransformFromSpaces(predicted_display_time, pointer_pose_space_,
                                grip_pose_space_, &emulated_position);
}

absl::optional<gfx::Transform> OpenXrController::GetTransformFromSpaces(
    XrTime predicted_display_time,
    XrSpace target,
    XrSpace origin,
    bool* emulated_position) const {
  XrSpaceLocation location = {XR_TYPE_SPACE_LOCATION};
  // emulated_position indicates when there is a fallback from a fully-tracked
  // (i.e. 6DOF) type case to some form of orientation-only type tracking
  // (i.e. 3DOF/IMU type sensors)
  // Thus we have to make sure orientation is tracked.
  // Valid Bit only indicates it's either tracked or emulated, we have to check
  // for XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT to make sure orientation is
  // tracked.
  if (XR_FAILED(
          xrLocateSpace(target, origin, predicted_display_time, &location)) ||
      !(location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) ||
      !(location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT)) {
    return absl::nullopt;
  }

  *emulated_position = true;
  if (location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) {
    *emulated_position = false;
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

  *emulated_position = true;
  if (location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) {
    *emulated_position = false;
  }

  return gfx::Transform::Compose(decomp);
}

XrResult OpenXrController::CreateActionsForButton(
    OpenXrButtonType button_type) {
  const std::string type_string = GetStringFromType(type_);
  std::string name_prefix = type_string + "_controller_";

  switch (button_type) {
    case OpenXrButtonType::kTrigger:
      name_prefix += "trigger_";
      break;
    case OpenXrButtonType::kSqueeze:
      name_prefix += "squeeze_";
      break;
    case OpenXrButtonType::kTrackpad:
      name_prefix += "trackpad_";
      break;
    case OpenXrButtonType::kThumbstick:
      name_prefix += "thumbstick_";
      break;
    case OpenXrButtonType::kThumbrest:
      name_prefix += "thumbrest_";
      break;
    case OpenXrButtonType::kButton1:
      name_prefix += "upper_button_";
      break;
    case OpenXrButtonType::kButton2:
      name_prefix += "lower_button_";
      break;
    case OpenXrButtonType::kGrasp:
      name_prefix += "grasp_";
      break;
    case OpenXrButtonType::kShoulder:
      name_prefix += "shoulder_";
      break;
  }

  std::unordered_map<OpenXrButtonActionType, XrAction>& cur_button =
      button_action_map_[button_type];
  XrAction new_action;
  RETURN_IF_XR_FAILED(CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT,
                                   name_prefix + "button_press", &new_action));
  cur_button[OpenXrButtonActionType::kPress] = new_action;
  RETURN_IF_XR_FAILED(CreateAction(XR_ACTION_TYPE_FLOAT_INPUT,
                                   name_prefix + "button_value", &new_action));
  cur_button[OpenXrButtonActionType::kValue] = new_action;
  RETURN_IF_XR_FAILED(CreateAction(XR_ACTION_TYPE_BOOLEAN_INPUT,
                                   name_prefix + "button_touch", &new_action));
  cur_button[OpenXrButtonActionType::kTouch] = new_action;
  return XR_SUCCESS;
}

XrResult OpenXrController::CreateAction(XrActionType type,
                                        const std::string& action_name,
                                        XrAction* action) {
  DCHECK(action);
  XrActionCreateInfo action_create_info = {XR_TYPE_ACTION_CREATE_INFO};
  action_create_info.actionType = type;

  size_t dest_size = std::size(action_create_info.actionName);
  size_t src_size = base::strlcpy(action_create_info.actionName,
                                  action_name.data(), dest_size);
  DCHECK_LT(src_size, dest_size);

  dest_size = std::size(action_create_info.localizedActionName);
  src_size = base::strlcpy(action_create_info.localizedActionName,
                           action_name.data(), dest_size);
  DCHECK_LT(src_size, dest_size);
  return xrCreateAction(action_set_, &action_create_info, action);
}

XrResult OpenXrController::SuggestActionBinding(
    std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings,
    XrPath interaction_profile_path,
    XrAction action,
    std::string binding_string) const {
  XrPath binding_path;
  // make sure all actions we try to suggest binding are initialized.
  DCHECK(action != XR_NULL_HANDLE);
  RETURN_IF_XR_FAILED(
      xrStringToPath(instance_, binding_string.c_str(), &binding_path));
  (*bindings)[interaction_profile_path].push_back({action, binding_path});

  return XR_SUCCESS;
}

XrResult OpenXrController::CreateActionSpace(XrAction action, XrSpace* space) {
  DCHECK(space);
  XrActionSpaceCreateInfo action_space_create_info = {};
  action_space_create_info.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
  action_space_create_info.action = action;
  action_space_create_info.subactionPath = XR_NULL_PATH;
  action_space_create_info.poseInActionSpace = PoseIdentity();
  return xrCreateActionSpace(session_, &action_space_create_info, space);
}

}  // namespace device
