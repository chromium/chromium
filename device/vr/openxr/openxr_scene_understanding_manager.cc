// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_scene_understanding_manager.h"

#include <algorithm>

#include "device/vr/openxr/openxr_extension_helper.h"

namespace device {

OpenXRSceneUnderstandingManager::OpenXRSceneUnderstandingManager() = default;
OpenXRSceneUnderstandingManager::~OpenXRSceneUnderstandingManager() = default;

void OpenXRSceneUnderstandingManager::OnDiscoveryRecommended(
    const XrEventDataSpatialDiscoveryRecommendedEXT* event_data) {}

}  // namespace device
