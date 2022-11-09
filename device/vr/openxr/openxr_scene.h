// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_OPENXR_SCENE_H_
#define DEVICE_VR_OPENXR_OPENXR_SCENE_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_generic.h"
#include "device/vr/openxr/openxr_extension_handle.h"
#include "device/vr/openxr/openxr_scene_plane.h"
#include "device/vr/openxr/openxr_util.h"

namespace device {

// C++ wrapper for XrSceneMSFT
class OpenXrScene {
 public:
  OpenXrScene(const device::OpenXrExtensionHelper& extensions,
              XrSceneObserverMSFT scene_observer);
  ~OpenXrScene();

  XrResult GetPlanes(std::vector<OpenXrScenePlane>& out_planes);
  XrResult LocateObjects(XrSpace base_space,
                         XrTime time,
                         std::vector<OpenXrScenePlane>& planes);

  XrSceneMSFT Handle() const { return scene_.get(); }

 private:
  const raw_ref<const device::OpenXrExtensionHelper> extensions_;
  OpenXrExtensionHandle<XrSceneMSFT> scene_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SCENE_H_
