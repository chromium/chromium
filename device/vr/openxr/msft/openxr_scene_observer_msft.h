// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_OBSERVER_MSFT_H_
#define DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_OBSERVER_MSFT_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "device/vr/openxr/openxr_extension_handle.h"
#include "device/vr/openxr/msft/openxr_scene_msft.h"
#include "device/vr/openxr/msft/openxr_scene_bounds_msft.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

// C++ wrapper for XrSceneObserverMSFT
class OpenXrSceneObserverMsft {
 public:
  OpenXrSceneObserverMsft(const device::OpenXrExtensionHelper& extensions,
                      XrSession session);
  ~OpenXrSceneObserverMsft();

  XrResult ComputeNewScene(
      base::span<const XrSceneComputeFeatureMSFT> requested_features,
      const OpenXrSceneBoundsMsft& bounds);

  XrSceneComputeStateMSFT GetSceneComputeState() const;

  bool IsSceneComputeCompleted() const;

  std::unique_ptr<OpenXrSceneMsft> CreateScene() const;

  XrSceneObserverMSFT Handle() const { return scene_observer_.get(); }

 private:
  const raw_ref<const device::OpenXrExtensionHelper> extensions_;
  OpenXrExtensionHandle<XrSceneObserverMSFT> scene_observer_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_MSFT_OPENXR_SCENE_OBSERVER_MSFT_H_
