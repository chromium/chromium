// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_SPATIAL_ANCHOR_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_SPATIAL_ANCHOR_MANAGER_H_

#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "device/vr/openxr/openxr_anchor_manager.h"
#include "device/vr/public/mojom/pose.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;
class OpenXrSpatialFrameworkManager;
class OpenXrSpatialPlaneManager;

// Delegate class for OpenXrSpatialFrameworkManager responsible for integration
// with the XR_EXT_SPATIAL_ANCHOR extension (aka the "Anchors" feature).
class OpenXrSpatialAnchorManager : public OpenXrAnchorManager {
 public:
  // Queries the supported spatial capabilities and spatial components on the
  // current system instance and returns whether or not this manager can be
  // enabled to provide support for the anchors feature.
  static bool IsSupported(
      const std::vector<XrSpatialCapabilityEXT>& capabilities);

  OpenXrSpatialAnchorManager(
      const OpenXrExtensionHelper& extension_helper,
      const OpenXrSpatialFrameworkManager& spatial_framework_manager,
      OpenXrSpatialPlaneManager* plane_manager,
      XrSpace mojo_space);
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
  AnchorId CreateAnchor(XrPosef pose,
                        XrSpace space,
                        XrTime predicted_display_time,
                        std::optional<PlaneId> plane_id) override;
  void DetachAnchor(AnchorId anchor_id) override;
  std::optional<XrLocation> GetXrLocationFromAnchor(
      AnchorId anchor_id,
      const gfx::Transform& anchor_id_from_new_anchor) const override;
  mojom::XRAnchorsDataPtr GetCurrentAnchorsData(
      XrTime predicted_display_time) override;

 private:
  struct SpatialAnchorData {
    XrSpatialEntityEXT entity;
    XrSpatialEntityIdEXT entity_id;
  };

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  const raw_ref<const OpenXrSpatialFrameworkManager> spatial_framework_manager_;
  const raw_ptr<OpenXrSpatialPlaneManager> plane_manager_;
  XrSpace mojo_space_;

  AnchorId::Generator anchor_id_generator_;
  absl::flat_hash_map<AnchorId, SpatialAnchorData> anchors_;
  absl::flat_hash_map<XrSpatialEntityIdEXT, AnchorId> entity_id_to_anchor_id_;

  absl::flat_hash_map<AnchorId, std::optional<device::Pose>>
      cached_anchor_poses_;

  base::WeakPtrFactory<OpenXrSpatialAnchorManager> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SPATIAL_ANCHOR_MANAGER_H_
