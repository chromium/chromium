// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_path_helper.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"

namespace device {

using device::mojom::OpenXrInteractionProfileType;

namespace {
constexpr std::array<OpenXrInteractionProfileType, 3> kHandProfiles{
    OpenXrInteractionProfileType::kHandSelectGrasp,
    OpenXrInteractionProfileType::kExtHand,
    OpenXrInteractionProfileType::kMetaHandAim,
};
}  // namespace

OpenXRPathHelper::OpenXRPathHelper() = default;

OpenXRPathHelper::~OpenXRPathHelper() = default;

XrResult OpenXRPathHelper::Initialize(XrInstance instance, XrSystemId system) {
  DCHECK(!initialized_);

  // Get the system properties, which is needed to determine the name of the
  // hardware being used. This helps disambiguate certain sets of controllers.
  XrSystemProperties system_properties = {XR_TYPE_SYSTEM_PROPERTIES};
  RETURN_IF_XR_FAILED(
      xrGetSystemProperties(instance, system, &system_properties));
  system_name_ = std::string(system_properties.systemName);

  // Create path declarations
  for (const auto& profile : GetOpenXrControllerInteractionProfiles()) {
    RETURN_IF_XR_FAILED(
        xrStringToPath(instance, profile.path.c_str(),
                       &(declared_interaction_profile_paths_[profile.type])));
  }
  initialized_ = true;

  return XR_SUCCESS;
}

OpenXrInteractionProfileType OpenXRPathHelper::GetInputProfileType(
    XrPath interaction_profile) const {
  DCHECK(initialized_);
  for (auto it : declared_interaction_profile_paths_) {
    if (it.second == interaction_profile) {
      return it.first;
    }
  }
  return OpenXrInteractionProfileType::kInvalid;
}

std::vector<std::string> OpenXRPathHelper::GetInputProfiles(
    OpenXrInteractionProfileType interaction_profile,
    bool hand_joints_enabled) const {
  DCHECK(initialized_);

  bool can_use_hand_joint_profile =
      hand_joints_enabled && base::Contains(kHandProfiles, interaction_profile);

  const auto& input_profiles_map = GetOpenXrInputProfilesMap();
  if (input_profiles_map.contains(interaction_profile)) {
    const OpenXrSystemInputProfiles* active_system = nullptr;
    for (const auto& system : input_profiles_map.at(interaction_profile)) {
      if ((!can_use_hand_joint_profile && system.system_name.empty()) ||
          (can_use_hand_joint_profile &&
           system.system_name.compare(kOpenXrHandJointSystem) == 0)) {
        active_system = &system;
      } else if (system_name_.compare(system.system_name) == 0) {
        active_system = &system;
        break;
      }
    }

    // Each interaction profile should always at least have a null system_name
    // entry. Profiles that support hand joints should have both a null
    // system_name and a `kOpenXrHandJointSystem` system_name.
    DCHECK(active_system);
    return active_system->input_profiles;
  }

  return {};
}

XrPath OpenXRPathHelper::GetInteractionProfileXrPath(
    OpenXrInteractionProfileType type) const {
  if (type == OpenXrInteractionProfileType::kInvalid) {
    return XR_NULL_PATH;
  }
  return declared_interaction_profile_paths_.at(type);
}

}  // namespace device
