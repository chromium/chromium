// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_MSFT_OPENXR_PLANE_MANAGER_MSFT_H_
#define DEVICE_VR_OPENXR_MSFT_OPENXR_PLANE_MANAGER_MSFT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"
#include "device/vr/openxr/msft/openxr_scene_bounds_msft.h"
#include "device/vr/openxr/msft/openxr_scene_msft.h"
#include "device/vr/openxr/msft/openxr_scene_observer_msft.h"
#include "device/vr/openxr/msft/openxr_scene_plane_msft.h"
#include "device/vr/openxr/openxr_plane_manager.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionHelper;

class OpenXrPlaneManagerMsft : public OpenXrPlaneManager {
 public:
  OpenXrPlaneManagerMsft(const OpenXrExtensionHelper& extension_helper,
                         XrSession session);
  ~OpenXrPlaneManagerMsft() override;

  void EnsureFrameUpdated(XrTime predicted_display_time, XrSpace mojo_space);

  // Helper methods to start/stop the plane manager. Note that at present these
  // are only expected to be called by `OpenXrHitTestManagerMsft`. During a
  // full planes implementation (unlikely for this class), we should refactor
  // how these are exposed/used.
  void Start();
  void Stop();

  const std::vector<OpenXrScenePlaneMsft>& GetPlanes() const;

 private:
  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;

  std::unique_ptr<OpenXrSceneObserverMsft> scene_observer_;
  std::unique_ptr<OpenXrSceneMsft> scene_;
  OpenXrSceneBoundsMsft scene_bounds_;
  XrTime next_scene_update_time_{0};
  XrTime last_predicted_display_time_{0};

  enum class SceneComputeState { kOff, kIdle, kWaiting };
  SceneComputeState scene_compute_state_{SceneComputeState::kOff};

  std::vector<OpenXrScenePlaneMsft> planes_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_MSFT_OPENXR_PLANE_MANAGER_MSFT_H_
