// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_OBJECT_MSFT_H_
#define DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_OBJECT_MSFT_H_

#include "base/types/id_type.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

struct OpenXrSceneObjectMsft {  // XR_SCENE_COMPONENT_TYPE_OBJECT_MSFT
  OpenXrSceneObjectMsft();
  ~OpenXrSceneObjectMsft();
  using Id = base::StrongAlias<OpenXrSceneObjectMsft, XrUuidMSFT>;
  using Type = ::XrSceneObjectTypeMSFT;
  OpenXrSceneObjectMsft::Id id_;
  OpenXrSceneObjectMsft::Id parent_id_;
  XrTime update_time;
  Type type_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_OBJECT_MSFT_H_
