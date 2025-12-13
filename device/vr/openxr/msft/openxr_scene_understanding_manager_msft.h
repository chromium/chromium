// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_UNDERSTANDING_MANAGER_MSFT_H_
#define DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_UNDERSTANDING_MANAGER_MSFT_H_

#include <memory>
#include <optional>
#include <vector>

#include "device/vr/openxr/msft/openxr_anchor_manager_msft.h"
#include "device/vr/openxr/msft/openxr_hit_test_manager_msft.h"
#include "device/vr/openxr/msft/openxr_plane_manager_msft.h"
#include "device/vr/openxr/msft/openxr_scene_bounds_msft.h"
#include "device/vr/openxr/msft/openxr_scene_msft.h"
#include "device/vr/openxr/msft/openxr_scene_observer_msft.h"
#include "device/vr/openxr/msft/openxr_scene_plane_msft.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "device/vr/openxr/openxr_scene_understanding_manager.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

// SceneUnderstandingManager for the XR_MSFT family of extensions.
class OpenXRSceneUnderstandingManagerMSFT
    : public OpenXRSceneUnderstandingManager {
 public:
  OpenXRSceneUnderstandingManagerMSFT(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      XrSpace mojo_space);
  ~OpenXRSceneUnderstandingManagerMSFT() override;

 protected:
  // OpenXRSceneUnderstandingManager
  OpenXrSceneUnderstandingManagerType GetType() const override;
  OpenXrPlaneManager* GetPlaneManager() override;
  OpenXrAnchorManager* GetAnchorManager() override;
  OpenXrHitTestManager* GetHitTestManager() override;

 private:
  XrSpace mojo_space_;

  std::unique_ptr<OpenXrPlaneManagerMsft> plane_manager_;
  std::unique_ptr<OpenXrAnchorManagerMsft> anchor_manager_;
  std::unique_ptr<OpenXrHitTestManagerMsft> hit_test_manager_;
};

class OpenXrSceneUnderstandingManagerMsftFactory
    : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrSceneUnderstandingManagerMsftFactory();
  ~OpenXrSceneUnderstandingManagerMsftFactory() override;

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
                                  XrSpace mojo_space) const override;

 private:
  std::set<device::mojom::XRSessionFeature> supported_features_;
};

}  // namespace device
#endif  // DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_UNDERSTANDING_MANAGER_MSFT_H_
