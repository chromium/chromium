// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_spatial_capability_configuration_base.h"

#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

OpenXrSpatialCapabilityConfigurationBase::
    OpenXrSpatialCapabilityConfigurationBase(
        XrSpatialCapabilityEXT capability,
        const absl::flat_hash_set<XrSpatialComponentTypeEXT>& components)
    : components_(components.begin(), components.end()) {
  // The `type` of the XrSpatialCapabilityConfigurationBaseHeaderEXT struct has
  // a 1:1 correlation with the `capability` that it is being provided for.
  XrStructureType type = XR_TYPE_UNKNOWN;
  if (capability == XR_SPATIAL_CAPABILITY_PLANE_TRACKING_EXT) {
    type = XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_PLANE_TRACKING_EXT;
  } else if (capability == XR_SPATIAL_CAPABILITY_ANCHOR_EXT) {
    type = XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_ANCHOR_EXT;
  } else if (capability == XR_SPATIAL_CAPABILITY_DEPTH_RAYCAST_ANDROID) {
    type = XR_TYPE_SPATIAL_CAPABILITY_CONFIGURATION_DEPTH_RAYCAST_ANDROID;
  } else {
    NOTREACHED() << __func__ << " Unhandled capability type: " << capability;
  }

  config_ = XrSpatialCapabilityConfigurationBaseHeaderEXT{
      type, /*next=*/nullptr, capability,
      /*enabledComponentCount=*/static_cast<uint32_t>(components_.size()),
      /*enabledComponents=*/components_.data()};
}

OpenXrSpatialCapabilityConfigurationBase::
    ~OpenXrSpatialCapabilityConfigurationBase() = default;

OpenXrSpatialCapabilityConfigurationBase::
    OpenXrSpatialCapabilityConfigurationBase(
        const OpenXrSpatialCapabilityConfigurationBase& other) = default;

OpenXrSpatialCapabilityConfigurationBase&
OpenXrSpatialCapabilityConfigurationBase::operator=(
    const OpenXrSpatialCapabilityConfigurationBase& other) = default;

OpenXrSpatialCapabilityConfigurationBase::
    OpenXrSpatialCapabilityConfigurationBase(
        OpenXrSpatialCapabilityConfigurationBase&& other) = default;

OpenXrSpatialCapabilityConfigurationBase&
OpenXrSpatialCapabilityConfigurationBase::operator=(
    OpenXrSpatialCapabilityConfigurationBase&& other) = default;

XrSpatialCapabilityConfigurationBaseHeaderEXT*
OpenXrSpatialCapabilityConfigurationBase::GetAsBaseHeader() {
  return &config_;
}

}  // namespace device
