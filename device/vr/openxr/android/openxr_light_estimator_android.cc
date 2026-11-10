// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/android/openxr_light_estimator_android.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "base/bit_cast.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/public/cpp/xr_session_feature_set.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace {
constexpr base::TimeDelta kCubemapUpdateInterval = base::Seconds(1);

// This format maps to 16-bit half float (RGBA16F), which is what mojo/blink
// expect for each pixel component.
constexpr XrCubemapLightingColorFormatANDROID kRequiredColorFormat =
    XR_CUBEMAP_LIGHTING_COLOR_FORMAT_R16G16B16A16_SFLOAT_ANDROID;

// These values are somewhat arbitrarily chosen to avoid sending something too
// low res to be useful or so high res that it bogs down the render loop (even
// though we *do* limit processing to once per second). Min and Max are intended
// to be inclusive.
constexpr uint32_t kPreferredCubeMapResolution = 32;
constexpr uint32_t kMinCubeMapResolution = 8;
constexpr uint32_t kMaxCubeMapResolution = 64;

}  // namespace

namespace device {
OpenXrLightEstimatorAndroid::OpenXrLightEstimatorAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space,
    uint32_t cubemap_resolution)
    : extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space),
      cubemap_resolution_(cubemap_resolution) {
  XrLightEstimatorCreateInfoANDROID create_info{
      XR_TYPE_LIGHT_ESTIMATOR_CREATE_INFO_ANDROID};

  // The structure must exist outside of the if block to be appended, so that
  // the pointer remains valid when passed to the runtime.
  XrCubemapLightEstimatorCreateInfoANDROID cubemap_create_info = {
      .type = XR_TYPE_CUBEMAP_LIGHT_ESTIMATOR_CREATE_INFO_ANDROID,
      .cubemapResolution = cubemap_resolution_,
      .colorFormat = kRequiredColorFormat,
      .reproject = XR_TRUE};
  if (IsReflectionSupported()) {
    create_info.next = &cubemap_create_info;
  }

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

bool OpenXrLightEstimatorAndroid::IsReflectionSupported() const {
  // To simplify, we use a cubemap_resolution_ of 0 to indicate that we cannot
  // support the reflection cubemap.
  return cubemap_resolution_ > 0;
}

mojom::XRReflectionProbePtr
OpenXrLightEstimatorAndroid::CreateAndConfigureReflectionProbe(
    XrCubemapLightingDataANDROID& cubemap_data) {
  auto reflection_probe = device::mojom::XRReflectionProbe::New();
  reflection_probe->cube_map = device::mojom::XRCubeMap::New();
  reflection_probe->cube_map->width_and_height = cubemap_resolution_;

  // RGBA16F has 4 channels per pixel. We use checked multiplication to
  // guarantee no integer overflow occurs during allocation.
  size_t face_size =
      base::CheckMul<size_t>(cubemap_resolution_, cubemap_resolution_, 4u)
          .ValueOrDie();
  reflection_probe->cube_map->positive_x.resize(face_size);
  reflection_probe->cube_map->negative_x.resize(face_size);
  reflection_probe->cube_map->positive_y.resize(face_size);
  reflection_probe->cube_map->negative_y.resize(face_size);
  reflection_probe->cube_map->positive_z.resize(face_size);
  reflection_probe->cube_map->negative_z.resize(face_size);

  auto& cube_map = reflection_probe->cube_map;

  cubemap_data.imageBufferSize = base::checked_cast<uint32_t>(
      cube_map->positive_x.size() * sizeof(uint16_t));
  cubemap_data.rightImageBuffer =
      base::as_writable_bytes(base::span(cube_map->positive_x)).data();
  cubemap_data.leftImageBuffer =
      base::as_writable_bytes(base::span(cube_map->negative_x)).data();
  cubemap_data.topImageBuffer =
      base::as_writable_bytes(base::span(cube_map->positive_y)).data();
  cubemap_data.bottomImageBuffer =
      base::as_writable_bytes(base::span(cube_map->negative_y)).data();
  cubemap_data.frontImageBuffer =
      base::as_writable_bytes(base::span(cube_map->negative_z)).data();
  cubemap_data.backImageBuffer =
      base::as_writable_bytes(base::span(cube_map->positive_z)).data();

  return reflection_probe;
}

mojom::XRLightEstimationDataPtr OpenXrLightEstimatorAndroid::GetLightEstimate(
    XrTime frame_time) {
  if (light_estimator_ == XR_NULL_HANDLE) {
    return nullptr;
  }
  TRACE_EVENT0("xr", "GetLightEstimate");

  XrLightEstimateGetInfoANDROID estimate_info = {
      XR_TYPE_LIGHT_ESTIMATE_GET_INFO_ANDROID};
  estimate_info.space = mojo_space_;
  estimate_info.time = frame_time;

  XrLightEstimateANDROID estimate = {XR_TYPE_LIGHT_ESTIMATE_ANDROID};
  XrNextChainBuilder next_chain(&estimate);

  XrDirectionalLightANDROID directional_light = {
      XR_TYPE_DIRECTIONAL_LIGHT_ANDROID};
  next_chain.Add(&directional_light);

  XrSphericalHarmonicsANDROID ambient_harmonics = {
      XR_TYPE_SPHERICAL_HARMONICS_ANDROID};
  ambient_harmonics.kind = XR_SPHERICAL_HARMONICS_KIND_AMBIENT_ANDROID;
  next_chain.Add(&ambient_harmonics);

  // We have to allocate the OpenXr struct outside of any if block that would
  // append it so that it can exist for OpenXR and also be referenced later.
  device::mojom::XRReflectionProbePtr reflection_probe = nullptr;
  XrCubemapLightingDataANDROID cubemap_data = {
      .type = XR_TYPE_CUBEMAP_LIGHTING_DATA_ANDROID,
      .state = XR_LIGHT_ESTIMATE_STATE_INVALID_ANDROID};

  auto now = base::TimeTicks::Now();
  bool update_reflection =
      IsReflectionSupported() &&
      (!reflection_probe_ ||
       ((now - last_reflection_probe_update_) > kCubemapUpdateInterval));

  if (update_reflection) {
    reflection_probe = CreateAndConfigureReflectionProbe(cubemap_data);
    next_chain.Add(&cubemap_data);
  }

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
  constexpr size_t kNumChannels = 3;
  constexpr size_t kRedChannel = 0;
  constexpr size_t kGreenChannel = 1;
  constexpr size_t kBlueChannel = 2;

  base::span<float[kNumChannels], kNumShCoefficients> coefficients =
      base::span(ambient_harmonics.coefficients);
  spherical_harmonics->coefficients.reserve(coefficients.size());

  for (auto& coefficient : coefficients) {
    base::span<float, kNumChannels> coefficient_data = base::span(coefficient);
    spherical_harmonics->coefficients.emplace_back(
        coefficient_data[kRedChannel], coefficient_data[kGreenChannel],
        coefficient_data[kBlueChannel]);
  }

  light_probe->main_light_intensity = {directional_light.intensity.x,
                                       directional_light.intensity.y,
                                       directional_light.intensity.z};
  light_probe->main_light_direction = gfx::Vector3dF(
      directional_light.direction.x, directional_light.direction.y,
      directional_light.direction.z);

  // If the reflection cubemap is not enabled or was not intended to be updated
  // this frame, this state will stay invalid, just like if there's an error.
  if (cubemap_data.state == XR_LIGHT_ESTIMATE_STATE_VALID_ANDROID) {
    reflection_probe_ = std::move(reflection_probe);
    last_reflection_probe_update_ = now;
  }

  if (reflection_probe_) {
    light_estimation_data->reflection_probe = reflection_probe_.Clone();
  }

  return light_estimation_data;
}

OpenXrLightEstimatorAndroidFactory::OpenXrLightEstimatorAndroidFactory() =
    default;
OpenXrLightEstimatorAndroidFactory::~OpenXrLightEstimatorAndroidFactory() =
    default;

const base::flat_set<std::string_view>&
OpenXrLightEstimatorAndroidFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_ANDROID_LIGHT_ESTIMATION_EXTENSION_NAME,
       XR_ANDROID_LIGHT_ESTIMATION_CUBEMAP_EXTENSION_NAME});
  return *kExtensions;
}

XRSessionFeatureSet OpenXrLightEstimatorAndroidFactory::GetSupportedFeatures()
    const {
  if (!IsEnabled()) {
    return {};
  }

  return {device::mojom::XRSessionFeature::LIGHT_ESTIMATION};
}

#define OPENXR_LOAD_FN(fn)             \
  PFN_##fn fn = nullptr;               \
  std::ignore = xrGetInstanceProcAddr( \
      instance, #fn, reinterpret_cast<PFN_xrVoidFunction*>(&fn));

void OpenXrLightEstimatorAndroidFactory::CheckAndUpdateEnabledState(
    const OpenXrExtensionEnumeration* extension_enum,
    XrInstance instance,
    XrSystemId system) {
  selected_resolution_ = 0;

  if (!extension_enum->ExtensionSupported(
          XR_ANDROID_LIGHT_ESTIMATION_EXTENSION_NAME)) {
    SetEnabled(false);
    return;
  }

  XrSystemLightEstimationPropertiesANDROID light_estimation_properties{
      XR_TYPE_SYSTEM_LIGHT_ESTIMATION_PROPERTIES_ANDROID};

  XrSystemProperties system_properties{XR_TYPE_SYSTEM_PROPERTIES};
  system_properties.next = &light_estimation_properties;

  bool lighting_supported = false;
  XrResult result = xrGetSystemProperties(instance, system, &system_properties);
  if (XR_SUCCEEDED(result)) {
    lighting_supported = light_estimation_properties.supportsLightEstimation;
  }

  SetEnabled(lighting_supported);
  if (!base::FeatureList::IsEnabled(features::kOpenXrAndroidCubeMap)) {
    return;
  }

  if (!extension_enum->ExtensionSupported(
          XR_ANDROID_LIGHT_ESTIMATION_CUBEMAP_EXTENSION_NAME)) {
    return;
  }

  XrSystemCubemapLightEstimationPropertiesANDROID cubemap_properties = {
      .type = XR_TYPE_SYSTEM_CUBEMAP_LIGHT_ESTIMATION_PROPERTIES_ANDROID};
  system_properties.next = &cubemap_properties;
  result = xrGetSystemProperties(instance, system, &system_properties);
  if (XR_FAILED(result) || !cubemap_properties.supportsCubemapLightEstimation) {
    return;
  }

  // Load extension functions locally.
  OPENXR_LOAD_FN(xrEnumerateCubemapLightingResolutionsANDROID);
  OPENXR_LOAD_FN(xrEnumerateCubemapLightingColorFormatsANDROID);

  if (!xrEnumerateCubemapLightingResolutionsANDROID ||
      !xrEnumerateCubemapLightingColorFormatsANDROID) {
    return;
  }

  // Verify color format support (we require R16G16B16A16_SFLOAT_ANDROID).
  uint32_t format_count = 0;
  result = xrEnumerateCubemapLightingColorFormatsANDROID(
      instance, system, 0, &format_count, nullptr);
  if (XR_FAILED(result) || format_count == 0) {
    return;
  }

  std::vector<XrCubemapLightingColorFormatANDROID> formats(format_count);
  result = xrEnumerateCubemapLightingColorFormatsANDROID(
      instance, system, format_count, &format_count, formats.data());
  if (XR_FAILED(result)) {
    return;
  }

  if (!std::ranges::contains(formats, kRequiredColorFormat)) {
    return;
  }

  // Now that we've confirmed our color format is supported, check that we have
  // a resolution we are okay to support, and cache it for later use.
  uint32_t resolution_count = 0;
  result = xrEnumerateCubemapLightingResolutionsANDROID(
      instance, system, 0, &resolution_count, nullptr);
  if (XR_FAILED(result) || resolution_count == 0) {
    return;
  }

  std::vector<uint32_t> resolutions(resolution_count);
  result = xrEnumerateCubemapLightingResolutionsANDROID(
      instance, system, resolution_count, &resolution_count,
      resolutions.data());
  if (XR_FAILED(result)) {
    return;
  }

  // Select our preferred resolution if we can, otherwise select the first
  // resolution that is within our bounds of accepted resolutions.
  if (std::ranges::contains(resolutions, kPreferredCubeMapResolution)) {
    selected_resolution_ = kPreferredCubeMapResolution;
  } else {
    auto it = std::ranges::find_if(resolutions, [](uint32_t resolution) {
      return resolution >= kMinCubeMapResolution &&
             resolution <= kMaxCubeMapResolution;
    });
    if (it != resolutions.end()) {
      selected_resolution_ = *it;
    }
  }
}
#undef OPENXR_LOAD_FN

std::unique_ptr<OpenXrLightEstimator>
OpenXrLightEstimatorAndroidFactory::CreateLightEstimator(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space) const {
  bool is_supported = IsEnabled();
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrLightEstimatorAndroid>(
        extension_helper, session, mojo_space, selected_resolution_);
  }

  return nullptr;
}

}  // namespace device
