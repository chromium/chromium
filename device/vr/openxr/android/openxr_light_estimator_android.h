// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_LIGHT_ESTIMATOR_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_LIGHT_ESTIMATOR_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
#include "device/vr/openxr/openxr_light_estimator.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
class OpenXrLightEstimatorAndroid : public OpenXrLightEstimator {
 public:
  OpenXrLightEstimatorAndroid(const OpenXrExtensionHelper& extension_helper,
                              XrSession session,
                              XrSpace mojo_space,
                              uint32_t cubemap_resolution);
  ~OpenXrLightEstimatorAndroid() override;

  mojom::XRLightEstimationDataPtr GetLightEstimate(XrTime frame_time) override;

 private:
  bool IsReflectionSupported() const;
  mojom::XRReflectionProbePtr CreateAndConfigureReflectionProbe(
      XrCubemapLightingDataANDROID& cubemap_data);

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrSpace mojo_space_;
  uint32_t cubemap_resolution_;

  XrLightEstimatorANDROID light_estimator_ = XR_NULL_HANDLE;

  base::TimeTicks last_reflection_probe_update_;
  mojom::XRReflectionProbePtr reflection_probe_ = nullptr;
};

class OpenXrLightEstimatorAndroidFactory
    : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrLightEstimatorAndroidFactory();
  ~OpenXrLightEstimatorAndroidFactory() override;

  const base::flat_set<std::string_view>& GetRequestedExtensions()
      const override;
  XRSessionFeatureSet GetSupportedFeatures() const override;

  void CheckAndUpdateEnabledState(
      const OpenXrExtensionEnumeration* extension_enum,
      XrInstance instance,
      XrSystemId system) override;

  std::unique_ptr<OpenXrLightEstimator> CreateLightEstimator(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      XrSpace mojo_space) const override;

 private:
  uint32_t selected_resolution_ = 0;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_LIGHT_ESTIMATOR_ANDROID_H_
