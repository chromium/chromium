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

namespace {
// A helper macro to avoid the repetitive boilerplate required to load a
// function from OpenXR. Note that these are typically extension functions and
// that the name of the method is repeated several times throughout the
// declaration. For a function named "xrCreateHandTrackerEXT" this will exapnd
// to the following:
// std::ignore = xrGetInstanceProcAddr(
//     instance, "xrCreateHandTrackerEXT",
//     reinterpret_cast<PFN_xrVoidFunction*>(
//         const_cast<PFN_xrCreateHandTrackerEXT*>(
//             &extension_methods_.xrCreateHandTrackerEXT)));
#define OPENXR_LOAD_FN(name)                 \
  std::ignore = xrGetInstanceProcAddr(       \
      instance, #name,                       \
      reinterpret_cast<PFN_xrVoidFunction*>( \
          const_cast<PFN_##name*>(&extension_methods_.name)))

// The ExtensionHandlers are all created/checked in very similar ways, with the
// only difference really being what method they need called on the
// OpenXrExtensionHandler factory to be created (and thus the arguments that
// they need to have supplied). This helper abstracts the boilerplate so that
// we can simply use a factory lambda which has that knowledge, and this helper
// then takes care of the rest of the boilerplate.
template <typename T, typename FunctionType>
std::unique_ptr<T> CreateExtensionHandler(
    const OpenXrExtensionEnumeration* extension_enum,
    FunctionType fn) {
  for (const auto* factory : GetExtensionHandlerFactories()) {
    CHECK(factory);
    if (factory->IsEnabled(extension_enum)) {
      auto ret = fn(*factory);
      if (ret != nullptr) {
        return ret;
      }
    }
  }

  return nullptr;
}
}  // namespace

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
  OPENXR_LOAD_FN(xrCreateHandTrackerEXT);
  OPENXR_LOAD_FN(xrDestroyHandTrackerEXT);
  OPENXR_LOAD_FN(xrLocateHandJointsEXT);

  // Anchors methods
  OPENXR_LOAD_FN(xrCreateSpatialAnchorMSFT);
  OPENXR_LOAD_FN(xrDestroySpatialAnchorMSFT);
  OPENXR_LOAD_FN(xrCreateSpatialAnchorSpaceMSFT);

  // MSFT Scene Understanding Methods
  OPENXR_LOAD_FN(xrEnumerateSceneComputeFeaturesMSFT);
  OPENXR_LOAD_FN(xrCreateSceneObserverMSFT);
  OPENXR_LOAD_FN(xrDestroySceneObserverMSFT);
  OPENXR_LOAD_FN(xrCreateSceneMSFT);
  OPENXR_LOAD_FN(xrDestroySceneMSFT);
  OPENXR_LOAD_FN(xrComputeNewSceneMSFT);
  OPENXR_LOAD_FN(xrGetSceneComputeStateMSFT);
  OPENXR_LOAD_FN(xrGetSceneComponentsMSFT);
  OPENXR_LOAD_FN(xrLocateSceneComponentsMSFT);
  OPENXR_LOAD_FN(xrGetSceneMeshBuffersMSFT);

#if BUILDFLAG(IS_WIN)
  OPENXR_LOAD_FN(xrConvertWin32PerformanceCounterToTimeKHR);
#endif

#if BUILDFLAG(IS_ANDROID)
  OPENXR_LOAD_FN(xrGetReferenceSpaceBoundsPolygonANDROID);

  OPENXR_LOAD_FN(xrCreateTrackableTrackerANDROID);
  OPENXR_LOAD_FN(xrDestroyTrackableTrackerANDROID);

  OPENXR_LOAD_FN(xrRaycastANDROID);

  OPENXR_LOAD_FN(xrCreateAnchorSpaceANDROID);

  OPENXR_LOAD_FN(xrCreateLightEstimatorANDROID);
  OPENXR_LOAD_FN(xrDestroyLightEstimatorANDROID);
  OPENXR_LOAD_FN(xrGetLightEstimateANDROID);

  OPENXR_LOAD_FN(xrCreateDepthSwapchainANDROID);
  OPENXR_LOAD_FN(xrDestroyDepthSwapchainANDROID);
  OPENXR_LOAD_FN(xrEnumerateDepthSwapchainImagesANDROID);
  OPENXR_LOAD_FN(xrEnumerateDepthResolutionsANDROID);
  OPENXR_LOAD_FN(xrAcquireDepthSwapchainImagesANDROID);
#endif
}

bool OpenXrExtensionHelper::IsFeatureSupported(
    device::mojom::XRSessionFeature feature) const {
  const auto* extension_enum = ExtensionEnumeration();
  switch (feature) {
    case device::mojom::XRSessionFeature::ANCHORS:
    case device::mojom::XRSessionFeature::DEPTH:
    case device::mojom::XRSessionFeature::HAND_INPUT:
    case device::mojom::XRSessionFeature::HIT_TEST:
    case device::mojom::XRSessionFeature::LIGHT_ESTIMATION:
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
  return CreateExtensionHandler<OpenXrAnchorManager>(
      ExtensionEnumeration(),
      [this, session,
       base_space](const OpenXrExtensionHandlerFactory& factory) {
        return factory.CreateAnchorManager(*this, session, base_space);
      });
}

std::unique_ptr<OpenXrDepthSensor> OpenXrExtensionHelper::CreateDepthSensor(
    XrSession session,
    XrSpace base_space,
    const mojom::XRDepthOptions& depth_options) const {
  return CreateExtensionHandler<OpenXrDepthSensor>(
      ExtensionEnumeration(),
      [this, session, base_space,
       depth_options](const OpenXrExtensionHandlerFactory& factory)
          -> std::unique_ptr<OpenXrDepthSensor> {
        auto sensor = factory.CreateDepthSensor(*this, session, base_space,
                                                depth_options);
        if (sensor && XR_SUCCEEDED(sensor->Initialize())) {
          return sensor;
        }

        return nullptr;
      });
}

std::unique_ptr<OpenXrHandTracker> OpenXrExtensionHelper::CreateHandTracker(
    XrSession session,
    OpenXrHandednessType handedness) const {
  return CreateExtensionHandler<OpenXrHandTracker>(
      ExtensionEnumeration(),
      [this, session,
       handedness](const OpenXrExtensionHandlerFactory& factory) {
        return factory.CreateHandTracker(*this, session, handedness);
      });
}

std::unique_ptr<OpenXrLightEstimator>
OpenXrExtensionHelper::CreateLightEstimator(XrSession session,
                                            XrSpace base_space) const {
  return CreateExtensionHandler<OpenXrLightEstimator>(
      ExtensionEnumeration(),
      [this, session,
       base_space](const OpenXrExtensionHandlerFactory& factory) {
        return factory.CreateLightEstimator(*this, session, base_space);
      });
}

std::unique_ptr<OpenXRSceneUnderstandingManager>
OpenXrExtensionHelper::CreateSceneUnderstandingManager(
    XrSession session,
    XrSpace base_space) const {
  return CreateExtensionHandler<OpenXRSceneUnderstandingManager>(
      ExtensionEnumeration(),
      [this, session,
       base_space](const OpenXrExtensionHandlerFactory& factory) {
        return factory.CreateSceneUnderstandingManager(*this, session,
                                                       base_space);
      });
}

std::unique_ptr<OpenXrStageBoundsProvider>
OpenXrExtensionHelper::CreateStageBoundsProvider(XrSession session) const {
  return CreateExtensionHandler<OpenXrStageBoundsProvider>(
      ExtensionEnumeration(),
      [this, session](const OpenXrExtensionHandlerFactory& factory) {
        return factory.CreateStageBoundsProvider(*this, session);
      });
}

std::unique_ptr<OpenXrUnboundedSpaceProvider>
OpenXrExtensionHelper::CreateUnboundedSpaceProvider() const {
  return CreateExtensionHandler<OpenXrUnboundedSpaceProvider>(
      ExtensionEnumeration(),
      [this](const OpenXrExtensionHandlerFactory& factory) {
        return factory.CreateUnboundedSpaceProvider(*this);
      });
}

}  // namespace device
