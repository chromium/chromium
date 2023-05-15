// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_OPENXR_SCENE_OBJECT_H_
#define DEVICE_VR_OPENXR_OPENXR_SCENE_OBJECT_H_

#include "base/types/id_type.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

struct OpenXrSceneObject {  // XR_SCENE_COMPONENT_TYPE_OBJECT_MSFT
  OpenXrSceneObject();
  ~OpenXrSceneObject();
  using Id = base::StrongAlias<OpenXrSceneObject, XrUuidMSFT>;
  using Type = ::XrSceneObjectTypeMSFT;
  OpenXrSceneObject::Id id_;
  OpenXrSceneObject::Id parent_id_;
  XrTime update_time;
  Type type_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SCENE_OBJECT_H_
