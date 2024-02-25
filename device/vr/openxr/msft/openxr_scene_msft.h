// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_MSFT_H_
#define DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_MSFT_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_generic.h"
#include "device/vr/openxr/openxr_extension_handle.h"
#include "device/vr/openxr/msft/openxr_scene_plane_msft.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

// C++ wrapper for XrSceneMSFT
class OpenXrSceneMsft {
 public:
  OpenXrSceneMsft(const device::OpenXrExtensionHelper& extensions,
              XrSceneObserverMSFT scene_observer);
  ~OpenXrSceneMsft();

  XrResult GetPlanes(std::vector<OpenXrScenePlaneMsft>& out_planes);
  XrResult LocateObjects(XrSpace base_space,
                         XrTime time,
                         std::vector<OpenXrScenePlaneMsft>& planes);

  XrSceneMSFT Handle() const { return scene_.get(); }

 private:
  const raw_ref<const device::OpenXrExtensionHelper> extensions_;
  OpenXrExtensionHandle<XrSceneMSFT> scene_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_MSFT_H_
