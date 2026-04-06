// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_util.h"

#include <string>

#include "base/check_op.h"
#include "base/numerics/angle_conversions.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
namespace device {

namespace {
// This represents a 90 degree (or pi/2) rotation about the X axis. Suitable
// for turning "+Z" from being up to "+Y" being up.
// clang-format off
static constexpr gfx::Transform kZNormalToYNormalTransform =
  gfx::Transform::RowMajor(1,  0,  0, 0,
                            0,  0, -1, 0,
                            0,  1,  0, 0,
                            0,  0,  0, 1);

float Cross2D(const mojom::XRPlanePointDataPtr& a,
              const mojom::XRPlanePointDataPtr& b,
              const mojom::XRPlanePointDataPtr& c) {
  return (b->x - a->x) * (c->z - a->z) - (b->z - a->z) * (c->x - a->x);
}
// clang-format on
}  // namespace

XrPosef PoseIdentity() {
  XrPosef pose{};
  pose.orientation.w = 1;
  return pose;
}

gfx::Transform XrPoseToGfxTransform(const XrPosef& pose) {
  gfx::DecomposedTransform decomp;
  decomp.quaternion = gfx::Quaternion(pose.orientation.x, pose.orientation.y,
                                      pose.orientation.z, pose.orientation.w);
  decomp.translate[0] = pose.position.x;
  decomp.translate[1] = pose.position.y;
  decomp.translate[2] = pose.position.z;

  return gfx::Transform::Compose(decomp);
}

device::Pose XrPoseToDevicePose(const XrPosef& pose) {
  gfx::Quaternion orientation{pose.orientation.x, pose.orientation.y,
                              pose.orientation.z, pose.orientation.w};
  gfx::Point3F position{pose.position.x, pose.position.y, pose.position.z};
  return device::Pose{position, orientation};
}

device::Pose ZNormalXrPoseToYNormalDevicePose(const XrPosef& pose) {
  auto z_normal = XrPoseToGfxTransform(pose);
  auto y_normal = z_normal * kZNormalToYNormalTransform;
  auto maybe_pose = device::Pose::Create(y_normal);

  // Our XrPose is guaranteed parseable, and applying a simple rotation should
  // not change that.
  CHECK(maybe_pose);
  return *maybe_pose;
}

gfx::Point3F ZNormalPositionToYNormalPosition(const gfx::Point3F& point) {
  return kZNormalToYNormalTransform.MapPoint(point);
}

XrPosef GfxTransformToXrPose(const gfx::Transform& transform) {
  std::optional<gfx::DecomposedTransform> decomposed_transform =
      transform.Decompose();
  // This pose should always be a simple translation and rotation so this should
  // always be true
  DCHECK(decomposed_transform);
  return {GfxQuaternionToXrQuaternion(decomposed_transform->quaternion),
          {static_cast<float>(decomposed_transform->translate[0]),
           static_cast<float>(decomposed_transform->translate[1]),
           static_cast<float>(decomposed_transform->translate[2])}};
}

XrQuaternionf GfxQuaternionToXrQuaternion(const gfx::Quaternion& quaternion) {
  return {
      .x = static_cast<float>(quaternion.x()),
      .y = static_cast<float>(quaternion.y()),
      .z = static_cast<float>(quaternion.z()),
      .w = static_cast<float>(quaternion.w()),
  };
}

mojom::VRFieldOfViewPtr XrFovToMojomFov(const XrFovf& xr_fov) {
  auto field_of_view = mojom::VRFieldOfView::New();
  field_of_view->up_degrees = base::RadToDeg(xr_fov.angleUp);
  field_of_view->down_degrees = base::RadToDeg(-xr_fov.angleDown);
  field_of_view->left_degrees = base::RadToDeg(-xr_fov.angleLeft);
  field_of_view->right_degrees = base::RadToDeg(xr_fov.angleRight);

  return field_of_view;
}

bool IsPoseValid(XrSpaceLocationFlags locationFlags) {
  XrSpaceLocationFlags PoseValidFlags = XR_SPACE_LOCATION_POSITION_VALID_BIT |
                                        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
  return (locationFlags & PoseValidFlags) == PoseValidFlags;
}

bool IsArOnlyFeature(device::mojom::XRSessionFeature feature) {
  switch (feature) {
    case device::mojom::XRSessionFeature::REF_SPACE_VIEWER:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL:
    case device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
    case device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED:
    case device::mojom::XRSessionFeature::LAYERS:
    case device::mojom::XRSessionFeature::HAND_INPUT:
    case device::mojom::XRSessionFeature::SECONDARY_VIEWS:
    case device::mojom::XRSessionFeature::WEBGPU:
      return false;
    case device::mojom::XRSessionFeature::DOM_OVERLAY:
    case device::mojom::XRSessionFeature::HIT_TEST:
    case device::mojom::XRSessionFeature::LIGHT_ESTIMATION:
    case device::mojom::XRSessionFeature::ANCHORS:
    case device::mojom::XRSessionFeature::CAMERA_ACCESS:
    case device::mojom::XRSessionFeature::PLANE_DETECTION:
    case device::mojom::XRSessionFeature::DEPTH:
    case device::mojom::XRSessionFeature::IMAGE_TRACKING:
    case device::mojom::XRSessionFeature::FRONT_FACING:
    case device::mojom::XRSessionFeature::MESH_DETECTION:
      return true;
  }
}

bool IsFeatureSupportedForMode(device::mojom::XRSessionFeature feature,
                               device::mojom::XRSessionMode mode) {
  // OpenXR doesn't support inline.
  CHECK_NE(mode, device::mojom::XRSessionMode::kInline);
  // If the feature is AR-only, then it's only supported if the mode is AR.
  if (IsArOnlyFeature(feature)) {
    return mode == device::mojom::XRSessionMode::kImmersiveAr;
  }

  // If the feature isn't AR-only, then it's supported.
  return true;
}

mojom::XRSemanticLabel ToMojomSemanticLabel(
    XrSpatialPlaneSemanticLabelEXT label) {
  switch (label) {
    case XR_SPATIAL_PLANE_SEMANTIC_LABEL_FLOOR_EXT:
      return mojom::XRSemanticLabel::kFloor;
    case XR_SPATIAL_PLANE_SEMANTIC_LABEL_WALL_EXT:
      return mojom::XRSemanticLabel::kWall;
    case XR_SPATIAL_PLANE_SEMANTIC_LABEL_CEILING_EXT:
      return mojom::XRSemanticLabel::kCeiling;
    case XR_SPATIAL_PLANE_SEMANTIC_LABEL_TABLE_EXT:
      return mojom::XRSemanticLabel::kTable;
    case XR_SPATIAL_PLANE_SEMANTIC_LABEL_UNCATEGORIZED_EXT:
    default:
      return mojom::XRSemanticLabel::kOther;
  }
}

mojom::XRSemanticLabel ToMojomSemanticLabel(
    XrSceneMeshSemanticLabelANDROID label) {
  switch (label) {
    case XR_SCENE_MESH_SEMANTIC_LABEL_FLOOR_ANDROID:
      return mojom::XRSemanticLabel::kFloor;
    case XR_SCENE_MESH_SEMANTIC_LABEL_WALL_ANDROID:
      return mojom::XRSemanticLabel::kWall;
    case XR_SCENE_MESH_SEMANTIC_LABEL_CEILING_ANDROID:
      return mojom::XRSemanticLabel::kCeiling;
    case XR_SCENE_MESH_SEMANTIC_LABEL_TABLE_ANDROID:
      return mojom::XRSemanticLabel::kTable;
    case XR_SCENE_MESH_SEMANTIC_LABEL_OTHER_ANDROID:
    default:
      return mojom::XRSemanticLabel::kOther;
  }
}

bool IsConvexPolygon(
    const std::vector<mojom::XRPlanePointDataPtr>& polygon) {
  const size_t n = polygon.size();
  if (n < 3) {
    return false;
  }
  bool has_positive = false;
  bool has_negative = false;
  for (size_t i = 0; i < n; ++i) {
    float cross =
        Cross2D(polygon[i], polygon[(i + 1) % n], polygon[(i + 2) % n]);
    if (cross > 0) {
      has_positive = true;
    } else if (cross < 0) {
      has_negative = true;
    }
    if (has_positive && has_negative) {
      return false;
    }
  }
  return true;
}

std::vector<uint32_t> EarClipTriangulate(
    const std::vector<mojom::XRPlanePointDataPtr>& polygon) {
  std::vector<uint32_t> indices;
  const size_t n = polygon.size();
  if (n < 3) {
    return indices;
  }

  std::vector<uint32_t> remaining;
  remaining.reserve(n);
  for (uint32_t i = 0; i < n; ++i) {
    remaining.push_back(i);
  }

  // Determine winding: if total signed area is negative, polygon is clockwise.
  float area = 0;
  for (size_t i = 0; i < n; ++i) {
    const auto& cur = polygon[i];
    const auto& next = polygon[(i + 1) % n];
    area += cur->x * next->z - next->x * cur->z;
  }
  const bool ccw = area > 0;

  size_t iterations = 0;
  const size_t max_iterations = remaining.size() * remaining.size();
  while (remaining.size() > 2 && iterations < max_iterations) {
    bool ear_found = false;
    const size_t m = remaining.size();
    for (size_t i = 0; i < m; ++i) {
      uint32_t prev_idx = remaining[(i + m - 1) % m];
      uint32_t cur_idx = remaining[i];
      uint32_t next_idx = remaining[(i + 1) % m];

      float cross =
          Cross2D(polygon[prev_idx], polygon[cur_idx], polygon[next_idx]);
      bool is_convex_vertex = ccw ? (cross > 0) : (cross < 0);
      if (!is_convex_vertex) {
        continue;
      }

      // Check that no other remaining vertex lies inside this triangle.
      bool contains_point = false;
      for (size_t j = 0; j < m; ++j) {
        uint32_t test_idx = remaining[j];
        if (test_idx == prev_idx || test_idx == cur_idx ||
            test_idx == next_idx) {
          continue;
        }
        float d1 =
            Cross2D(polygon[prev_idx], polygon[cur_idx], polygon[test_idx]);
        float d2 =
            Cross2D(polygon[cur_idx], polygon[next_idx], polygon[test_idx]);
        float d3 =
            Cross2D(polygon[next_idx], polygon[prev_idx], polygon[test_idx]);
        bool all_same_sign = ccw ? (d1 > 0 && d2 > 0 && d3 > 0)
                                 : (d1 < 0 && d2 < 0 && d3 < 0);
        if (all_same_sign) {
          contains_point = true;
          break;
        }
      }

      if (!contains_point) {
        indices.push_back(prev_idx);
        indices.push_back(cur_idx);
        indices.push_back(next_idx);
        remaining.erase(remaining.begin() + i);
        ear_found = true;
        break;
      }
    }
    ++iterations;
    if (!ear_found) {
      break;
    }
  }
  return indices;
}

}  // namespace device
