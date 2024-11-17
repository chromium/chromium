// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_CONTROLLER_H_
#define DEVICE_VR_OPENXR_OPENXR_CONTROLLER_H_

#include <stdint.h>
#include <string.h>

#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "device/gamepad/public/cpp/gamepad.h"
#include "device/vr/openxr/openxr_hand_tracker.h"
#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "device/vr/openxr/openxr_path_helper.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/transform.h"

namespace device {

class OpenXrExtensionHelper;

class OpenXrController {
 public:
  OpenXrController();

  OpenXrController(const OpenXrController&) = delete;
  OpenXrController& operator=(const OpenXrController&) = delete;

  ~OpenXrController();

  // The lifetime of OpenXRInputHelper is a superset of OpenXRController. Thus
  // we may safely pass the OpenXRPathHelper of the parent class to
  // OpenXRController as a dependency.
  XrResult Initialize(
      OpenXrHandednessType type,
      XrInstance instance,
      XrSession session,
      const OpenXRPathHelper* path_helper,
      const OpenXrExtensionHelper& extension_helper,
      bool hand_input_enabled,
      std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings);

  XrActionSet action_set() const { return action_set_; }

  mojom::XRInputSourceDescriptionPtr GetDescription(
      XrTime predicted_display_time);

  std::optional<GamepadButton> GetButton(OpenXrButtonType type) const;
  std::optional<Gamepad> GetWebXRGamepad() const;

  std::optional<gfx::Transform> GetMojoFromGripTransform(
      XrTime predicted_display_time,
      XrSpace local_space,
      bool* emulated_position) const;

  // Returns true if this controller can supply HandTracking data.
  bool IsHandTrackingEnabled() const;
  mojom::XRHandTrackingDataPtr GetHandTrackingData();

  // Specifically update just the interaction profile. Will not update any other
  // data.
  XrResult UpdateInteractionProfile();

  XrResult Update(XrSpace base_space, XrTime predicted_display_time);

 private:
  XrResult InitializeControllerActions();
  XrResult InitializeControllerSpaces();

  XrResult SuggestBindings(
      std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) const;
  XrResult SuggestBindingsForButtonMaps(
      std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings,
      const std::vector<OpenXrButtonPathMap>& button_maps,
      XrPath interaction_profile_path,
      const std::string& binding_prefix) const;

  XrResult CreateActionsForButton(OpenXrButtonType button_type);
  XrResult CreateAction(XrActionType type,
                        const std::string& action_name,
                        XrAction* action);

  XrResult CreateActionSpace(XrAction action, XrSpace* space);

  XrResult SuggestActionBinding(
      std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings,
      XrPath interaction_profile_path,
      XrAction action,
      std::string binding_string) const;

  bool IsCurrentProfileFromHandTracker() const;

  std::optional<gfx::Transform> GetGripFromPointerTransform(
      XrTime predicted_display_time) const;

  mojom::XRTargetRayMode GetTargetRayMode() const;
  mojom::XRHandedness GetHandness() const;
  std::vector<double> GetAxis(OpenXrAxisType type) const;

  template <typename T>
  XrResult QueryState(XrAction action, T* action_state) const {
    // this function should never be called because each valid XrActionState
    // has its own template function defined below.
    NOTREACHED();
  }

  template <>
  XrResult QueryState<XrActionStateFloat>(
      XrAction action,
      XrActionStateFloat* action_state) const {
    action_state->type = XR_TYPE_ACTION_STATE_FLOAT;
    XrActionStateGetInfo get_info = {XR_TYPE_ACTION_STATE_GET_INFO};
    get_info.action = action;
    return xrGetActionStateFloat(session_, &get_info, action_state);
  }

  template <>
  XrResult QueryState<XrActionStateBoolean>(
      XrAction action,
      XrActionStateBoolean* action_state) const {
    action_state->type = XR_TYPE_ACTION_STATE_BOOLEAN;
    XrActionStateGetInfo get_info = {XR_TYPE_ACTION_STATE_GET_INFO};
    get_info.action = action;
    return xrGetActionStateBoolean(session_, &get_info, action_state);
  }

  template <>
  XrResult QueryState<XrActionStateVector2f>(
      XrAction action,
      XrActionStateVector2f* action_state) const {
    action_state->type = XR_TYPE_ACTION_STATE_VECTOR2F;
    XrActionStateGetInfo get_info = {XR_TYPE_ACTION_STATE_GET_INFO};
    get_info.action = action;
    return xrGetActionStateVector2f(session_, &get_info, action_state);
  }

  template <>
  XrResult QueryState<XrActionStatePose>(
      XrAction action,
      XrActionStatePose* action_state) const {
    action_state->type = XR_TYPE_ACTION_STATE_POSE;
    XrActionStateGetInfo get_info = {XR_TYPE_ACTION_STATE_GET_INFO};
    get_info.action = action;
    return xrGetActionStatePose(session_, &get_info, action_state);
  }

  mojom::XRInputSourceDescriptionPtr description_;

  OpenXrHandednessType type_;
  XrInstance instance_;
  XrSession session_;
  XrActionSet action_set_;
  XrAction grip_pose_action_;
  XrSpace grip_pose_space_;
  XrAction pointer_pose_action_;
  XrSpace pointer_pose_space_;

  std::unique_ptr<OpenXrHandTracker> hand_tracker_;

  mojom::OpenXrInteractionProfileType interaction_profile_;

  std::unordered_map<OpenXrButtonType,
                     std::unordered_map<OpenXrButtonActionType, XrAction>>
      button_action_map_;
  std::unordered_map<OpenXrAxisType, XrAction> axis_action_map_;

  raw_ptr<const OpenXRPathHelper, DanglingUntriaged> path_helper_;
  raw_ptr<const OpenXrExtensionHelper> extension_helper_;
  bool hand_joints_enabled_ = false;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_CONTROLLER_H_
