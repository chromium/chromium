// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_extension_helper.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"
#include "device/vr/openxr/openxr_extension_handler_factories.h"
#include "device/vr/openxr/openxr_extension_handler_factory.h"
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

  if constexpr (DCHECK_IS_ON()) {
    DVLOG(1) << __func__ << ": Supported Extensions Begin";
    for (const auto& extension : extension_properties_) {
      DVLOG(1) << __func__ << ": " << extension.extensionName
               << " version=" << extension.extensionVersion;
    }
    DVLOG(1) << __func__ << ": Supported Extensions End";
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

  // MSFT Scene Understanding Methods
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

#if BUILDFLAG(IS_ANDROID)
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrGetReferenceSpaceBoundsPolygonANDROID",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrGetReferenceSpaceBoundsPolygonANDROID*>(
              &extension_methods_.xrGetReferenceSpaceBoundsPolygonANDROID)));

  std::ignore = xrGetInstanceProcAddr(
      instance, "xrCreateTrackableTrackerANDROID",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrCreateTrackableTrackerANDROID*>(
              &extension_methods_.xrCreateTrackableTrackerANDROID)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrDestroyTrackableTrackerANDROID",
      reinterpret_cast<PFN_xrVoidFunction*>(
          const_cast<PFN_xrDestroyTrackableTrackerANDROID*>(
              &extension_methods_.xrDestroyTrackableTrackerANDROID)));
  std::ignore = xrGetInstanceProcAddr(
      instance, "xrRaycastANDROID",
      reinterpret_cast<PFN_xrVoidFunction*>(const_cast<PFN_xrRaycastANDROID*>(
          &extension_methods_.xrRaycastANDROID)));
#endif
}

bool OpenXrExtensionHelper::IsFeatureSupported(
    device::mojom::XRSessionFeature feature) const {
  const auto* extension_enum = ExtensionEnumeration();
  switch (feature) {
    case device::mojom::XRSessionFeature::ANCHORS:
    case device::mojom::XRSessionFeature::HAND_INPUT:
    case device::mojom::XRSessionFeature::HIT_TEST:
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
      return base::ranges::any_of(
          GetExtensionHandlerFactories(),
          [feature, &extension_enum](const auto* extension_handler_factory) {
            return base::Contains(
                extension_handler_factory->GetSupportedFeatures(extension_enum),
                feature);
          });
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
  for (const auto* factory : GetExtensionHandlerFactories()) {
    if (factory->IsEnabled(ExtensionEnumeration())) {
      auto ret = factory->CreateAnchorManager(*this, session, base_space);
      if (ret != nullptr) {
        return ret;
      }
    }
  }

  return nullptr;
}

std::unique_ptr<OpenXrHandTracker> OpenXrExtensionHelper::CreateHandTracker(
    XrSession session,
    OpenXrHandednessType handedness) const {
  for (const auto* factory : GetExtensionHandlerFactories()) {
    if (factory->IsEnabled(ExtensionEnumeration())) {
      auto ret = factory->CreateHandTracker(*this, session, handedness);
      if (ret != nullptr) {
        return ret;
      }
    }
  }

  return nullptr;
}

std::unique_ptr<OpenXRSceneUnderstandingManager>
OpenXrExtensionHelper::CreateSceneUnderstandingManager(
    XrSession session,
    XrSpace base_space) const {
  for (const auto* factory : GetExtensionHandlerFactories()) {
    if (factory->IsEnabled(ExtensionEnumeration())) {
      auto ret =
          factory->CreateSceneUnderstandingManager(*this, session, base_space);
      if (ret != nullptr) {
        return ret;
      }
    }
  }

  return nullptr;
}

std::unique_ptr<OpenXrStageBoundsProvider>
OpenXrExtensionHelper::CreateStageBoundsProvider(XrSession session) const {
  for (const auto* factory : GetExtensionHandlerFactories()) {
    if (factory->IsEnabled(ExtensionEnumeration())) {
      auto ret = factory->CreateStageBoundsProvider(*this, session);
      if (ret != nullptr) {
        return ret;
      }
    }
  }

  return nullptr;
}

std::unique_ptr<OpenXrUnboundedSpaceProvider>
OpenXrExtensionHelper::CreateUnboundedSpaceProvider() const {
  for (const auto* factory : GetExtensionHandlerFactories()) {
    if (factory->IsEnabled(ExtensionEnumeration())) {
      auto ret = factory->CreateUnboundedSpaceProvider(*this);
      if (ret != nullptr) {
        return ret;
      }
    }
  }

  return nullptr;
}

}  // namespace device
