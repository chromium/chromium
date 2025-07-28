// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/msft/openxr_hit_test_manager_msft.h"

#include <algorithm>
#include <numbers>
#include <optional>
#include <vector>

#include "device/vr/openxr/msft/openxr_plane_manager_msft.h"
#include "device/vr/openxr/openxr_util.h"

namespace device {

OpenXrHitTestManagerMsft::OpenXrHitTestManagerMsft(
    OpenXrPlaneManagerMsft* plane_manager,
    XrSpace mojo_space)
    : plane_manager_(plane_manager), mojo_space_(mojo_space) {}

OpenXrHitTestManagerMsft::~OpenXrHitTestManagerMsft() = default;

std::optional<float> OpenXrHitTestManagerMsft::GetRayPlaneDistance(
    const gfx::Point3F& ray_origin,
    const gfx::Vector3dF& ray_vector,
    const gfx::Point3F& plane_origin,
    const gfx::Vector3dF& plane_normal) {
  gfx::Vector3dF ray_origin_to_plane_origin_vector = plane_origin - ray_origin;
  float ray_to_plane_dot_product = gfx::DotProduct(ray_vector, plane_normal);

  if (ray_to_plane_dot_product == 0) {
    // If dot_product_1 is 0, that means the 2 vectors are normal to each other
    // so the vector is normal to the plane's normal, so it's parallel to the
    // plane and there is no intersection in this case.
    return std::nullopt;
  }

  float full_ray_to_plane_dot_product =
      gfx::DotProduct(ray_origin_to_plane_origin_vector, plane_normal);

  // |ray_to_plane_dot_product| and |full_ray_to_plane_dot_product| would be the
  // same if the ray_vector touches the plane. Therefore if we use the ratio
  // between them, we would have the same ratio between ray_vector and the
  // actual vector that touches the plane. We then return that ratio as the
  // distance to the plane.
  float distance = full_ray_to_plane_dot_product / ray_to_plane_dot_product;
  return distance;
}

std::vector<mojom::XRHitResultPtr> OpenXrHitTestManagerMsft::RequestHitTest(
    const gfx::Point3F& ray_origin,
    const gfx::Vector3dF& ray_direction) {
  if (!plane_manager_) {
    return {};
  }

  const auto& planes = plane_manager_->GetPlanes();
  std::vector<std::pair<float, mojom::XRHitResultPtr>> sorted_results;
  sorted_results.reserve(planes.size());
  for (auto& plane : planes) {
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
      // XrScenePlaneMSFT does provide the triangle mesh for the plane but for
      // performance reasons, we are using the bounding box (size) for hit tests
      // instead of the triangle mesh.
      if (hitpoint_in_plane_space.x() <= plane.size_.width / 2 &&
          hitpoint_in_plane_space.x() >= -(plane.size_.width / 2) &&
          hitpoint_in_plane_space.y() <= plane.size_.height / 2 &&
          hitpoint_in_plane_space.y() >= -(plane.size_.height / 2)) {
        mojom::XRHitResultPtr mojo_hit = mojom::XRHitResult::New();
        gfx::Quaternion plane_direction_openxr(
            plane_pose.orientation.x, plane_pose.orientation.y,
            plane_pose.orientation.z, plane_pose.orientation.w);
        // OpenXR's plane convention has the Z-axis as normal. However, WebXR
        // specs have the plane with Y-axis as normal. Thus we need to rotate
        // the plane direction by pi/2 around X-axis before returning it to
        // blink.
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

void OpenXrHitTestManagerMsft::OnStartProcessingHitTests(
    XrTime predicted_display_time) {
  // Ensure that the PlaneManager is updated so that we can get the latest
  // state for hit tests.
  plane_manager_->EnsureFrameUpdated(predicted_display_time, mojo_space_);
}

bool OpenXrHitTestManagerMsft::OnNewHitTestSubscription() {
  plane_manager_->Start();
  return true;
}

void OpenXrHitTestManagerMsft::OnAllHitTestSubscriptionsRemoved() {
  plane_manager_->Stop();
}

}  // namespace device
