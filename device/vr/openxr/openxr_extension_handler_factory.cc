// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_extension_handler_factory.h"

#include <algorithm>
#include <memory>

#include "device/vr/openxr/openxr_extension_helper.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
OpenXrExtensionHandlerFactory::OpenXrExtensionHandlerFactory() = default;
OpenXrExtensionHandlerFactory::~OpenXrExtensionHandlerFactory() = default;

bool OpenXrExtensionHandlerFactory::IsEnabled() const {
  return enabled_;
}

void OpenXrExtensionHandlerFactory::CheckAndUpdateEnabledState(
    const OpenXrExtensionEnumeration* extension_enum,
    XrInstance instance,
    XrSystemId system) {
  SetEnabled(AreAllRequestedExtensionsSupported(extension_enum));
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
    OpenXrApiWrapper* openxr,
    XrSpace mojo_space) const {
  return nullptr;
}

std::unique_ptr<OpenXrStageBoundsProvider>
OpenXrExtensionHandlerFactory::CreateStageBoundsProvider(
    XrSession session) const {
  return nullptr;
}

std::unique_ptr<OpenXrUnboundedSpaceProvider>
OpenXrExtensionHandlerFactory::CreateUnboundedSpaceProvider() const {
  return nullptr;
}

bool OpenXrExtensionHandlerFactory::AreAllRequestedExtensionsSupported(
    const OpenXrExtensionEnumeration* extension_enum) const {
  return std::ranges::all_of(
      GetRequestedExtensions(),
      [&extension_enum](std::string_view extension_name) {
        return extension_enum->ExtensionSupported(extension_name.data());
      });
}

void OpenXrExtensionHandlerFactory::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

}  // namespace device
