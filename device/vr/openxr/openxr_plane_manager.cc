// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_plane_manager.h"

#include "device/vr/openxr/openxr_api_wrapper.h"

namespace device {

OpenXrPlaneManager::~OpenXrPlaneManager() = default;

mojom::XRPlaneDetectionDataPtr OpenXrPlaneManager::GetDetectedPlanesData() {
  return nullptr;
}

std::optional<XrLocation> OpenXrPlaneManager::GetXrLocationFromPlane(
    PlaneId plane_id,
    const gfx::Transform& plane_id_from_object) const {
  return std::nullopt;
}

}  // namespace device
