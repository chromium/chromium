// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_OPENXR_SCENE_UNDERSTANDING_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_SCENE_UNDERSTANDING_MANAGER_H_

#include <map>
#include "base/memory/raw_ref.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/math_constants.h"
#include "device/vr/openxr/openxr_scene_observer.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/util/hit_test_subscription_data.h"

namespace device {

class OpenXRSceneUnderstandingManager {
 public:
  OpenXRSceneUnderstandingManager(const OpenXrExtensionHelper& extension_helper,
                                  XrSession session,
                                  XrSpace mojo_space);
  ~OpenXRSceneUnderstandingManager();

  HitTestSubscriptionId SubscribeToHitTest(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray);

  HitTestSubscriptionId SubscribeToHitTestForTransientInput(
      const std::string& profile_name,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray);

  device::mojom::XRHitTestSubscriptionResultDataPtr
  GetHitTestSubscriptionResult(HitTestSubscriptionId id,
                               const mojom::XRRay& native_origin_ray,
                               const gfx::Transform& mojo_from_native_origin);

  mojom::XRHitTestSubscriptionResultsDataPtr ProcessHitTestResultsForFrame(
      XrTime predicted_display_time,
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);

  device::mojom::XRHitTestTransientInputSubscriptionResultDataPtr
  GetTransientHitTestSubscriptionResult(
      HitTestSubscriptionId id,
      const mojom::XRRay& ray,
      const std::vector<std::pair<uint32_t, gfx::Transform>>&
          input_source_ids_and_transforms);

  void UnsubscribeFromHitTest(HitTestSubscriptionId subscription_id);
  void OnFrameUpdate(XrTime predicted_display_time);

 private:
  // Enable the scene understanding computing query, the state of the query
  // is tracked by SceneComputeState. And the OnFrameUpdate() is leveraged to
  // move the state machines along.
  void EnableSceneCompute();
  void DisableSceneCompute();

  HitTestSubscriptionId::Generator
      hittest_id_generator_;  // 0 is not a valid hittest subscription ID

  std::map<HitTestSubscriptionId, HitTestSubscriptionData>
      hit_test_subscription_id_to_data_;
  std::map<HitTestSubscriptionId, TransientInputHitTestSubscriptionData>
      hit_test_subscription_id_to_transient_hit_test_data_;

  absl::optional<gfx::Transform> GetMojoFromNativeOrigin(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);
  absl::optional<gfx::Transform> GetMojoFromReferenceSpace(
      device::mojom::XRReferenceSpaceType type,
      const gfx::Transform& mojo_from_viewer);
  absl::optional<gfx::Transform> GetMojoFromPointerInput(
      const device::mojom::XRInputSourceStatePtr& input_source_state);
  std::vector<std::pair<uint32_t, gfx::Transform>> GetMojoFromInputSources(
      const std::string& profile_name,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);
  void RequestHitTest(const gfx::Point3F& origin,
                      const gfx::Vector3dF& direction,
                      std::vector<mojom::XRHitResultPtr>* hit_results);

  absl::optional<float> GetRayPlaneDistance(const gfx::Point3F& ray_origin,
                                            const gfx::Vector3dF& ray_vector,
                                            const gfx::Point3F& plane_origin,
                                            const gfx::Vector3dF& plane_normal);

  const raw_ref<const OpenXrExtensionHelper> extension_helper_;
  XrSession session_;
  XrSpace mojo_space_;

  std::unique_ptr<OpenXrSceneObserver> scene_observer_;
  std::unique_ptr<OpenXrScene> scene_;
  OpenXrSceneBounds scene_bounds_;
  XrTime next_scene_update_time_;

  // Scene Compute State Machine:
  // - SceneComputeState::Off
  //     There is no hittest subscription and no active scene compute query.
  // - SceneComputeState::Idle
  //     There is active hittest subscription and OnFrameUpdate will try to
  //     start a new scene-compute query every kUpdateInterval (5 seconds).
  //     When a new scene-compute query is successfully submitted, we then
  //     go to SceneComputeState::Waiting state.
  // - SceneComputeState::Waiting
  //     There is an active scene compute query. We need to wait for
  //     IsSceneComputeCompleted() then we can get the scene data
  //     then immediately go back to SceneComputeState::Idle state.
  enum class SceneComputeState { Off, Idle, Waiting };
  SceneComputeState scene_compute_state_{SceneComputeState::Off};

  std::vector<OpenXrScenePlane> planes_;
};

}  // namespace device
#endif  // DEVICE_VR_OPENXR_OPENXR_SCENE_UNDERSTANDING_MANAGER_H_
