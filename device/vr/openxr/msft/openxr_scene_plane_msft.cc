// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/msft/openxr_scene_plane_msft.h"

namespace device {

OpenXrScenePlaneMsft::OpenXrScenePlaneMsft(
    const XrSceneComponentMSFT& component,
    const XrScenePlaneMSFT& plane)
    : id_(OpenXrScenePlaneMsft::Id(component.id)),
      parent_id_(OpenXrSceneObjectMsft::Id(component.parentId)),
      update_time_(component.updateTime),
      alignment_(plane.alignment),
      size_(plane.size),
      mesh_buffer_id_(plane.meshBufferId),
      supports_indices_uint16_(plane.supportsIndicesUint16) {}
OpenXrScenePlaneMsft::OpenXrScenePlaneMsft(const OpenXrScenePlaneMsft& other) =
    default;
OpenXrScenePlaneMsft::~OpenXrScenePlaneMsft() = default;
OpenXrScenePlaneMsft& OpenXrScenePlaneMsft::operator=(
    const OpenXrScenePlaneMsft& other) = default;

}  // namespace device
