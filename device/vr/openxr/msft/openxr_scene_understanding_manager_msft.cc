// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/msft/openxr_scene_understanding_manager_msft.h"

#include <algorithm>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/numerics/math_constants.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
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

OpenXRSceneUnderstandingManagerMSFT::~OpenXRSceneUnderstandingManagerMSFT() =
    default;

OpenXRSceneUnderstandingManagerMSFT::OpenXRSceneUnderstandingManagerMSFT(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space) {
  scene_bounds_.sphere_bounds_.push_back({{}, kScanRadius});
}

bool OpenXRSceneUnderstandingManagerMSFT::OnNewHitTestSubscription() {
  if (scene_compute_state_ == SceneComputeState::Off) {
    if (!scene_observer_) {
      scene_observer_ = std::make_unique<OpenXrSceneObserverMsft>(
          *extension_helper_, session_);
      scene_compute_state_ = SceneComputeState::Idle;
    }
  }

  return true;
}

void OpenXRSceneUnderstandingManagerMSFT::OnAllHitTestSubscriptionsRemoved() {
  // When there is no active hittest subscription, we want to clear out
  // all the cached data from the scene understanding.
  scene_observer_ = nullptr;
  scene_ = nullptr;
  planes_.clear();
  scene_compute_state_ = SceneComputeState::Off;
}

void OpenXRSceneUnderstandingManagerMSFT::OnFrameUpdate(
    XrTime predicted_display_time) {
  switch (scene_compute_state_) {
    case SceneComputeState::Off:
      // SceneComputeState can only be turned on by `OnNewHitTestSubscription`.
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

std::optional<float> OpenXRSceneUnderstandingManagerMSFT::GetRayPlaneDistance(
    const gfx::Point3F& ray_origin,
    const gfx::Vector3dF& ray_vector,
    const gfx::Point3F& plane_origin,
    const gfx::Vector3dF& plane_normal) {
  gfx::Vector3dF ray_origin_to_plane_origin_vector = plane_origin - ray_origin;
  float ray_to_plane_dot_product = gfx::DotProduct(ray_vector, plane_normal);

  if (ray_to_plane_dot_product == 0) {
    // if dot_product_1 is 0, that means the 2 vectors are normal to each other
    // so the vector is normal to the plane's normal, so it's parallel
    // to the plane and there is no intersection in this case.
    return std::nullopt;
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

std::vector<mojom::XRHitResultPtr>
OpenXRSceneUnderstandingManagerMSFT::RequestHitTest(
    const gfx::Point3F& ray_origin,
    const gfx::Vector3dF& ray_direction) {
  std::vector<std::pair<float, mojom::XRHitResultPtr>> sorted_results;
  sorted_results.reserve(planes_.size());
  for (auto& plane : planes_) {
    if (!IsPoseValid(plane.location_.flags)) {
      continue;
    }

    XrPosef plane_pose = plane.location_.pose;
    gfx::Point3F plane_origin = gfx::Point3F(
        plane_pose.position.x, plane_pose.position.y, plane_pose.position.z);
    gfx::Transform mojo_to_plane = XrPoseToGfxTransform(plane_pose);
    gfx::Vector3dF plane_direction_vector =
        mojo_to_plane.MapVector(gfx::Vector3dF(0, 0, -1));

    std::optional<float> distance_to_plane = GetRayPlaneDistance(
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
            gfx::Quaternion(gfx::Vector3dF(1, 0, 0), std::numbers::pi / 2);
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

  std::vector<mojom::XRHitResultPtr> hit_results;
  hit_results.reserve(sorted_results.size());

  for (auto& result : sorted_results) {
    hit_results.push_back(std::move(result.second));
  }

  DVLOG(2) << __func__ << ": hit_results->size()=" << hit_results.size();
  return hit_results;
}

OpenXrSceneUnderstandingManagerMsftFactory::
    OpenXrSceneUnderstandingManagerMsftFactory() = default;
OpenXrSceneUnderstandingManagerMsftFactory::
    ~OpenXrSceneUnderstandingManagerMsftFactory() = default;

const base::flat_set<std::string_view>&
OpenXrSceneUnderstandingManagerMsftFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_MSFT_SCENE_UNDERSTANDING_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrSceneUnderstandingManagerMsftFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::HIT_TEST};
}

std::unique_ptr<OpenXRSceneUnderstandingManager>
OpenXrSceneUnderstandingManagerMsftFactory::CreateSceneUnderstandingManager(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXRSceneUnderstandingManagerMSFT>(
        extension_helper, session, mojo_space);
  }

  return nullptr;
}
}  // namespace device
