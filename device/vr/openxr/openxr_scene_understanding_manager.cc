// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_scene_understanding_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

OpenXRSceneUnderstandingManager::~OpenXRSceneUnderstandingManager() = default;

OpenXRSceneUnderstandingManager::OpenXRSceneUnderstandingManager(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space) {
  scene_bounds_.sphere_bounds_.push_back({{}, kScanRadius});
}

void OpenXRSceneUnderstandingManager::EnableSceneCompute() {
  if (scene_compute_state_ == SceneComputeState::Off) {
    if (!scene_observer_) {
      scene_observer_ =
          std::make_unique<OpenXrSceneObserver>(*extension_helper_, session_);
      scene_compute_state_ = SceneComputeState::Idle;
    }
  }
}

void OpenXRSceneUnderstandingManager::DisableSceneCompute() {
  // When there is no active hittest subscription, we want to clear out
  // all the cached data from the scene understanding.
  scene_observer_ = nullptr;
  scene_ = nullptr;
  planes_.clear();
  scene_compute_state_ = SceneComputeState::Off;
}

void OpenXRSceneUnderstandingManager::OnFrameUpdate(
    XrTime predicted_display_time) {
  switch (scene_compute_state_) {
    case SceneComputeState::Off:
      // SceneComputeState can only be turned on by EnableSceneCompute
      break;
    case SceneComputeState::Idle:
      if (predicted_display_time > next_scene_update_time_) {
        DCHECK(scene_observer_);
        scene_bounds_.space_ = mojo_space_;
        scene_bounds_.time_ = predicted_display_time;
        static constexpr XrSceneComputeFeatureMSFT kSceneFeatures[] = {
            XR_SCENE_COMPUTE_FEATURE_PLANE_MSFT};
        if (XR_SUCCEEDED(scene_observer_->ComputeNewScene(kSceneFeatures,
                                                          scene_bounds_))) {
          scene_compute_state_ = SceneComputeState::Waiting;
        }
        next_scene_update_time_ = predicted_display_time + kUpdateInterval;
      }
      break;
    case SceneComputeState::Waiting:
      if (scene_observer_->IsSceneComputeCompleted()) {
        DCHECK(scene_observer_);
        scene_ = scene_observer_->CreateScene();
        scene_compute_state_ = SceneComputeState::Idle;

        // After getting a new scene, we want to cache the planes and its id.
        planes_.clear();
        if (XR_FAILED(scene_->GetPlanes(planes_))) {
          // If GetPlanes fails, we want to clear out the scene_ to avoid
          // further operations, and wait for a new scene_ to try again.
          scene_ = nullptr;
        }
      }
      break;
  }

  if (scene_) {
    // If there is an active scene_, always update the location of the objects
    XrResult locate_object_result =
        scene_->LocateObjects(mojo_space_, predicted_display_time, planes_);
    if (XR_FAILED(locate_object_result)) {
      // If there is a tracking loss for any reason, we should clear out the
      // cached planes_
      planes_.clear();
    }
  }
}

absl::optional<float> OpenXRSceneUnderstandingManager::GetRayPlaneDistance(
    const gfx::Point3F& ray_origin,
    const gfx::Vector3dF& ray_vector,
    const gfx::Point3F& plane_origin,
    const gfx::Vector3dF& plane_normal) {
  gfx::Vector3dF ray_origin_to_plane_origin_vector = plane_origin - ray_origin;
  float ray_to_plane_dot_product = gfx::DotProduct(ray_vector, plane_normal);

  if (ray_to_plane_dot_product == 0) {
    // if dot_product_1 is 0, that means the 2 vectors are normal to each other
    // so the vector is normal to the plane's normal, so it's parallel
    // to the plane and there is no intesection in this case.
    return absl::nullopt;
    ;
  }

  float full_ray_to_plane_dot_product =
      gfx::DotProduct(ray_origin_to_plane_origin_vector, plane_normal);

  // ray_to_plane_dot_product and full_ray_to_plane_dot_product would be
  // the same if the ray_vector touches the plane. Therefore if we use
  // the ratio between them, we would have the same ratio between ray_vector
  // and the actual vector that touches the plane.
  // We then return that ratio as the distance to the plane.
  float distance = full_ray_to_plane_dot_product / ray_to_plane_dot_product;
  return distance;
}

void OpenXRSceneUnderstandingManager::RequestHitTest(
    const gfx::Point3F& ray_origin,
    const gfx::Vector3dF& ray_direction,
    std::vector<mojom::XRHitResultPtr>* hit_results) {
  std::vector<std::pair<float, mojom::XRHitResultPtr>> sorted_results;
  sorted_results.reserve(planes_.size());
  for (auto& plane : planes_) {
    if (!IsPoseValid(plane.location_.flags))
      continue;

    XrPosef plane_pose = plane.location_.pose;
    gfx::Point3F plane_origin = gfx::Point3F(
        plane_pose.position.x, plane_pose.position.y, plane_pose.position.z);
    gfx::Transform mojo_to_plane = XrPoseToGfxTransform(plane_pose);
    gfx::Vector3dF plane_direction_vector =
        mojo_to_plane.MapVector(gfx::Vector3dF(0, 0, -1));

    absl::optional<float> distance_to_plane = GetRayPlaneDistance(
        ray_origin, ray_direction, plane_origin, plane_direction_vector);
    if (distance_to_plane.has_value() && distance_to_plane.value() > 0) {
      gfx::Point3F hitpoint_position =
          ray_origin +
          gfx::ScaleVector3d(ray_direction, distance_to_plane.value());
      gfx::Point3F hitpoint_in_plane_space =
          mojo_to_plane.InverseMapPoint(hitpoint_position)
              .value_or(hitpoint_position);

      // Check to make sure that the hitpoint is within the plane boundaries.
      // XrScenePlaneMSFT does provide the triangle mesh for the plane
      // but for performance reason, we are using the bounding box (size)
      // for hittesting instead of the triangle mesh.
      if (hitpoint_in_plane_space.x() <= plane.size_.width / 2 &&
          hitpoint_in_plane_space.x() >= -(plane.size_.width / 2) &&
          hitpoint_in_plane_space.y() <= plane.size_.height / 2 &&
          hitpoint_in_plane_space.y() >= -(plane.size_.height / 2)) {
        mojom::XRHitResultPtr mojo_hit = mojom::XRHitResult::New();
        gfx::Quaternion plane_direction_openxr(
            plane_pose.orientation.x, plane_pose.orientation.y,
            plane_pose.orientation.z, plane_pose.orientation.w);
        // OpenXR's plane convention has the Z-axis as normal
        // However, WebXR specs has the plane with Y-axis as normal
        // thus we need to rotate the plane direction by π/2 around X-axis
        // before returning it to blink.
        gfx::Quaternion plane_direction_webxr =
            plane_direction_openxr *
            gfx::Quaternion(gfx::Vector3dF(1, 0, 0), base::kPiDouble / 2);
        mojo_hit->mojo_from_result =
            device::Pose(hitpoint_position, plane_direction_webxr);
        DVLOG(3) << __func__ << ": adding hit test result, position="
                 << hitpoint_position.ToString()
                 << ", orientation=" << plane_direction_webxr.ToString();
        sorted_results.push_back(
            {distance_to_plane.value(), std::move(mojo_hit)});
      }
    }
  }

  std::sort(sorted_results.begin(), sorted_results.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  for (auto& result : sorted_results) {
    hit_results->push_back(std::move(result.second));
  }

  DVLOG(2) << __func__ << ": hit_results->size()=" << hit_results->size();
}

HitTestSubscriptionId OpenXRSceneUnderstandingManager::SubscribeToHitTest(
    mojom::XRNativeOriginInformationPtr native_origin_information,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray) {
  EnableSceneCompute();
  auto subscription_id = hittest_id_generator_.GenerateNextId();

  hit_test_subscription_id_to_data_.emplace(
      subscription_id,
      HitTestSubscriptionData{std::move(native_origin_information),
                              entity_types, std::move(ray)});

  return subscription_id;
}

HitTestSubscriptionId
OpenXRSceneUnderstandingManager::SubscribeToHitTestForTransientInput(
    const std::string& profile_name,
    const std::vector<mojom::EntityTypeForHitTest>& entity_types,
    mojom::XRRayPtr ray) {
  EnableSceneCompute();
  auto subscription_id = hittest_id_generator_.GenerateNextId();

  hit_test_subscription_id_to_transient_hit_test_data_.emplace(
      subscription_id, TransientInputHitTestSubscriptionData{
                           profile_name, entity_types, std::move(ray)});

  return subscription_id;
}

device::mojom::XRHitTestSubscriptionResultDataPtr
OpenXRSceneUnderstandingManager::GetHitTestSubscriptionResult(
    HitTestSubscriptionId id,
    const mojom::XRRay& native_origin_ray,
    const gfx::Transform& mojo_from_native_origin) {
  DVLOG(3) << __func__ << ": id=" << id;

  // Transform the ray according to the latest transform based on the XRSpace
  // used in hit test subscription.

  gfx::Point3F origin =
      mojo_from_native_origin.MapPoint(native_origin_ray.origin);

  gfx::Vector3dF direction =
      mojo_from_native_origin.MapVector(native_origin_ray.direction);

  std::vector<mojom::XRHitResultPtr> hit_results;
  RequestHitTest(origin, direction, &hit_results);

  return mojom::XRHitTestSubscriptionResultData::New(id.GetUnsafeValue(),
                                                     std::move(hit_results));
}

device::mojom::XRHitTestTransientInputSubscriptionResultDataPtr
OpenXRSceneUnderstandingManager::GetTransientHitTestSubscriptionResult(
    HitTestSubscriptionId id,
    const mojom::XRRay& input_source_ray,
    const std::vector<std::pair<uint32_t, gfx::Transform>>&
        input_source_ids_and_mojo_from_input_sources) {
  auto result =
      device::mojom::XRHitTestTransientInputSubscriptionResultData::New();

  result->subscription_id = id.GetUnsafeValue();

  for (const auto& input_source_id_and_mojo_from_input_source :
       input_source_ids_and_mojo_from_input_sources) {
    gfx::Point3F origin =
        input_source_id_and_mojo_from_input_source.second.MapPoint(
            input_source_ray.origin);

    gfx::Vector3dF direction =
        input_source_id_and_mojo_from_input_source.second.MapVector(
            input_source_ray.direction);

    std::vector<mojom::XRHitResultPtr> hit_results;
    RequestHitTest(origin, direction, &hit_results);

    result->input_source_id_to_hit_test_results.insert(
        {input_source_id_and_mojo_from_input_source.first,
         std::move(hit_results)});
  }

  return result;
}

mojom::XRHitTestSubscriptionResultsDataPtr
OpenXRSceneUnderstandingManager::ProcessHitTestResultsForFrame(
    XrTime predicted_display_time,
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  OnFrameUpdate(predicted_display_time);
  mojom::XRHitTestSubscriptionResultsDataPtr result =
      mojom::XRHitTestSubscriptionResultsData::New();

  DVLOG(3) << __func__
           << ": calculating hit test subscription results, "
              "hit_test_subscription_id_to_data_.size()="
           << hit_test_subscription_id_to_data_.size();

  for (auto& subscription_id_and_data : hit_test_subscription_id_to_data_) {
    // First, check if we can find the current transformation for a ray. If not,
    // skip processing this subscription.
    auto maybe_mojo_from_native_origin = GetMojoFromNativeOrigin(
        *subscription_id_and_data.second.native_origin_information,
        mojo_from_viewer, input_state);

    if (!maybe_mojo_from_native_origin) {
      continue;
    }

    // Since we have a transform, let's use it to obtain hit test results.
    result->results.push_back(GetHitTestSubscriptionResult(
        HitTestSubscriptionId(subscription_id_and_data.first),
        *subscription_id_and_data.second.ray, *maybe_mojo_from_native_origin));
  }

  // Calculate results for transient input sources
  DVLOG(3)
      << __func__
      << ": calculating hit test subscription results for transient input, "
         "hit_test_subscription_id_to_transient_hit_test_data_.size()="
      << hit_test_subscription_id_to_transient_hit_test_data_.size();

  for (const auto& subscription_id_and_data :
       hit_test_subscription_id_to_transient_hit_test_data_) {
    auto input_source_ids_and_transforms = GetMojoFromInputSources(
        subscription_id_and_data.second.profile_name, input_state);

    result->transient_input_results.push_back(
        GetTransientHitTestSubscriptionResult(
            HitTestSubscriptionId(subscription_id_and_data.first),
            *subscription_id_and_data.second.ray,
            input_source_ids_and_transforms));
  }

  return result;
}

void OpenXRSceneUnderstandingManager::UnsubscribeFromHitTest(
    HitTestSubscriptionId subscription_id) {
  // Hit test subscription ID space is the same for transient and non-transient
  // hit test sources, so we can attempt to remove it from both collections (it
  // will succeed only for one of them anyway).
  hit_test_subscription_id_to_data_.erase(
      HitTestSubscriptionId(subscription_id));
  hit_test_subscription_id_to_transient_hit_test_data_.erase(
      HitTestSubscriptionId(subscription_id));
  if (hit_test_subscription_id_to_data_.empty() &&
      hit_test_subscription_id_to_transient_hit_test_data_.empty()) {
    DisableSceneCompute();
  }
}

absl::optional<gfx::Transform>
OpenXRSceneUnderstandingManager::GetMojoFromNativeOrigin(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& mojo_from_viewer,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  switch (native_origin_information.which()) {
    case mojom::XRNativeOriginInformation::Tag::kInputSourceSpaceInfo:
      for (auto& input_source_state : input_state) {
        mojom::XRInputSourceSpaceInfo* input_source_space_info =
            native_origin_information.get_input_source_space_info().get();
        if (input_source_state->source_id ==
            input_source_space_info->input_source_id) {
          return GetMojoFromPointerInput(input_source_state);
        }
      }
      return absl::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kReferenceSpaceType:
      return GetMojoFromReferenceSpace(
          native_origin_information.get_reference_space_type(),
          mojo_from_viewer);
    case mojom::XRNativeOriginInformation::Tag::kPlaneId:
      return absl::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kAnchorId:
      return absl::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kHandJointSpaceInfo:
      return absl::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kImageIndex:
      return absl::nullopt;
  }
}

absl::optional<gfx::Transform>
OpenXRSceneUnderstandingManager::GetMojoFromReferenceSpace(
    device::mojom::XRReferenceSpaceType type,
    const gfx::Transform& mojo_from_viewer) {
  DVLOG(3) << __func__ << ": type=" << type;

  switch (type) {
    case device::mojom::XRReferenceSpaceType::kLocal:
      return gfx::Transform{};
    case device::mojom::XRReferenceSpaceType::kLocalFloor:
      return absl::nullopt;
    case device::mojom::XRReferenceSpaceType::kViewer:
      return mojo_from_viewer;
    case device::mojom::XRReferenceSpaceType::kBoundedFloor:
      return absl::nullopt;
    case device::mojom::XRReferenceSpaceType::kUnbounded:
      return absl::nullopt;
  }
}

std::vector<std::pair<uint32_t, gfx::Transform>>
OpenXRSceneUnderstandingManager::GetMojoFromInputSources(
    const std::string& profile_name,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  std::vector<std::pair<uint32_t, gfx::Transform>> result;

  for (const auto& input_source_state : input_state) {
    if (input_source_state && input_source_state->description) {
      if (base::Contains(input_source_state->description->profiles,
                         profile_name)) {
        // Input source represented by input_state matches the profile, find
        // the transform and grab input source id.
        absl::optional<gfx::Transform> maybe_mojo_from_input_source =
            GetMojoFromPointerInput(input_source_state);

        if (!maybe_mojo_from_input_source)
          continue;

        result.push_back(
            {input_source_state->source_id, *maybe_mojo_from_input_source});
      }
    }
  }

  return result;
}

absl::optional<gfx::Transform>
OpenXRSceneUnderstandingManager::GetMojoFromPointerInput(
    const device::mojom::XRInputSourceStatePtr& input_source_state) {
  if (!input_source_state->mojo_from_input ||
      !input_source_state->description ||
      !input_source_state->description->input_from_pointer) {
    return absl::nullopt;
  }

  gfx::Transform mojo_from_input = *input_source_state->mojo_from_input;

  gfx::Transform input_from_pointer =
      *input_source_state->description->input_from_pointer;

  return mojo_from_input * input_from_pointer;
}

}  // namespace device
