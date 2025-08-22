// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_SPATIAL_ANCHOR_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_SPATIAL_ANCHOR_MANAGER_H_

#include "base/types/expected.h"
#include "device/vr/openxr/openxr_anchor_manager.h"
#include "device/vr/public/mojom/pose.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

// Delegate class for OpenXrSpatialFrameworkManager responsible for integration
// with the XR_EXT_SPATIAL_ANCHOR extension (aka the "Anchors" feature).
class OpenXrSpatialAnchorManager : public OpenXrAnchorManager {
 public:
  // Queries the supported spatial capabilities and spatial components on the
  // current system instance and returns whether or not this manager can be
  // enabled to provide support for the anchors feature.
  static bool IsSupported(
      const std::vector<XrSpatialCapabilityEXT>& capabilities);

  OpenXrSpatialAnchorManager();
  ~OpenXrSpatialAnchorManager() override;

  // Mutates the provided map to fill in the necessary capabilities and
  // components for those capabilities that need to be enabled. Used to build
  // the list of |capabilityConfigs| on the |XrSpatialContextCreateInfoEXT| used
  // to create the spatial context.
  void PopulateCapabilityConfiguration(
      absl::flat_hash_map<XrSpatialCapabilityEXT,
                          absl::flat_hash_set<XrSpatialComponentTypeEXT>>&
          capability_configuration) const;

  // OpenXrAnchorManager
  XrSpace CreateAnchor(XrPosef pose,
                       XrSpace space,
                       XrTime predicted_display_time) override;
  void OnDetachAnchor(const XrSpace& anchor) override;
  base::expected<device::Pose, AnchorTrackingErrorType> GetAnchorFromMojom(
      XrSpace anchor_space,
      XrTime predicted_display_time) const override;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SPATIAL_ANCHOR_MANAGER_H_
