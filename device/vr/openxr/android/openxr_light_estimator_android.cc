// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/vr/openxr/android/openxr_light_estimator_android.h"

#include <memory>
#include <set>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
OpenXrLightEstimatorAndroid::OpenXrLightEstimatorAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space) {
  XrLightEstimatorCreateInfoANDROID create_info{
      XR_TYPE_LIGHT_ESTIMATOR_CREATE_INFO_ANDROID};
  extension_helper_->ExtensionMethods().xrCreateLightEstimatorANDROID(
      session_, &create_info, &light_estimator_);
}

OpenXrLightEstimatorAndroid::~OpenXrLightEstimatorAndroid() {
  if (light_estimator_ != XR_NULL_HANDLE) {
    XrResult result =
        extension_helper_->ExtensionMethods().xrDestroyLightEstimatorANDROID(
            light_estimator_);
    if (XR_FAILED(result)) {
      LOG(ERROR) << __func__ << " Failed to destroy light estimator.";
    }

    light_estimator_ = XR_NULL_HANDLE;
  }
}

mojom::XRLightEstimationDataPtr OpenXrLightEstimatorAndroid::GetLightEstimate(
    XrTime frame_time) {
  if (light_estimator_ == XR_NULL_HANDLE) {
    return nullptr;
  }

  XrLightEstimateGetInfoANDROID estimate_info = {
      XR_TYPE_LIGHT_ESTIMATE_GET_INFO_ANDROID};
  estimate_info.space = mojo_space_;
  estimate_info.time = frame_time;

  XrDirectionalLightANDROID directional_light = {
      XR_TYPE_DIRECTIONAL_LIGHT_ANDROID};

  XrSphericalHarmonicsANDROID ambient_harmonics = {
      XR_TYPE_SPHERICAL_HARMONICS_ANDROID};
  ambient_harmonics.kind = XR_SPHERICAL_HARMONICS_KIND_AMBIENT_ANDROID;
  ambient_harmonics.next = &directional_light;

  XrLightEstimateANDROID estimate = {XR_TYPE_LIGHT_ESTIMATE_ANDROID};
  estimate.next = &ambient_harmonics;

  XrResult result =
      extension_helper_->ExtensionMethods().xrGetLightEstimateANDROID(
          light_estimator_, &estimate_info, &estimate);
  if (XR_FAILED(result) ||
      estimate.state != XR_LIGHT_ESTIMATE_STATE_VALID_ANDROID ||
      ambient_harmonics.state != XR_LIGHT_ESTIMATE_STATE_VALID_ANDROID ||
      directional_light.state != XR_LIGHT_ESTIMATE_STATE_VALID_ANDROID) {
    return nullptr;
  }

  auto light_estimation_data = mojom::XRLightEstimationData::New();
  light_estimation_data->light_probe = device::mojom::XRLightProbe::New();
  auto& light_probe = light_estimation_data->light_probe;

  light_probe->spherical_harmonics = device::mojom::XRSphericalHarmonics::New();
  auto& spherical_harmonics = light_probe->spherical_harmonics;

  constexpr size_t kNumShCoefficients = 9;
  constexpr size_t kRedChannel = 0;
  constexpr size_t kGreenChannel = 1;
  constexpr size_t kBlueChannel = 2;
  spherical_harmonics->coefficients.reserve(kNumShCoefficients);
  for (size_t i = 0; i < kNumShCoefficients; i++) {
    spherical_harmonics->coefficients.emplace_back(
        ambient_harmonics.coefficients[i][kRedChannel],
        ambient_harmonics.coefficients[i][kGreenChannel],
        ambient_harmonics.coefficients[i][kBlueChannel]);
  }
  light_probe->main_light_intensity = {directional_light.intensity.x,
                                       directional_light.intensity.y,
                                       directional_light.intensity.z};
  light_probe->main_light_direction = gfx::Vector3dF(
      directional_light.direction.x, directional_light.direction.y,
      directional_light.direction.z);

  return light_estimation_data;
}

OpenXrLightEstimatorAndroidFactory::OpenXrLightEstimatorAndroidFactory() =
    default;
OpenXrLightEstimatorAndroidFactory::~OpenXrLightEstimatorAndroidFactory() =
    default;

const base::flat_set<std::string_view>&
OpenXrLightEstimatorAndroidFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_ANDROID_LIGHT_ESTIMATION_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrLightEstimatorAndroidFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::LIGHT_ESTIMATION};
}

std::unique_ptr<OpenXrLightEstimator>
OpenXrLightEstimatorAndroidFactory::CreateLightEstimator(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrLightEstimatorAndroid>(extension_helper,
                                                         session, mojo_space);
  }

  return nullptr;
}

}  // namespace device
