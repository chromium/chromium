// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_SPATIAL_PLANE_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_SPATIAL_PLANE_MANAGER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_plane_manager.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;
class OpenXrSpatialFrameworkManager;

// Delegate class for OpenXrSpatialFrameworkManager responsible for integration
// with the XR_EXT_SPATIAL_PLANE_TRACKING extension (aka the "Plane detection"
// feature).
class OpenXrSpatialPlaneManager : public OpenXrPlaneManager {
 public:
  // Queries the supported spatial capabilities and spatial components on the
  // current system instance and returns whether or not this manager can be
  // enabled to provide support for the plane detection feature.
  static bool IsSupported(
      const std::vector<XrSpatialCapabilityEXT>& capabilities);

  OpenXrSpatialPlaneManager(
      XrSpace mojo_space,
      const OpenXrExtensionHelper& extension_helper,
      const OpenXrSpatialFrameworkManager& framework_manager,
      XrInstance instance,
      XrSystemId system);
  ~OpenXrSpatialPlaneManager() override;

  // Mutates the provided map to fill in the necessary capabilities and
  // components for those capabilities that need to be enabled. Used to build
  // the list of |capabilityConfigs| on the |XrSpatialContextCreateInfoEXT| used
  // to create the spatial context.
  void PopulateCapabilityConfiguration(
      absl::flat_hash_map<XrSpatialCapabilityEXT,
                          absl::flat_hash_set<XrSpatialComponentTypeEXT>>&
          capability_configuration) const;

  void OnSnapshotChanged();
  mojom::XRPlaneDetectionDataPtr GetDetectedPlanesData() override;
  std::optional<device::Pose> TryGetMojoFromPlane(PlaneId plane_id) const;

  // Return the `PlaneId` of the corresponding |entity_id|. Will return
  // |kInvalidPlaneId| if the entity is not currently tracked.
  PlaneId GetPlaneId(XrSpatialEntityIdEXT entity_id) const;

  std::optional<XrLocation> GetXrLocationFromPlane(
      PlaneId plane_id,
      const gfx::Transform& plane_id_from_object) const override;

  // Return the `XrSpatialEntityIdEXT` of the corresponding |plane_id|. Will
  // return XR_NULL_SPATIAL_ENTITY_ID_EXT if the |plane_id| is not currently
  // tracked or otherwise invalid.
  XrSpatialEntityIdEXT GetEntityId(PlaneId plane_id) const;

  bool can_parent_anchors() const { return can_parent_anchors_; }

 private:
  XrSpace mojo_space_;
  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  const raw_ref<const OpenXrSpatialFrameworkManager> framework_manager_;

  absl::flat_hash_map<XrSpatialEntityIdEXT, mojom::XRPlaneDataPtr>
      entity_id_to_data_;
  absl::flat_hash_set<XrSpatialEntityIdEXT> updated_entity_ids_;

  bool can_parent_anchors_ = false;

  // Both of these components are guaranteed to be supported for the
  absl::flat_hash_set<XrSpatialComponentTypeEXT> enabled_components_;

  base::WeakPtrFactory<OpenXrSpatialPlaneManager> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SPATIAL_PLANE_MANAGER_H_
