// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_SPATIAL_FRAMEWORK_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_SPATIAL_FRAMEWORK_MANAGER_H_

#include <set>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "device/vr/openxr/openxr_scene_understanding_manager.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrSpatialAnchorManager;
class OpenXrSpatialHitTestManager;
class OpenXrSpatialPlaneManager;

// Orchestrator class for interfacing with the XR_EXT_SPATIAL_* class of
// extensions and leveraging them to provide "Scene Understanding" style data.
class OpenXrSpatialFrameworkManager : public OpenXRSceneUnderstandingManager {
 public:
  OpenXrSpatialFrameworkManager(
      const OpenXrExtensionHelper& extension_helper,
      OpenXrApiWrapper* openxr,
      XrSpace space,
      const std::set<device::mojom::XRSessionFeature>& supported_features);
  ~OpenXrSpatialFrameworkManager() override;

  // OpenXRSceneUnderstandingManager
  OpenXrSceneUnderstandingManagerType GetType() const override;
  OpenXrPlaneManager* GetPlaneManager() override;
  OpenXrHitTestManager* GetHitTestManager() override;
  OpenXrAnchorManager* GetAnchorManager() override;
  void OnDiscoveryRecommended(
      const XrEventDataSpatialDiscoveryRecommendedEXT* event_data) override;

  // These represent core components of the spatial framework that delegate
  // managers will need to access.
  XrSpatialContextEXT GetSpatialContext() const;
  XrSpatialSnapshotEXT GetDiscoverySnapshot() const;

 private:
  void OnCreateSpatialContextComplete(XrFutureEXT future);
  void OnCreateSpatialDiscoverySnapshotComplete(XrFutureEXT future);

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  raw_ptr<OpenXrApiWrapper> openxr_;
  XrSpace base_space_ = XR_NULL_HANDLE;
  XrSpatialContextEXT spatial_context_ = XR_NULL_HANDLE;
  XrSpatialSnapshotEXT discovery_snapshot_ = XR_NULL_HANDLE;

  std::unique_ptr<OpenXrSpatialPlaneManager> plane_manager_;
  std::unique_ptr<OpenXrSpatialHitTestManager> hit_test_manager_;
  std::unique_ptr<OpenXrSpatialAnchorManager> anchor_manager_;

  base::WeakPtrFactory<OpenXrSpatialFrameworkManager> weak_ptr_factory_{this};
};

class OpenXrSpatialFrameworkManagerFactory
    : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrSpatialFrameworkManagerFactory();
  ~OpenXrSpatialFrameworkManagerFactory() override;

  const base::flat_set<std::string_view>& GetRequestedExtensions()
      const override;
  std::set<device::mojom::XRSessionFeature> GetSupportedFeatures()
      const override;
  void CheckAndUpdateEnabledState(
      const OpenXrExtensionEnumeration* extension_enum,
      XrInstance instance,
      XrSystemId system) override;
  std::unique_ptr<OpenXRSceneUnderstandingManager>
  CreateSceneUnderstandingManager(const OpenXrExtensionHelper& extension_helper,
                                  OpenXrApiWrapper* openxr,
                                  XrSpace base_space) const override;

 private:
  std::set<device::mojom::XRSessionFeature> supported_features_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SPATIAL_FRAMEWORK_MANAGER_H_
