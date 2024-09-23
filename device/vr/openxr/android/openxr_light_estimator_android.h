// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_ANDROID_OPENXR_LIGHT_ESTIMATOR_ANDROID_H_
#define DEVICE_VR_OPENXR_ANDROID_OPENXR_LIGHT_ESTIMATOR_ANDROID_H_

#include "base/memory/raw_ref.h"
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
                              XrSpace mojo_space);
  ~OpenXrLightEstimatorAndroid() override;

  mojom::XRLightEstimationDataPtr GetLightEstimate(XrTime frame_time) override;

 private:
  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrSpace mojo_space_;

  XrLightEstimatorANDROID light_estimator_ = XR_NULL_HANDLE;
};

class OpenXrLightEstimatorAndroidFactory
    : public OpenXrExtensionHandlerFactory {
 public:
  OpenXrLightEstimatorAndroidFactory();
  ~OpenXrLightEstimatorAndroidFactory() override;

  const base::flat_set<std::string_view>& GetRequestedExtensions()
      const override;
  std::set<device::mojom::XRSessionFeature> GetSupportedFeatures(
      const OpenXrExtensionEnumeration* extension_enum) const override;

  std::unique_ptr<OpenXrLightEstimator> CreateLightEstimator(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session,
      XrSpace mojo_space) const override;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_ANDROID_OPENXR_LIGHT_ESTIMATOR_ANDROID_H_
