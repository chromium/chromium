// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_extension_helper.h"

#include <tuple>

#include "base/ranges/algorithm.h"

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

  // D3D11
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrGetD3D11GraphicsRequirementsKHR",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrGetD3D11GraphicsRequirementsKHR*>(
              &extension_methods_.xrGetD3D11GraphicsRequirementsKHR)));

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
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrConvertWin32PerformanceCounterToTimeKHR",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrConvertWin32PerformanceCounterToTimeKHR*>(
              &extension_methods_.xrConvertWin32PerformanceCounterToTimeKHR)));
}

}  // namespace device
