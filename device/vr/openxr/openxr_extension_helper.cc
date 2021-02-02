// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_extension_helper.h"

namespace device {

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
  return std::find_if(
             extension_properties_.begin(), extension_properties_.end(),
             [&extension_name](const XrExtensionProperties& properties) {
               return strcmp(properties.extensionName, extension_name) == 0;
             }) != extension_properties_.end();
}

OpenXrExtensionHelper::~OpenXrExtensionHelper() = default;

OpenXrExtensionHelper::OpenXrExtensionHelper(
    XrInstance instance,
    const OpenXrExtensionEnumeration* const extension_enumeration)
    : extension_enumeration_(extension_enumeration) {
  // Failure to query a method results in a nullptr

  // D3D11
  (void)xrGetInstanceProcAddr(
      instance, "xrGetD3D11GraphicsRequirementsKHR",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrGetD3D11GraphicsRequirementsKHR*>(
              &extension_methods_.xrGetD3D11GraphicsRequirementsKHR)));

  // Hand tracking methods
  (void)xrGetInstanceProcAddr(
      instance, "xrCreateHandTrackerEXT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrCreateHandTrackerEXT*>(
              &extension_methods_.xrCreateHandTrackerEXT)));
  (void)xrGetInstanceProcAddr(
      instance, "xrDestroyHandTrackerEXT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrDestroyHandTrackerEXT*>(
              &extension_methods_.xrDestroyHandTrackerEXT)));
  (void)xrGetInstanceProcAddr(
      instance, "xrLocateHandJointsEXT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrLocateHandJointsEXT*>(
              &extension_methods_.xrLocateHandJointsEXT)));

  // Anchors methods
  (void)xrGetInstanceProcAddr(
      instance, "xrCreateSpatialAnchorMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrCreateSpatialAnchorMSFT*>(
              &extension_methods_.xrCreateSpatialAnchorMSFT)));
  (void)xrGetInstanceProcAddr(
      instance, "xrDestroySpatialAnchorMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrDestroySpatialAnchorMSFT*>(
              &extension_methods_.xrDestroySpatialAnchorMSFT)));
  (void)xrGetInstanceProcAddr(
      instance, "xrCreateSpatialAnchorSpaceMSFT",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrCreateSpatialAnchorSpaceMSFT*>(
              &extension_methods_.xrCreateSpatialAnchorSpaceMSFT)));
}

}  // namespace device
