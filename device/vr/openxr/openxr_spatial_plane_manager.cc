// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_spatial_plane_manager.h"

#include "base/containers/contains.h"
#include "device/vr/openxr/openxr_spatial_framework_manager.h"
#include "device/vr/openxr/openxr_spatial_utils.h"

namespace device {
// static
bool OpenXrSpatialPlaneManager::IsSupported(
    const std::vector<XrSpatialCapabilityEXT>& capabilities) {
  // The only components that we need to support planes are
  // XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT and
  // XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT which are guaranteed to be
  // supported if the XR_SPATIAL_CAPABILITY_ANCHOR_EXT is supported, so that's
  // all we need to check.
  return base::Contains(capabilities, XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT);
}

OpenXrSpatialPlaneManager::OpenXrSpatialPlaneManager() = default;
OpenXrSpatialPlaneManager::~OpenXrSpatialPlaneManager() = default;

void OpenXrSpatialPlaneManager::PopulateCapabilityConfiguration(
    absl::flat_hash_map<XrSpatialCapabilityEXT,
                        absl::flat_hash_set<XrSpatialComponentTypeEXT>>&
        capability_configuration) const {
  capability_configuration[XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT].insert(
      {XR_SPATIAL_COMPONENT_TYPE_BOUNDED_2D_EXT,
       XR_SPATIAL_COMPONENT_TYPE_PLANE_ALIGNMENT_EXT});
}

}  // namespace device
