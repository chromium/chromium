// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_OPENXR_SCENE_OBSERVER_H_
#define DEVICE_VR_OPENXR_OPENXR_SCENE_OBSERVER_H_

#include "base/check.h"
#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_extension_handle.h"
#include "device/vr/openxr/openxr_scene.h"
#include "device/vr/openxr/openxr_scene_bounds.h"
#include "device/vr/openxr/openxr_util.h"

namespace device {

// C++ wrapper for XrSceneObserverMSFT
class OpenXrSceneObserver {
 public:
  OpenXrSceneObserver(const device::OpenXrExtensionHelper& extensions,
                      XrSession session);
  ~OpenXrSceneObserver();

  XrResult ComputeNewScene(
      const std::vector<XrSceneComputeFeatureMSFT>& requested_features,
      const OpenXrSceneBounds& bounds);

  XrSceneComputeStateMSFT GetSceneComputeState() const;

  bool IsSceneComputeCompleted() const;

  std::unique_ptr<OpenXrScene> CreateScene() const;

  XrSceneObserverMSFT Handle() const { return scene_observer_.get(); }

 private:
  const raw_ref<const device::OpenXrExtensionHelper> extensions_;
  OpenXrExtensionHandle<XrSceneObserverMSFT> scene_observer_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_SCENE_OBSERVER_H_
