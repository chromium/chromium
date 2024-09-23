// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_extension_handler_factory.h"

#include <memory>

#include "base/ranges/algorithm.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
OpenXrExtensionHandlerFactory::OpenXrExtensionHandlerFactory() = default;
OpenXrExtensionHandlerFactory::~OpenXrExtensionHandlerFactory() = default;

bool OpenXrExtensionHandlerFactory::IsEnabled(
    const OpenXrExtensionEnumeration* extension_enum) const {
  return supported_by_system_properties_ &&
         AreAllRequestedExtensionsSupported(extension_enum);
}

void OpenXrExtensionHandlerFactory::ProcessSystemProperties(
    const OpenXrExtensionEnumeration* extension_enum,
    XrInstance instance,
    XrSystemId system) {
  SetSystemPropertiesSupport(true);
}

std::unique_ptr<OpenXrAnchorManager>
OpenXrExtensionHandlerFactory::CreateAnchorManager(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space) const {
  return nullptr;
}

std::unique_ptr<OpenXrDepthSensor>
OpenXrExtensionHandlerFactory::CreateDepthSensor(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space,
    const mojom::XRDepthOptions& depth_options) const {
  return nullptr;
}

std::unique_ptr<OpenXrHandTracker>
OpenXrExtensionHandlerFactory::CreateHandTracker(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    OpenXrHandednessType type) const {
  return nullptr;
}

std::unique_ptr<OpenXrLightEstimator>
OpenXrExtensionHandlerFactory::CreateLightEstimator(
    const OpenXrExtensionHelper& extenion_helper,
    XrSession session,
    XrSpace local_space) const {
  return nullptr;
}

std::unique_ptr<OpenXRSceneUnderstandingManager>
OpenXrExtensionHandlerFactory::CreateSceneUnderstandingManager(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space) const {
  return nullptr;
}

std::unique_ptr<OpenXrStageBoundsProvider>
OpenXrExtensionHandlerFactory::CreateStageBoundsProvider(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session) const {
  return nullptr;
}

std::unique_ptr<OpenXrUnboundedSpaceProvider>
OpenXrExtensionHandlerFactory::CreateUnboundedSpaceProvider(
    const OpenXrExtensionHelper& extension_helper) const {
  return nullptr;
}

bool OpenXrExtensionHandlerFactory::AreAllRequestedExtensionsSupported(
    const OpenXrExtensionEnumeration* extension_enum) const {
  return base::ranges::all_of(
      GetRequestedExtensions(),
      [&extension_enum](std::string_view extension_name) {
        return extension_enum->ExtensionSupported(extension_name.data());
      });
}

void OpenXrExtensionHandlerFactory::SetSystemPropertiesSupport(bool supported) {
  supported_by_system_properties_ = supported;
}

}  // namespace device
