// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_EXTENSION_HELPER_H_
#define DEVICE_VR_OPENXR_OPENXR_EXTENSION_HELPER_H_

#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "device/vr/openxr/openxr_platform.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
struct OpenXrExtensionMethods {
  OpenXrExtensionMethods();
  ~OpenXrExtensionMethods();
  // Hand Tracking
  PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT{nullptr};
  PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT{nullptr};
  PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT{nullptr};

  // Anchors
  PFN_xrCreateSpatialAnchorMSFT xrCreateSpatialAnchorMSFT{nullptr};
  PFN_xrDestroySpatialAnchorMSFT xrDestroySpatialAnchorMSFT{nullptr};
  PFN_xrCreateSpatialAnchorSpaceMSFT xrCreateSpatialAnchorSpaceMSFT{nullptr};

  // Scene Understanding
  PFN_xrEnumerateSceneComputeFeaturesMSFT xrEnumerateSceneComputeFeaturesMSFT{
      nullptr};
  PFN_xrCreateSceneObserverMSFT xrCreateSceneObserverMSFT{nullptr};
  PFN_xrDestroySceneObserverMSFT xrDestroySceneObserverMSFT{nullptr};
  PFN_xrCreateSceneMSFT xrCreateSceneMSFT{nullptr};
  PFN_xrDestroySceneMSFT xrDestroySceneMSFT{nullptr};
  PFN_xrComputeNewSceneMSFT xrComputeNewSceneMSFT{nullptr};
  PFN_xrGetSceneComputeStateMSFT xrGetSceneComputeStateMSFT{nullptr};
  PFN_xrGetSceneComponentsMSFT xrGetSceneComponentsMSFT{nullptr};
  PFN_xrLocateSceneComponentsMSFT xrLocateSceneComponentsMSFT{nullptr};
  PFN_xrGetSceneMeshBuffersMSFT xrGetSceneMeshBuffersMSFT{nullptr};

#if BUILDFLAG(IS_WIN)
  // Time
  PFN_xrConvertWin32PerformanceCounterToTimeKHR
      xrConvertWin32PerformanceCounterToTimeKHR{nullptr};
#endif
};

class OpenXrExtensionEnumeration {
 public:
  OpenXrExtensionEnumeration();
  ~OpenXrExtensionEnumeration();

  bool ExtensionSupported(const char* extension_name) const;

 private:
  std::vector<XrExtensionProperties> extension_properties_;
};

class OpenXrExtensionHelper {
 public:
  OpenXrExtensionHelper(
      XrInstance instance,
      const OpenXrExtensionEnumeration* const extension_enumeration);
  ~OpenXrExtensionHelper();

  const OpenXrExtensionEnumeration* ExtensionEnumeration() const {
    return extension_enumeration_;
  }

  const OpenXrExtensionMethods& ExtensionMethods() const {
    return extension_methods_;
  }

 private:
  const OpenXrExtensionMethods extension_methods_;
  const raw_ptr<const OpenXrExtensionEnumeration> extension_enumeration_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_EXTENSION_HELPER_H_
