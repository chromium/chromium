// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_SCENE_UNDERSTANDING_MANAGER_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_SCENE_UNDERSTANDING_MANAGER_ANDROID_H_

#include "device/vr/openxr/openxr_scene_understanding_manager.h"

#include <vector>

#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace gfx {
class Point3F;
class Vector3dF;
}  // namespace gfx

namespace device {

class OpenXrExtensionHelper;

// Class for managing planes and hittests using the _ANDROID extensions.
class OpenXRSceneUnderstandingManagerAndroid
    : public OpenXRSceneUnderstandingManager {
 public:
  OpenXRSceneUnderstandingManagerAndroid(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      XrSpace mojo_space);
  ~OpenXRSceneUnderstandingManagerAndroid() override;

 private:
  // OpenXRSceneUnderstandingManager
  void OnFrameUpdate(XrTime predicted_display_time) override;
  bool OnNewHitTestSubscription() override;
  void OnAllHitTestSubscriptionsRemoved() override;
  std::vector<mojom::XRHitResultPtr> RequestHitTest(
      const gfx::Point3F& origin,
      const gfx::Vector3dF& direction) override;

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrSpace mojo_space_;

  XrTime predicted_display_time_ = 0;

  XrTrackableTrackerANDROID plane_tracker_ = XR_NULL_HANDLE;
};

class OpenXrSceneUnderstandingManagerAndroidFactory
    : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrSceneUnderstandingManagerAndroidFactory();
  ~OpenXrSceneUnderstandingManagerAndroidFactory() override;

  const base::flat_set<std::string_view>& GetRequestedExtensions()
      const override;
  std::set<device::mojom::XRSessionFeature> GetSupportedFeatures(
      const OpenXrExtensionEnumeration* extension_enum) const override;

  std::unique_ptr<OpenXRSceneUnderstandingManager>
  CreateSceneUnderstandingManager(const OpenXrExtensionHelper& extension_helper,
                                  XrSession session,
                                  XrSpace mojo_space) const override;
};

}  // namespace device
#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_SCENE_UNDERSTANDING_MANAGER_ANDROID_H_
