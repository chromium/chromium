// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_DEPTH_SENSOR_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_DEPTH_SENSOR_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_depth_sensor.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
class OpenXrDepthSensorAndroid : public OpenXrDepthSensor {
 public:
  OpenXrDepthSensorAndroid(const OpenXrExtensionHelper& extension_helper,
                           XrSession session,
                           XrSpace mojo_space,
                           const mojom::XRDepthOptions& depth_options);
  ~OpenXrDepthSensorAndroid() override;

  XrResult Initialize() override;
  mojom::XRDepthConfigPtr GetDepthConfig() override;
  void PopulateDepthData(XrTime frame_time,
                         const std::vector<mojom::XRViewPtr>& views) override;

 private:
  mojom::XRDepthDataPtr GetDepthDataForEye(
      const XrDepthAcquireResultANDROID& acquire_result,
      const mojom::XRViewPtr& view);

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrSpace mojo_space_;

  XrDepthSwapchainANDROID swapchain_ = XR_NULL_HANDLE;
  XrDepthCameraResolutionANDROID depth_camera_resolution_ =
      XR_DEPTH_CAMERA_RESOLUTION_MAX_ENUM_ANDROID;
  std::vector<XrDepthSwapchainImageANDROID> depth_images_;

  mojom::XRDepthConfigPtr depth_config_ = nullptr;
  bool initialized_ = false;
};

class OpenXrDepthSensorAndroidFactory : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrDepthSensorAndroidFactory();
  ~OpenXrDepthSensorAndroidFactory() override;

  const base::flat_set<std::string_view>& GetRequestedExtensions()
      const override;
  std::set<device::mojom::XRSessionFeature> GetSupportedFeatures(
      const OpenXrExtensionEnumeration* extension_enum) const override;

  void ProcessSystemProperties(const OpenXrExtensionEnumeration* extension_enum,
                               XrInstance instance,
                               XrSystemId system) override;

  std::unique_ptr<OpenXrDepthSensor> CreateDepthSensor(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      XrSpace mojo_space,
      const mojom::XRDepthOptions& depth_options) const override;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_DEPTH_SENSOR_ANDROID_H_
