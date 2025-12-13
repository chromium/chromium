// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_SPATIAL_HIT_TEST_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_SPATIAL_HIT_TEST_MANAGER_H_

#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_hit_test_manager.h"
#include "device/vr/openxr/openxr_platform.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrSpatialFrameworkManager;
class OpenXrSpatialPlaneManager;

// Delegate class for OpenXrSpatialFrameworkManager responsible for integration
// with the XR_ANDROID_SPATIAL_DISCOVERY_RAYCAST extension (aka the "hit test"
// feature).
// NOTE: The usage of the XR_ANDROID_SPATIAL_DISCOVERY_RAYCAST is a temporary
// stopgap to enable the usage of the whole spatial framework. It is intended
// that as soon as the XR_EXT_ version of this extension exists that this class
// will be ported to leverage that extension instead. It is hoped however, that
// this extension should look very similar to the current version and thus
// hopefully only minimal changes will be required.
class OpenXrSpatialHitTestManager : public OpenXrHitTestManager {
 public:
  // Queries the supported spatial capabilities and spatial components on the
  // current system instance and returns whether or not this manager can be
  // enabled to provide support for the plane detection feature.
  static bool IsSupported(
      XrInstance instance,
      XrSystemId system,
      PFN_xrEnumerateSpatialCapabilityComponentTypesEXT
          xrEnumerateSpatialCapabilityComponentTypesEXT,
      const std::vector<XrSpatialCapabilityEXT>& capabilities);

  OpenXrSpatialHitTestManager(
      const OpenXrExtensionHelper& extension_helper,
      const OpenXrSpatialFrameworkManager& spatial_framework_manager,
      OpenXrSpatialPlaneManager* plane_manager,
      XrSpace mojo_space,
      XrInstance instance,
      XrSystemId system);
  ~OpenXrSpatialHitTestManager() override;

  // Mutates the provided map to fill in the necessary capabilities and
  // components for those capabilities that need to be enabled. Used to build
  // the list of |capabilityConfigs| on the |XrSpatialContextCreateInfoEXT| used
  // to create the spatial context.
  void PopulateCapabilityConfiguration(
      absl::flat_hash_map<XrSpatialCapabilityEXT,
                          absl::flat_hash_set<XrSpatialComponentTypeEXT>>&
          capability_components) const;

  std::vector<mojom::XRHitResultPtr> RequestHitTest(
      const gfx::Point3F& origin,
      const gfx::Vector3dF& direction) override;

 protected:
  void OnStartProcessingHitTests(XrTime predicted_display_time) override;

 private:
  XrSpatialSnapshotEXT GetSnapshot(const gfx::Point3F& origin,
                                   const gfx::Vector3dF& direction);

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  const raw_ref<const OpenXrSpatialFrameworkManager> spatial_framework_manager_;
  const raw_ptr<OpenXrSpatialPlaneManager> plane_manager_;
  XrSpace mojo_space_;
  XrInstance instance_;
  XrSystemId system_;
  XrTime predicted_display_time_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SPATIAL_HIT_TEST_MANAGER_H_
