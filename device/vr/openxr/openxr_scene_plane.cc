// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_scene_plane.h"

namespace device {

OpenXrScenePlane::OpenXrScenePlane(const XrSceneComponentMSFT& component,
                                   const XrScenePlaneMSFT& plane)
    : id_(OpenXrScenePlane::Id(component.id)),
      parent_id_(OpenXrSceneObject::Id(component.parentId)),
      update_time_(component.updateTime),
      alignment_(plane.alignment),
      size_(plane.size),
      mesh_buffer_id_(plane.meshBufferId),
      supports_indices_uint16_(plane.supportsIndicesUint16) {}
OpenXrScenePlane::OpenXrScenePlane(const OpenXrScenePlane& other) = default;
OpenXrScenePlane::~OpenXrScenePlane() = default;
OpenXrScenePlane& OpenXrScenePlane::operator=(const OpenXrScenePlane& other) =
    default;

}  // namespace device
