// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_OPENXR_OPENXR_SCENE_UNDERSTANDING_MANAGER_H_
#define DEVICE_VR_OPENXR_OPENXR_SCENE_UNDERSTANDING_MANAGER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/util/hit_test_subscription_data.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace gfx {
class Point3F;
class Transform;
class Vector3dF;
}  // namespace gfx

namespace device {

// The OpenXRSceneUnderstandingManager is responsible for managing both hit
// tests and the things that we attempt to hit test against (e.g. Planes). None
// of the extensions we currently support manage subscriptions themselves, so
// this base class abstracts the work of managing the subscriptions and
// requesting for updates for each of the subscriptions and aggregating them
// into the single return value. Callers are expected to call `OnFrameUpdate`
// before `GetHitTestResults` if they wanted updated values.
class OpenXRSceneUnderstandingManager {
 public:
  OpenXRSceneUnderstandingManager();
  virtual ~OpenXRSceneUnderstandingManager();

  std::optional<HitTestSubscriptionId> SubscribeToHitTest(
      mojom::XRNativeOriginInformationPtr native_origin_information,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray);

  std::optional<HitTestSubscriptionId> SubscribeToHitTestForTransientInput(
      const std::string& profile_name,
      const std::vector<mojom::EntityTypeForHitTest>& entity_types,
      mojom::XRRayPtr ray);

  void UnsubscribeFromHitTest(HitTestSubscriptionId subscription_id);

  // Gets HitTestResults as of the last prediction passed to `OnFrameUpdate`.
  mojom::XRHitTestSubscriptionResultsDataPtr GetHitTestResults(
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);

  virtual void OnFrameUpdate(XrTime predicted_display_time) = 0;

 protected:
  // Can return false to indicate that there was an error with processing the
  // new subscription and that it is not actually registered.
  virtual bool OnNewHitTestSubscription() = 0;
  virtual void OnAllHitTestSubscriptionsRemoved() = 0;

  // Called to get hit test results in the mojom space from the specified origin
  // and in the specified direction. Results should be appended to the end of
  // |hit_results|, which should not be cleared.
  virtual std::vector<mojom::XRHitResultPtr> RequestHitTest(
      const gfx::Point3F& origin,
      const gfx::Vector3dF& direction) = 0;

 private:
  // Transform helpers
  std::optional<gfx::Transform> GetMojoFromNativeOrigin(
      const mojom::XRNativeOriginInformation& native_origin_information,
      const gfx::Transform& mojo_from_viewer,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);
  std::optional<gfx::Transform> GetMojoFromReferenceSpace(
      device::mojom::XRReferenceSpaceType type,
      const gfx::Transform& mojo_from_viewer);
  std::optional<gfx::Transform> GetMojoFromPointerInput(
      const device::mojom::XRInputSourceStatePtr& input_source_state);
  std::vector<std::pair<uint32_t, gfx::Transform>> GetMojoFromInputSources(
      const std::string& profile_name,
      const std::vector<mojom::XRInputSourceStatePtr>& input_state);

  // Hit Test Helpers
  device::mojom::XRHitTestSubscriptionResultDataPtr
  GetHitTestSubscriptionResult(HitTestSubscriptionId id,
                               const mojom::XRRay& native_origin_ray,
                               const gfx::Transform& mojo_from_native_origin);
  device::mojom::XRHitTestTransientInputSubscriptionResultDataPtr
  GetTransientHitTestSubscriptionResult(
      HitTestSubscriptionId id,
      const mojom::XRRay& ray,
      const std::vector<std::pair<uint32_t, gfx::Transform>>&
          input_source_ids_and_transforms);

  HitTestSubscriptionId::Generator
      hittest_id_generator_;  // 0 is not a valid hittest subscription ID

  std::map<HitTestSubscriptionId, HitTestSubscriptionData>
      hit_test_subscription_id_to_data_;
  std::map<HitTestSubscriptionId, TransientInputHitTestSubscriptionData>
      hit_test_subscription_id_to_transient_hit_test_data_;
};

}  // namespace device
#endif  // DEVICE_VR_OPENXR_OPENXR_SCENE_UNDERSTANDING_MANAGER_H_
