// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_extension_helper.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "device/vr/openxr/openxr_anchor_manager.h"
#include "device/vr/openxr/openxr_scene_understanding_manager.h"
#include "device/vr/openxr/openxr_stage_bounds_provider_basic.h"
#include "device/vr/public/mojom/xr_session.mojom.h"

namespace device {

OpenXrExtensionMethods::OpenXrExtensionMethods() = default;
OpenXrExtensionMethods::~OpenXrExtensionMethods() = default;

OpenXrExtensionEnumeration::OpenXrExtensionEnumeration() {
  uint32_t extension_count;
  if (XR_SUCCEEDED(xrEnumerateInstanceExtensionProperties(
          nullptr, 0, &extension_count, nullptr))) {
    extension_properties_.resize(extension_count,
                                 {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, extension_count,
                                           &extension_count,
                                           extension_properties_.data());
  }
}

OpenXrExtensionEnumeration::~OpenXrExtensionEnumeration() = default;

bool OpenXrExtensionEnumeration::ExtensionSupported(
    const char* extension_name) const {
  return base::ranges::any_of(
      extension_properties_,
      [&extension_name](const XrExtensionProperties& properties) {
        return strcmp(properties.extensionName, extension_name) == 0;
      });
}

OpenXrExtensionHelper::~OpenXrExtensionHelper() = default;

OpenXrExtensionHelper::OpenXrExtensionHelper(
    XrInstance instance,
    const OpenXrExtensionEnumeration* const extension_enumeration)
    : extension_enumeration_(extension_enumeration) {
  // Failure to query a method results in a nullptr

  // Hand tracking methods
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrCreateHandTrackerEXT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrCreateHandTrackerEXT*>(
              &extension_methods_.xrCreateHandTrackerEXT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrDestroyHandTrackerEXT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrDestroyHandTrackerEXT*>(
              &extension_methods_.xrDestroyHandTrackerEXT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrLocateHandJointsEXT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrLocateHandJointsEXT*>(
              &extension_methods_.xrLocateHandJointsEXT)));

  // Anchors methods
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrCreateSpatialAnchorMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrCreateSpatialAnchorMSFT*>(
              &extension_methods_.xrCreateSpatialAnchorMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrDestroySpatialAnchorMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrDestroySpatialAnchorMSFT*>(
              &extension_methods_.xrDestroySpatialAnchorMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrCreateSpatialAnchorSpaceMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrCreateSpatialAnchorSpaceMSFT*>(
              &extension_methods_.xrCreateSpatialAnchorSpaceMSFT)));

  std::ignore = xrGetInstanceProcAddr(
      instance, "xrEnumerateSceneComputeFeaturesMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrEnumerateSceneComputeFeaturesMSFT*>(
              &extension_methods_.xrEnumerateSceneComputeFeaturesMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrCreateSceneObserverMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrCreateSceneObserverMSFT*>(
              &extension_methods_.xrCreateSceneObserverMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrDestroySceneObserverMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrDestroySceneObserverMSFT*>(
              &extension_methods_.xrDestroySceneObserverMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrCreateSceneMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(const_cast<PFN_xrCreateSceneMSFT*>(
          &extension_methods_.xrCreateSceneMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrDestroySceneMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(const_cast<PFN_xrDestroySceneMSFT*>(
          &extension_methods_.xrDestroySceneMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrComputeNewSceneMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrComputeNewSceneMSFT*>(
              &extension_methods_.xrComputeNewSceneMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrGetSceneComputeStateMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrGetSceneComputeStateMSFT*>(
              &extension_methods_.xrGetSceneComputeStateMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrGetSceneComponentsMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrGetSceneComponentsMSFT*>(
              &extension_methods_.xrGetSceneComponentsMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrLocateSceneComponentsMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrLocateSceneComponentsMSFT*>(
              &extension_methods_.xrLocateSceneComponentsMSFT)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrGetSceneMeshBuffersMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrGetSceneMeshBuffersMSFT*>(
              &extension_methods_.xrGetSceneMeshBuffersMSFT)));

#if BUILDFLAG(IS_WIN)
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrConvertWin32PerformanceCounterToTimeKHR",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrConvertWin32PerformanceCounterToTimeKHR*>(
              &extension_methods_.xrConvertWin32PerformanceCounterToTimeKHR)));
#endif
}

bool OpenXrExtensionHelper::IsFeatureSupported(
    device::mojom::XRSessionFeature feature) const {
  switch (feature) {
    case device::mojom::XRSessionFeature::ANCHORS:
      return IsExtensionSupported(XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME);
    case device::mojom::XRSessionFeature::HAND_INPUT:
      // We need the XR_EXT_HAND_TRACKING extension in order to supply the hand
      // mesh required by the spec for the hand input feature. However, the hand
      // mesh must be tied to an XrInputSource. In order to generate an
      // XrInputSource we need to be able to send up a "primary action" event
      // (i.e. a click), so we need to also check that we have an extension
      // enabled that we can use to generate that.
      return IsExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME) &&
             (IsExtensionSupported(XR_EXT_HAND_INTERACTION_EXTENSION_NAME) ||
              IsExtensionSupported(XR_MSFT_HAND_INTERACTION_EXTENSION_NAME));
    case device::mojom::XRSessionFeature::HIT_TEST:
      return IsExtensionSupported(XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME);
    case device::mojom::XRSessionFeature::SECONDARY_VIEWS:
      return IsExtensionSupported(
          XR_MSFT_SECONDARY_VIEW_CONFIGURATION_EXTENSION_NAME);
    default:
      // By default we assume a feature doesn't need to be supported by an
      // extension unless customized above.
      return true;
  }
}

bool OpenXrExtensionHelper::IsExtensionSupported(
    const char* extension_name) const {
  return extension_enumeration_->ExtensionSupported(extension_name);
}

std::unique_ptr<OpenXrAnchorManager> OpenXrExtensionHelper::CreateAnchorManager(
    XrSession session,
    XrSpace base_space) const {
  return std::make_unique<OpenXrAnchorManager>(*this, session, base_space);
}

std::unique_ptr<OpenXrHandTracker> OpenXrExtensionHelper::CreateHandTracker(
    XrSession session,
    OpenXrHandednessType handedness) const {
  if (IsExtensionSupported(XR_EXT_HAND_TRACKING_EXTENSION_NAME)) {
    return std::make_unique<OpenXrHandTracker>(*this, session, handedness);
  }

  return nullptr;
}

std::unique_ptr<OpenXRSceneUnderstandingManager>
OpenXrExtensionHelper::CreateSceneUnderstandingManager(
    XrSession session,
    XrSpace base_space) const {
  return std::make_unique<OpenXRSceneUnderstandingManager>(*this, session,
                                                           base_space);
}

std::unique_ptr<OpenXrStageBoundsProvider>
OpenXrExtensionHelper::CreateStageBoundsProvider(XrSession session) const {
  return std::make_unique<OpenXrStageBoundsProviderBasic>(session);
}

}  // namespace device
