// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_EXTENSION_HELPER_H_
#define DEVICE_VR_OPENXR_OPENXR_EXTENSION_HELPER_H_

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "build/buildflag.h"
#include "device/vr/openxr/openxr_anchor_manager.h"
#include "device/vr/openxr/openxr_depth_sensor.h"
#include "device/vr/openxr/openxr_hand_tracker.h"
#include "device/vr/openxr/openxr_light_estimator.h"
#include "device/vr/openxr/openxr_platform.h"
#include "device/vr/openxr/openxr_scene_understanding_manager.h"
#include "device/vr/openxr/openxr_stage_bounds_provider.h"
#include "device/vr/openxr/openxr_unbounded_space_provider.h"
#include "device/vr/public/mojom/xr_session.mojom-forward.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

#if BUILDFLAG(IS_ANDROID)
#include "third_party/openxr/dev/xr_android.h"
#endif

namespace device {
// Helper macro to facilitate declaring the method names of functions that will
// be loaded from the OpenXR Runtime.
// Expands to e.g.
// PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT{nullptr};
#define OPENXR_DECLARE_FN(name) PFN_##name name = nullptr

struct OpenXrExtensionMethods {
  OpenXrExtensionMethods();
  ~OpenXrExtensionMethods();
  // Hand Tracking
  OPENXR_DECLARE_FN(xrCreateHandTrackerEXT);
  OPENXR_DECLARE_FN(xrDestroyHandTrackerEXT);
  OPENXR_DECLARE_FN(xrLocateHandJointsEXT);

  // Anchors
  OPENXR_DECLARE_FN(xrCreateSpatialAnchorMSFT);
  OPENXR_DECLARE_FN(xrDestroySpatialAnchorMSFT);
  OPENXR_DECLARE_FN(xrCreateSpatialAnchorSpaceMSFT);

  // Scene Understanding
  OPENXR_DECLARE_FN(xrEnumerateSceneComputeFeaturesMSFT);
  OPENXR_DECLARE_FN(xrCreateSceneObserverMSFT);
  OPENXR_DECLARE_FN(xrDestroySceneObserverMSFT);
  OPENXR_DECLARE_FN(xrCreateSceneMSFT);
  OPENXR_DECLARE_FN(xrDestroySceneMSFT);
  OPENXR_DECLARE_FN(xrComputeNewSceneMSFT);
  OPENXR_DECLARE_FN(xrGetSceneComputeStateMSFT);
  OPENXR_DECLARE_FN(xrGetSceneComponentsMSFT);
  OPENXR_DECLARE_FN(xrLocateSceneComponentsMSFT);
  OPENXR_DECLARE_FN(xrGetSceneMeshBuffersMSFT);

#if BUILDFLAG(IS_WIN)
  // Time
  OPENXR_DECLARE_FN(xrConvertWin32PerformanceCounterToTimeKHR);
#endif

  // While these extensions don't need to be gated to a particular platform,
  // since the API is still under development we'll try to limit the scope for
  // the time being.
#if BUILDFLAG(IS_ANDROID)
  OPENXR_DECLARE_FN(xrGetReferenceSpaceBoundsPolygonANDROID);

  // Trackables and Raycasting.
  OPENXR_DECLARE_FN(xrCreateTrackableTrackerANDROID);
  OPENXR_DECLARE_FN(xrDestroyTrackableTrackerANDROID);
  OPENXR_DECLARE_FN(xrRaycastANDROID);

  OPENXR_DECLARE_FN(xrCreateAnchorSpaceANDROID);

  OPENXR_DECLARE_FN(xrCreateLightEstimatorANDROID);
  OPENXR_DECLARE_FN(xrDestroyLightEstimatorANDROID);
  OPENXR_DECLARE_FN(xrGetLightEstimateANDROID);

  OPENXR_DECLARE_FN(xrCreateDepthSwapchainANDROID);
  OPENXR_DECLARE_FN(xrDestroyDepthSwapchainANDROID);
  OPENXR_DECLARE_FN(xrEnumerateDepthSwapchainImagesANDROID);
  OPENXR_DECLARE_FN(xrEnumerateDepthResolutionsANDROID);
  OPENXR_DECLARE_FN(xrAcquireDepthSwapchainImagesANDROID);
#endif
};
// Ensure that we don't export our helper macro.
#undef OPENXR_DECLARE_FN

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

  // Returns whether or not we can support a given feature. If a given feature
  // is determined to be supported solely by the core spec, we will simply
  // return true for that feature as we assume the entire core spec is
  // supported.
  bool IsFeatureSupported(device::mojom::XRSessionFeature feature) const;

  // Feature Implementation Helpers ---------------------------------------
  //
  // There may be multiple extensions that can support a given WebXR feature,
  // though each device will likely only implement one of them. The following
  // methods help provide managers for the various WebXR features that can
  // abstract the *actual* extension that we need to use, since different
  // extensions will be looking for different methods.

  std::unique_ptr<OpenXrAnchorManager> CreateAnchorManager(
      XrSession session,
      XrSpace base_space) const;

  std::unique_ptr<OpenXrDepthSensor> CreateDepthSensor(
      XrSession session,
      XrSpace base_space,
      const mojom::XRDepthOptions& depth_options) const;

  std::unique_ptr<OpenXrHandTracker> CreateHandTracker(
      XrSession session,
      OpenXrHandednessType handedness) const;

  std::unique_ptr<OpenXrLightEstimator> CreateLightEstimator(
      XrSession session,
      XrSpace base_space) const;

  std::unique_ptr<OpenXRSceneUnderstandingManager>
  CreateSceneUnderstandingManager(XrSession session, XrSpace base_space) const;

  std::unique_ptr<OpenXrStageBoundsProvider> CreateStageBoundsProvider(
      XrSession session) const;

  std::unique_ptr<OpenXrUnboundedSpaceProvider> CreateUnboundedSpaceProvider()
      const;

 private:
  // Small helper method to check if a given extension is enabled.
  bool IsExtensionSupported(const char* extension_name) const;

  const OpenXrExtensionMethods extension_methods_;
  const raw_ptr<const OpenXrExtensionEnumeration> extension_enumeration_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_EXTENSION_HELPER_H_
