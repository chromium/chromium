// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_ray.h"
#include "third_party/blink/renderer/core/style/style_offset_rotation.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"

#include "base/notreached.h"

namespace blink {

scoped_refptr<StyleRay> StyleRay::Create(float angle,
                                         RaySize size,
                                         bool contain) {
  return base::AdoptRef(new StyleRay(angle, size, contain));
}

StyleRay::StyleRay(float angle, RaySize size, bool contain)
    : angle_(angle), size_(size), contain_(contain) {}

bool StyleRay::IsEqualAssumingSameType(const BasicShape& o) const {
  const StyleRay& other = To<StyleRay>(o);
  return angle_ == other.angle_ && size_ == other.size_ &&
         contain_ == other.contain_;
}

void StyleRay::GetPath(Path&, const gfx::RectF&, float) {
  // ComputedStyle::ApplyMotionPathTransform cannot call GetPath
  // for rays as they may have infinite length.
  NOTREACHED();
}

static float CalculatePerpendicularDistanceToBoundingBoxSide(
    const gfx::PointF& point,
    const gfx::SizeF& box_size,
    float (*comp)(std::initializer_list<float>)) {
  return comp({std::abs(point.x()), std::abs(point.x() - box_size.width()),
               std::abs(point.y()), std::abs(point.y() - box_size.height())});
}

static float CalculateDistance(const gfx::PointF& a, const gfx::PointF& b) {
  return (a - b).Length();
}

float CalculateDistanceToBoundingBoxCorner(
    const gfx::PointF& point,
    const gfx::SizeF& box_size,
    float (*comp)(std::initializer_list<float>)) {
  return comp({CalculateDistance(point, {0, 0}),
               CalculateDistance(point, {box_size.width(), 0}),
               CalculateDistance(point, {box_size.width(), box_size.height()}),
               CalculateDistance(point, {0, box_size.height()})});
}

static float CalculateDistanceToBoundingBoxSide(const gfx::PointF& point,
                                                const float angle,
                                                const gfx::SizeF& box_size) {
  if (!gfx::RectF(box_size).InclusiveContains(point)) {
    return 0;
  }
  const float theta = Deg2rad(angle);
  float cos_t = cos(theta);
  float sin_t = sin(theta);
  // We are looking for % point, let's swap signs and lines
  // so that we end up in situation like this:
  //         (0, 0) #--------------%--# (box.width, 0)
  //                |        |    /   |
  //                |        v   /    |
  //                |        |  /     |
  //                |        |t/      |
  //                |        |/       |
  //                 (point) *---h----* (box.width, point.y)
  //                |        |        |
  //                |        |        |
  // (0, box.height)#-----------------# (box.width, box.height)

  // cos_t and sin_t swapped due to the 0 angle is pointing up.
  const float vertical = cos_t >= 0 ? point.y() : box_size.height() - point.y();
  const float horizontal =
      sin_t >= 0 ? box_size.width() - point.x() : point.x();
  cos_t = abs(cos_t);
  sin_t = abs(sin_t);
  // Check what side we hit.
  if (vertical * sin_t > horizontal * cos_t) {
    return horizontal / sin_t;
  }
  return vertical / cos_t;
}

float StyleRay::CalculateRayPathLength(
    const gfx::PointF& initial_position,
    const gfx::SizeF& containing_box_size) const {
  switch (Size()) {
    case StyleRay::RaySize::kClosestSide:
      return CalculatePerpendicularDistanceToBoundingBoxSide(
          initial_position, containing_box_size, std::min);
    case StyleRay::RaySize::kFarthestSide:
      return CalculatePerpendicularDistanceToBoundingBoxSide(
          initial_position, containing_box_size, std::max);
    case StyleRay::RaySize::kClosestCorner:
      return CalculateDistanceToBoundingBoxCorner(
          initial_position, containing_box_size, std::min);
    case StyleRay::RaySize::kFarthestCorner:
      return CalculateDistanceToBoundingBoxCorner(
          initial_position, containing_box_size, std::max);
    case StyleRay::RaySize::kSides:
      return CalculateDistanceToBoundingBoxSide(initial_position, Angle(),
                                                containing_box_size);
  }
}

static void RotateVertices(std::array<gfx::PointF, 4>& vertices,
                           const float ray_angle,
                           const StyleOffsetRotation& rotate) {
  // https://drafts.fxtf.org/motion/#offset-rotate-property
  // For ray paths, the rotation implied by auto is 90 degrees less
  // than the ray’s bearing <angle>.
  float angle = rotate.angle;
  // NOTE: rotation type 'reverse' is handled during parsing and translated to
  // auto, 180deg.
  if (rotate.type == OffsetRotationType::kAuto) {
    angle += ray_angle - 90;
  }
  // Rotate ray to x-axis + rotate object by its rotate angle.
  const float angle_to_x_axis = Deg2rad(angle + 90 - ray_angle);
  const float cos_t = cos(angle_to_x_axis);
  const float sin_t = sin(angle_to_x_axis);
  for (auto& v : vertices) {
    const float vx = v.x();
    const float vy = v.y();
    v.set_x(cos_t * vx - sin_t * vy);
    v.set_y(cos_t * vy + sin_t * vx);
  }
}

float StyleRay::CalculateLength(const gfx::PointF& anchor,
                                const Length& offset_distance,
                                const StyleOffsetRotation& offset_rotate,
                                const gfx::PointF& initial_position,
                                const gfx::RectF& bounding_box,
                                const gfx::SizeF& containing_box_size) const {
  const float ray_length =
      CalculateRayPathLength(initial_position, containing_box_size);
  float float_length = FloatValueForLength(offset_distance, ray_length);
  if (!Contain()) {
    return float_length;
  }
  const float width = bounding_box.width();
  const float height = bounding_box.height();
  std::array<gfx::PointF, 4> vertices{{
      {-anchor.x(), -anchor.y()},
      {width - anchor.x(), -anchor.y()},
      {width - anchor.x(), height - anchor.y()},
      {-anchor.x(), height - anchor.y()},
  }};
  // Rotate ray to x axis;
  RotateVertices(vertices, Angle(), offset_rotate);
  float upper = std::numeric_limits<float>::max();
  float lower = std::numeric_limits<float>::lowest();
  bool should_increase = false;
  // Find path intervals that enclose the box.
  for (const auto& v : vertices) {
    const float d = ray_length * ray_length - v.y() * v.y();
    if (d < 0) {
      should_increase = true;
      break;
    }
    const float sqrt_d = sqrt(d);
    upper = std::min(upper, -v.x() + sqrt_d);
    lower = std::max(lower, -v.x() - sqrt_d);
  }
  if (!should_increase) {
    return std::max(lower, std::min(upper, float_length));
  }

  // Path length should be increased.
  // We find the smallest path length such that an offset exists
  // for all vertices to lie within the path.
  const auto comp = [](const auto& a, const auto& b) {
    return std::abs(a.y()) >= std::abs(b.y());
  };
  std::sort(vertices.begin(), vertices.end(), comp);
  float radius = std::abs(vertices[0].y());
  float_length = -vertices[0].x();
  const float eps = 1e-5;

  // Find the path length such that, for some offset,
  // vertices[i] and vertices[j] both lie within the path.
  for (size_t i = 0; i < 3; ++i) {
    for (size_t j = i + 1; j < 4; ++j) {
      const float xi = vertices[i].x();
      const float yi = vertices[i].y();
      const float xj = vertices[j].x();
      const float yj = vertices[j].y();
      const float dx = xi - xj;

      // Any path that encloses vertices[i] would also enclose vertices[j].
      if (dx * dx + yj * yj <= yi * yi + eps) {
        continue;
      }

      // If both lie on the path,
      // (offset + xi)**2 + yi**2 = (offset + xj)**2 + yj**2 = (path length)**2
      // 2 * xi * offset + xi**2 + yi**2 = 2 * xj * offset + xj**2 + yj**2
      const float new_length =
          (xj * xj + yj * yj - xi * xi - yi * yi) / dx / 2.0;
      const float x0 = xi + new_length;
      const float new_radius = sqrt(x0 * x0 + yi * yi);
      if (new_radius > radius) {
        radius = new_radius;
        float_length = new_length;
      }
    }
  }
  return float_length;
}

PointAndTangent StyleRay::PointAndNormalAtLength(float length) const {
  const float angle = Angle() - 90;
  const float rad = Deg2rad(angle);
  const float x = length * cos(rad);
  const float y = length * sin(rad);
  return {{x, y}, angle};
}

}  // namespace blink
