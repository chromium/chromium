// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/msft/openxr_plane_manager_msft.h"

#include <algorithm>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace {
// - UpdateInterval is the idle time between triggering a scene-compute query
// - ScanRadius is the spherical radius in which the scene-compute query
//   use to limit the scene compute.
// A radius of 5 meters is commonly used range in scene understanding apps on
// Hololens, that are known to be stable to have a good framerate experience.
// 5 meters is also the upper limit to have a optimal depth accuracy on ARCORE.
// Usually it's up to the app to trigger the new scene-compute query. But since
// the WebXR api does not expose the api to trigger the new scene-compute query,
// the platform defaults the UpdateInterval to 5 seconds which is reasonable
// for a 5 meters radius.
constexpr XrDuration kUpdateInterval =
    5LL * 1000 * 1000 * 1000;     // 5 Seconds in Nanoseconds
constexpr float kScanRadius = 5;  // 5 meters
}  // namespace

namespace device {

OpenXrPlaneManagerMsft::OpenXrPlaneManagerMsft(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session)
    : extension_helper_(extension_helper), session_(session) {
  scene_bounds_.sphere_bounds_.push_back({{}, kScanRadius});
}

OpenXrPlaneManagerMsft::~OpenXrPlaneManagerMsft() = default;

void OpenXrPlaneManagerMsft::Start() {
  if (!scene_observer_) {
    scene_observer_ =
        std::make_unique<OpenXrSceneObserverMsft>(*extension_helper_, session_);
    scene_compute_state_ = SceneComputeState::kIdle;
  }
}

void OpenXrPlaneManagerMsft::Stop() {
  scene_observer_ = nullptr;
  scene_ = nullptr;
  planes_.clear();
  scene_compute_state_ = SceneComputeState::kOff;
}

void OpenXrPlaneManagerMsft::EnsureFrameUpdated(XrTime predicted_display_time,
                                                XrSpace mojo_space) {
  if (last_predicted_display_time_ == predicted_display_time) {
    return;
  }
  last_predicted_display_time_ = predicted_display_time;

  switch (scene_compute_state_) {
    case SceneComputeState::kOff:
      // Start/Stop are the only way to start the SceneObserver.
      return;
    case SceneComputeState::kIdle:
      if (predicted_display_time > next_scene_update_time_) {
        DCHECK(scene_observer_);
        scene_bounds_.space_ = mojo_space;
        scene_bounds_.time_ = predicted_display_time;
        static constexpr XrSceneComputeFeatureMSFT kSceneFeatures[] = {
            XR_SCENE_COMPUTE_FEATURE_PLANE_MSFT};
        if (XR_SUCCEEDED(scene_observer_->ComputeNewScene(kSceneFeatures,
                                                          scene_bounds_))) {
          scene_compute_state_ = SceneComputeState::kWaiting;
        }
        next_scene_update_time_ = predicted_display_time + kUpdateInterval;
      }
      break;
    case SceneComputeState::kWaiting:
      if (scene_observer_->IsSceneComputeCompleted()) {
        DCHECK(scene_observer_);
        scene_ = scene_observer_->CreateScene();
        scene_compute_state_ = SceneComputeState::kIdle;

        // After getting a new scene, we want to cache the planes and ids.
        planes_.clear();
        if (XR_FAILED(scene_->GetPlanes(planes_))) {
          // If GetPlanes fails, we want to clear out the scene to avoid
          // further operations and wait for a new scene to try again.
          scene_ = nullptr;
        }
      }
      break;
  }

  if (scene_) {
    // If there is an active scene_, always update the location of the objects.
    if (XR_FAILED(scene_->LocateObjects(mojo_space, predicted_display_time,
                                        planes_))) {
      // If there is a tracking loss for any reason, we should clear out the
      // cached planes.
      planes_.clear();
    }
  }
}

const std::vector<OpenXrScenePlaneMsft>& OpenXrPlaneManagerMsft::GetPlanes()
    const {
  return planes_;
}

}  // namespace device
