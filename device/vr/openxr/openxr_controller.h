// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_CONTROLLER_H_
#define DEVICE_VR_OPENXR_OPENXR_CONTROLLER_H_

#include <stdint.h>
#include <string.h>
#include <map>
#include <unordered_map>
#include <vector>

#include "base/optional.h"
#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "device/vr/openxr/openxr_path_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/transform.h"

namespace device {

class OpenXrController {
 public:
  OpenXrController();
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
      std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings);

  XrActionSet action_set() const { return action_set_; }
  uint32_t GetId() const;
  device::mojom::XRHandedness GetHandness() const;
  OpenXrInteractionProfileType interaction_profile() const {
    return interaction_profile_;
  }

  mojom::XRInputSourceDescriptionPtr GetDescription(
      XrTime predicted_display_time);

  base::Optional<GamepadButton> GetButton(OpenXrButtonType type) const;
  std::vector<double> GetAxis(OpenXrAxisType type) const;

  base::Optional<gfx::Transform> GetMojoFromGripTransform(
      XrTime predicted_display_time,
      XrSpace local_space,
      bool* emulated_position) const;

  XrResult UpdateInteractionProfile();

 private:
  XrResult InitializeControllerActions();
  XrResult InitializeControllerSpaces();

  XrResult SuggestBindings(
      const OpenXrExtensionHelper& extension_helper,
      std::map<XrPath, std::vector<XrActionSuggestedBinding>>* bindings) const;

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

  base::Optional<gfx::Transform> GetPointerFromGripTransform(
      XrTime predicted_display_time) const;

  base::Optional<gfx::Transform> GetTransformFromSpaces(
      XrTime predicted_display_time,
      XrSpace target,
      XrSpace origin,
      bool* emulated_position) const;

  template <typename T>
  XrResult QueryState(XrAction action, T* action_state) const {
    // this function should never be called because each valid XrActionState
    // has its own template function defined below.
    NOTREACHED();
    return XR_ERROR_ACTION_TYPE_MISMATCH;
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

  device::mojom::XRInputSourceDescriptionPtr description_;

  OpenXrHandednessType type_;
  XrInstance instance_;
  XrSession session_;
  XrActionSet action_set_;
  XrAction grip_pose_action_;
  XrSpace grip_pose_space_;
  XrAction pointer_pose_action_;
  XrSpace pointer_pose_space_;

  OpenXrInteractionProfileType interaction_profile_;

  std::unordered_map<OpenXrButtonType,
                     std::unordered_map<OpenXrButtonActionType, XrAction>>
      button_action_map_;
  std::unordered_map<OpenXrAxisType, XrAction> axis_action_map_;

  const OpenXRPathHelper* path_helper_;

  DISALLOW_COPY_AND_ASSIGN(OpenXrController);
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_CONTROLLER_H_
