// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_SCENE_UNDERSTANDING_MANAGER_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_SCENE_UNDERSTANDING_MANAGER_ANDROID_H_

#include "device/vr/openxr/android/openxr_anchor_manager_android.h"
#include "device/vr/openxr/android/openxr_hit_test_manager_android.h"
#include "device/vr/openxr/android/openxr_plane_manager_android.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "device/vr/openxr/openxr_scene_understanding_manager.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

// SceneUnderstandingManager for the XR_ANDROID family of extensions.
class OpenXRSceneUnderstandingManagerAndroid
    : public OpenXRSceneUnderstandingManager {
 public:
  OpenXRSceneUnderstandingManagerAndroid(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      XrSpace mojo_space);
  ~OpenXRSceneUnderstandingManagerAndroid() override;

  // OpenXRSceneUnderstandingManager
  OpenXrSceneUnderstandingManagerType GetType() const override;
  OpenXrPlaneManager* GetPlaneManager() override;
  OpenXrAnchorManager* GetAnchorManager() override;
  OpenXrHitTestManager* GetHitTestManager() override;
 private:
  XrSpace mojo_space_;

  std::unique_ptr<OpenXrPlaneManagerAndroid> plane_manager_;
  std::unique_ptr<OpenXrAnchorManagerAndroid> anchor_manager_;
  std::unique_ptr<OpenXrHitTestManagerAndroid> hit_test_manager_;
};

class OpenXrSceneUnderstandingManagerAndroidFactory
    : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrSceneUnderstandingManagerAndroidFactory();
  ~OpenXrSceneUnderstandingManagerAndroidFactory() override;

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
#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_SCENE_UNDERSTANDING_MANAGER_ANDROID_H_
