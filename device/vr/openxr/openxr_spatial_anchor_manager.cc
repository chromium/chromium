// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_spatial_anchor_manager.h"

#include "base/containers/contains.h"
#include "base/types/expected.h"
#include "device/vr/openxr/openxr_spatial_framework_manager.h"
#include "device/vr/openxr/openxr_spatial_utils.h"

namespace device {
// static
bool OpenXrSpatialAnchorManager::IsSupported(
    const std::vector<XrSpatialCapabilityEXT>& capabilities) {
  // The only component that we need to support anchors is
  // XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT, which is guaranteed to be supported
  // if the XR_SPATIAL_CAPABILITY_ANCHOR_EXT is supported, so that's all we need
  // to check.
  return base::Contains(capabilities, XR_SPATIAL_CAPABILITY_ANCHOR_EXT);
}

OpenXrSpatialAnchorManager::OpenXrSpatialAnchorManager() = default;
OpenXrSpatialAnchorManager::~OpenXrSpatialAnchorManager() = default;

void OpenXrSpatialAnchorManager::PopulateCapabilityConfiguration(
    absl::flat_hash_map<XrSpatialCapabilityEXT,
                        absl::flat_hash_set<XrSpatialComponentTypeEXT>>&
        capability_configuration) const {
  // Operator[] creates an empty entry if it does not exist.
  capability_configuration[XR_SPATIAL_CAPABILITY_ANCHOR_EXT].insert(
      XR_SPATIAL_COMPONENT_TYPE_ANCHOR_EXT);
}

XrSpace OpenXrSpatialAnchorManager::CreateAnchor(
    XrPosef pose,
    XrSpace space,
    XrTime predicted_display_time) {
  // TODO: Implement anchor creation.
  return XR_NULL_HANDLE;
}

void OpenXrSpatialAnchorManager::OnDetachAnchor(const XrSpace& anchor) {
  // TODO: Implement anchor detachment.
}

base::expected<device::Pose, OpenXrAnchorManager::AnchorTrackingErrorType>
OpenXrSpatialAnchorManager::GetAnchorFromMojom(
    XrSpace anchor_space,
    XrTime predicted_display_time) const {
  // TODO: Implement anchor tracking.
  return base::unexpected(AnchorTrackingErrorType::kPermanent);
}

}  // namespace device
