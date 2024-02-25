// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_PLANE_MSFT_H_
#define DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_PLANE_MSFT_H_

#include "base/types/id_type.h"
#include "device/vr/openxr/msft/openxr_scene_object_msft.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

struct OpenXrScenePlaneMsft {  // XR_SCENE_COMPONENT_TYPE_PLANE_MSFT
  using Id = base::StrongAlias<OpenXrScenePlaneMsft, XrUuidMSFT>;
  using Alignment = ::XrScenePlaneAlignmentTypeMSFT;
  using Extent = XrExtent2Df;
  OpenXrScenePlaneMsft(const XrSceneComponentMSFT& component,
                       const XrScenePlaneMSFT& plane);
  OpenXrScenePlaneMsft(const OpenXrScenePlaneMsft& other);
  ~OpenXrScenePlaneMsft();
  OpenXrScenePlaneMsft& operator=(const OpenXrScenePlaneMsft& other);

  OpenXrScenePlaneMsft::Id id_;
  OpenXrSceneObjectMsft::Id parent_id_;
  XrTime update_time_;
  Alignment alignment_;
  Extent size_;
  uint64_t mesh_buffer_id_;
  bool supports_indices_uint16_;
  XrSceneComponentLocationMSFT location_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_PLANE_MSFT_H_
