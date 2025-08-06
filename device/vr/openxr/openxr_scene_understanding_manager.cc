// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_scene_understanding_manager.h"

#include <algorithm>

#include "device/vr/openxr/openxr_extension_helper.h"

namespace device {

OpenXRSceneUnderstandingManager::OpenXRSceneUnderstandingManager() = default;
OpenXRSceneUnderstandingManager::~OpenXRSceneUnderstandingManager() = default;

bool OpenXrSceneUnderstandingManagerFactory::IsEnabled(
    const OpenXrExtensionEnumeration* extension_enum) const {
  // SceneUnderstandingManagers have multiple components that they enable. By
  // default, we assume that with any of their requested extensions enabled,
  // they can supply at least one component. This is different from a standard
  // ExtensionHandler which requires *all* extensions to be supported.
  return std::ranges::any_of(
      GetRequestedExtensions(), [&extension_enum](std::string_view extension) {
        return extension_enum->ExtensionSupported(extension.data());
      });
}
}  // namespace device
