// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_OPENXR_SCENE_PLANE_H_
#define DEVICE_VR_OPENXR_OPENXR_SCENE_PLANE_H_

#include "base/types/id_type.h"
#include "device/vr/openxr/openxr_scene_object.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

struct OpenXrScenePlane {  // XR_SCENE_COMPONENT_TYPE_PLANE_MSFT
  using Id = base::StrongAlias<OpenXrScenePlane, XrUuidMSFT>;
  using Alignment = ::XrScenePlaneAlignmentTypeMSFT;
  using Extent = XrExtent2Df;
  OpenXrScenePlane(const XrSceneComponentMSFT& component,
                   const XrScenePlaneMSFT& plane);
  OpenXrScenePlane(const OpenXrScenePlane& other);
  ~OpenXrScenePlane();
  OpenXrScenePlane& operator=(const OpenXrScenePlane& other);

  OpenXrScenePlane::Id id_;
  OpenXrSceneObject::Id parent_id_;
  XrTime update_time_;
  Alignment alignment_;
  Extent size_;
  uint64_t mesh_buffer_id_;
  bool supports_indices_uint16_;
  XrSceneComponentLocationMSFT location_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SCENE_PLANE_H_
