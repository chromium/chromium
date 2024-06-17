// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_PATH_HELPER_H_
#define DEVICE_VR_OPENXR_OPENXR_PATH_HELPER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "device/vr/openxr/openxr_interaction_profiles.h"
#include "device/vr/public/mojom/openxr_interaction_profile_type.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXRPathHelper {
 public:
  OpenXRPathHelper();
  ~OpenXRPathHelper();

  XrResult Initialize(XrInstance instance, XrSystemId system);

  std::vector<std::string> GetInputProfiles(
      mojom::OpenXrInteractionProfileType interaction_profile,
      bool hand_joints_enabled) const;

  mojom::OpenXrInteractionProfileType GetInputProfileType(
      XrPath interaction_profile) const;

  XrPath GetInteractionProfileXrPath(
      mojom::OpenXrInteractionProfileType type) const;

 private:
  bool initialized_{false};
  std::string system_name_;

  std::unordered_map<mojom::OpenXrInteractionProfileType, XrPath>
      declared_interaction_profile_paths_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_PATH_HELPER_H_
